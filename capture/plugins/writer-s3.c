/******************************************************************************/
/* writer-s3.c  -- S3 Writer Plugin
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "moloch.h"

extern MolochConfig_t        config;

typedef struct writer_s3_output {
    struct writer_s3_output *os3_next, *os3_prev;
    uint16_t                   os3_count;

    unsigned char             *buf;
    int                        len;
} SavepcapS3Output_t;

typedef struct writer_s3_file {
    struct writer_s3_file   *fs3_next, *fs3_prev;
    uint16_t                   fs3_count;

    char                      *outputFileName;
    struct timespec            outputFileTime;
    char                      *outputPath;
    SavepcapS3Output_t         outputQ;
    char                      *uploadId;
    int                        partNumber;
    int                        partNumberResponses;
    char                       doClose;
    char                      *partNumbers[2001];
} SavepcapS3File_t;

LOCAL  char                 *outputBuffer;
LOCAL  uint32_t              outputPos;
LOCAL  uint32_t              outputId;
LOCAL  uint64_t              outputFilePos = 0;
LOCAL  uint64_t              outputActualFilePos = 0;
LOCAL  uint64_t              outputLastBlockStart = 0;
LOCAL  uint32_t              outputOffsetInBlock = 0;
LOCAL  uint32_t              outputDataSinceLastMiniBlock = 0;
LOCAL  z_stream              z_strm;

SavepcapS3File_t            *currentFile;
LOCAL  SavepcapS3File_t      fileQ;


LOCAL  void *                s3Server = 0;
LOCAL  void *                metadataServer = 0;
LOCAL  char                  *s3Region;
LOCAL  char                  *s3Host;
LOCAL  char                  *s3Bucket;
LOCAL  char                  s3PathAccessStyle;
LOCAL  char                  *s3AccessKeyId;
LOCAL  char                  *s3SecretAccessKey;
LOCAL  char                  *s3Token;
LOCAL  time_t                 s3TokenTime;
LOCAL  char                  *s3Role;
LOCAL  char                   s3Compress;
LOCAL  char                   s3WriteGzip;
LOCAL  char                  *s3StorageClass;
LOCAL  uint32_t               s3MaxConns;
LOCAL  uint32_t               s3MaxRequests;
LOCAL  char                   s3UseHttp;
LOCAL  char                   s3UseTokenForMetadata;

LOCAL  int                    inprogress;


void writer_s3_flush(gboolean all);


// These must agree with the index.js
#define COMPRESSED_BLOCK_SIZE  100000
#define COMPRESSED_WITHIN_BLOCK_BITS  20



void writer_s3_request(char *method, char *path, char *qs, unsigned char *data, int len, gboolean specifyStorageClass, MolochHttpResponse_cb cb, gpointer uw);

LOCAL  MOLOCH_LOCK_DEFINE(output);
/******************************************************************************/
uint32_t writer_s3_queue_length()
{
    int q = 0;

    SavepcapS3File_t *file;
    DLL_FOREACH(fs3_, &fileQ, file)
    {
        if (config.debug && DLL_COUNT(os3_, &file->outputQ) > 0)
            LOG("Waiting: %s - %d", file->outputFileName, DLL_COUNT(os3_, &file->outputQ));
        q += DLL_COUNT(os3_, &file->outputQ);
    }

    if (config.debug)
        LOG("queue length: http Q:%d in progress: %d waiting:%d", moloch_http_queue_length(s3Server), inprogress, q);

    return q + moloch_http_queue_length(s3Server) + inprogress;
}
/******************************************************************************/
void writer_s3_complete_cb (int code, unsigned char *data, int len, gpointer uw)
{
    SavepcapS3File_t  *file = uw;
    inprogress--;

    if (code != 200) {
        LOG("Bad Response: %d %s %.*s", code, file->outputFileName, len, data);
    }

    if (config.debug)
        LOG("Complete-Response: %s %d %.*s", file->outputFileName, len, len, data);

    DLL_REMOVE(fs3_, &fileQ, file);
    if (file->uploadId)
        g_free(file->uploadId);
    g_free(file->outputFileName);
    MOLOCH_TYPE_FREE(SavepcapS3File_t, file);
}
/******************************************************************************/
void writer_s3_part_cb (int code, unsigned char *data, int len, gpointer uw)
{
    SavepcapS3File_t  *file = uw;

    inprogress--;

    if (code != 200) {
        LOG("Bad Response: %d %s %.*s", code, file->outputFileName, len, data);
    }

    if (config.debug > 1)
        LOG("Part-Response: %d %s partNumber=%s, uploadId=%s, %d", code, file->outputFileName,
        file->partNumber, file->uploadId, len);

    file->partNumberResponses++; // Race condition??

    if (config.debug > 2)
        LOG("CompleteMultipartUpload conditions: doClose=%B, partNumber=%d, partNumberResponses=%d", file->doClose,
        file->partNumber, file->partNumberResponses);

    if (file->doClose && file->partNumber == file->partNumberResponses) {
        char qs[1000];
        snprintf(qs, sizeof(qs), "uploadId=%s", file->uploadId);
        char *buf = moloch_http_get_buffer(1000000);
        BSB bsb;

        BSB_INIT(bsb, buf, 1000000);
        BSB_EXPORT_cstr(bsb, "<CompleteMultipartUpload>\n");
        int i;
        for (i = 1; i < file->partNumber; i++) { // <=? off by one issue?
            BSB_EXPORT_sprintf(bsb, "<Part><PartNumber>%d</PartNumber><ETag>%s</ETag></Part>\n", i, file->partNumbers[i]);
            g_free(file->partNumbers[i]);
        }
        BSB_EXPORT_cstr(bsb, "</CompleteMultipartUpload>\n");

        writer_s3_request("POST", file->outputPath, qs, (unsigned char*)buf, BSB_LENGTH(bsb), FALSE, writer_s3_complete_cb, file);
        if (config.debug > 1)
            LOG("Complete-Request: %s %.*s", file->outputFileName, (int)BSB_LENGTH(bsb), buf);
    }

}
/******************************************************************************/
unsigned char *moloch_get_instance_metadata(void *serverV, char *key, int key_len, size_t *mlen)
{
    unsigned char *token;
    char *requestHeaders[2];
    char tokenHeader[200];
    char *tokenRequestHeaders[2];
    tokenRequestHeaders[0] = "X-aws-ec2-metadata-token-ttl-seconds: 30";
    tokenRequestHeaders[1] = NULL;
    requestHeaders[1] = NULL;
    if (s3UseTokenForMetadata) {
        if (config.debug)
            LOG("Requesting IMDSv2 metadata token");
        token = moloch_http_send_sync(serverV, "PUT", "/latest/api/token", -1, NULL, 0, tokenRequestHeaders, mlen);
        if (config.debug)
            LOG("IMDSv2 metadata token received");
        snprintf(tokenHeader, sizeof(tokenHeader), "X-aws-ec2-metadata-token: %s", token);
        requestHeaders[0] = tokenHeader;
    } else {
        if (config.debug)
            LOG("Using IMDSv1");
        requestHeaders[0] = NULL;
    }
    return moloch_http_send_sync(serverV, "GET", key, key_len, NULL, 0, requestHeaders, mlen);
}
/******************************************************************************/
void writer_s3_refresh_s3credentials(void)
{
    char role_url[1000];
    size_t rlen;
    struct timeval now;

    gettimeofday(&now, 0);

    if (now.tv_sec < s3TokenTime + 290) {
        // Nothing to be done -- token is still valid
        return;
    }

    snprintf(role_url, sizeof(role_url), "/latest/meta-data/iam/security-credentials/%s", s3Role);

    unsigned char *credentials = moloch_get_instance_metadata(metadataServer, role_url, -1, &rlen);

    char *newS3AccessKeyId = NULL;
    char *newS3SecretAccessKey = NULL;
    char *newS3Token = NULL;

    if (credentials && rlen) {
        // Now need to extract access key, secret key and token
        newS3AccessKeyId = moloch_js0n_get_str(credentials, rlen, "AccessKeyId");
        newS3SecretAccessKey = moloch_js0n_get_str(credentials, rlen, "SecretAccessKey");
        newS3Token = moloch_js0n_get_str(credentials, rlen, "Token");
    }

    if (newS3AccessKeyId && newS3SecretAccessKey && newS3Token) {
        free(s3AccessKeyId);
        free(s3SecretAccessKey);
        free(s3Token);

        s3AccessKeyId = newS3AccessKeyId;
        s3SecretAccessKey = newS3SecretAccessKey;
        s3Token = newS3Token;

        s3TokenTime = now.tv_sec;
    }

    if (!s3AccessKeyId || !s3SecretAccessKey || !s3Token) {
        printf("Cannot retrieve credentials from metadata service at %s\n", role_url);
        exit(1);
    }

    free(credentials);
}
/******************************************************************************/
void writer_s3_init_cb (int code, unsigned char *data, int len, gpointer uw)
{
    SavepcapS3File_t   *file = uw;

    inprogress--;

    if (code != 200) {
        LOG("Bad Response: %d %s %.*s", code, file->outputFileName, len, data);
    }

    if (config.debug)
        LOG("Init-Response: %s %d", file->outputFileName, len);

    if (len == 0) {
        writer_s3_request("POST", file->outputPath, "uploads=", 0, 0, TRUE, writer_s3_init_cb, file);
        return;
    }

    static GRegex      *regex = 0;
    SavepcapS3Output_t *output;

    if (!regex) {
        regex = g_regex_new("<UploadId>(.*)</UploadId>", 0, 0, 0);
    }
    GMatchInfo *match_info;
    g_regex_match_full(regex, (char *)data, len, 0, 0, &match_info, NULL);
    if (g_match_info_matches(match_info)) {
        file->uploadId = g_match_info_fetch(match_info, 1);
        file->partNumber = 1;
        file->partNumberResponses = 1;
    } else {
        LOGEXIT("ERROR - Unknown s3 response: %.*s", len, data);
    }
    g_match_info_free(match_info);

    while (DLL_POP_HEAD(os3_, &file->outputQ, output)) {
        char qs[1000];
        snprintf(qs, sizeof(qs), "partNumber=%d&uploadId=%s", file->partNumber, file->uploadId);
        if (config.debug)
            LOG("Part-Request: %s %s", file->outputFileName, qs);
        file->partNumber++;
        writer_s3_request("PUT", file->outputPath, qs, output->buf, output->len, FALSE, writer_s3_part_cb, file);
        MOLOCH_TYPE_FREE(SavepcapS3Output_t, output);
    }
}
/******************************************************************************/
void writer_s3_header_cb (char *url, const char *field, const char *value, int valueLen, gpointer uw)
{

    if (strcasecmp("etag", field) != 0)
        return;

    char *pnstr = strstr(url, "partNumber=");
    if (!pnstr)
        return;

    SavepcapS3File_t   *file = uw;
    int pn = atoi(pnstr + 11);

    if (*value == '"')
        file->partNumbers[pn] = g_strndup(value+1, valueLen-2);
    else
        file->partNumbers[pn] = g_strndup(value, valueLen);

    if (config.debug)
        LOG("Part-Etag: %s %d", file->outputFileName, pn);
}
/******************************************************************************/
GChecksum *checksum;
void writer_s3_request(char *method, char *path, char *qs, unsigned char *data, int len, gboolean specifyStorageClass, MolochHttpResponse_cb cb, gpointer uw)
{
    char           canonicalRequest[20000];
    char           datetime[17];
    char           objectkey[1000];
    char           fullpath[2000];
    char           bodyHash[65];
    char           storageClassHeader[1000];
    char           tokenHeader[4200];
    struct timeval outputFileTime;

    gettimeofday(&outputFileTime, 0);
    struct tm      gm;
    gmtime_r(&outputFileTime.tv_sec, &gm);
    snprintf(datetime, sizeof(datetime),
            "%04u%02u%02uT%02u%02u%02uZ",
            gm.tm_year + 1900,
            gm.tm_mon+1,
            gm.tm_mday,
            gm.tm_hour,
            gm.tm_min,
            gm.tm_sec);


    snprintf(storageClassHeader, sizeof(storageClassHeader), "x-amz-storage-class:%s\n", s3StorageClass);
    if (s3Token) {
      writer_s3_refresh_s3credentials();
      snprintf(tokenHeader, sizeof(tokenHeader), "x-amz-security-token:%s\n", s3Token);
    }

    g_checksum_reset(checksum);
    g_checksum_update(checksum, data, len);
    g_strlcpy(bodyHash, g_checksum_get_string(checksum), sizeof(bodyHash));

    if (s3PathAccessStyle)
        snprintf(objectkey, sizeof(objectkey), "/%s%s", s3Bucket, path);
    else
        snprintf(objectkey, sizeof(objectkey), "%s", path);

    snprintf(canonicalRequest, sizeof(canonicalRequest),
            "%s\n"       // HTTPRequestMethod
            "%s\n"       // CanonicalURI
            "%s\n"       // CanonicalQueryString
            //CanonicalHeaders
            "host:%s\n"
            "x-amz-content-sha256:%s\n"
            "x-amz-date:%s\n"
            "%s"
            "%s"
            "\n"
            // SignedHeaders
            "host;x-amz-content-sha256;x-amz-date%s%s\n"
            "%s"     // HexEncode(Hash(RequestPayload))
            ,
            method,
            objectkey,
            qs,
            s3Host,
            bodyHash,
            datetime,
            (s3Token?tokenHeader:""),
            (specifyStorageClass?storageClassHeader:""),
            (s3Token?";x-amz-security-token":""),
            (specifyStorageClass?";x-amz-storage-class":""),
            bodyHash);
    if (config.debug >= 2)
        LOG("canonicalRequest: %s", canonicalRequest);

    g_checksum_reset(checksum);
    g_checksum_update(checksum, (guchar*)canonicalRequest, -1);

    char stringToSign[1000];
    snprintf(stringToSign, sizeof(stringToSign),
             "AWS4-HMAC-SHA256\n"
             "%s\n"
             "%8.8s/%s/s3/aws4_request\n"
             "%s"
             ,
             datetime,
             datetime,
             s3Region,
             g_checksum_get_string(checksum));
    if (config.debug >= 2)
        LOG("stringToSign: %s", stringToSign);

    char kSecret[1000];
    snprintf(kSecret, sizeof(kSecret), "AWS4%s", s3SecretAccessKey);

    char  kDate[65];
    gsize kDateLen = sizeof(kDate);
    GHmac *hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kSecret, strlen(kSecret));
    g_hmac_update(hmac, (guchar*)datetime, 8);
    g_hmac_get_digest(hmac, (guchar*)kDate, &kDateLen);
    g_hmac_unref(hmac);

    char  kRegion[65];
    gsize kRegionLen = sizeof(kRegion);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kDate, kDateLen);
    g_hmac_update(hmac, (guchar*)s3Region, -1);
    g_hmac_get_digest(hmac, (guchar*)kRegion, &kRegionLen);
    g_hmac_unref(hmac);

    char  kService[65];
    gsize kServiceLen = sizeof(kService);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kRegion, kRegionLen);
    g_hmac_update(hmac, (guchar*)"s3", 2);
    g_hmac_get_digest(hmac, (guchar*)kService, &kServiceLen);
    g_hmac_unref(hmac);

    char kSigning[65];
    gsize kSigningLen = sizeof(kSigning);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kService, kServiceLen);
    g_hmac_update(hmac, (guchar*)"aws4_request", 12);
    g_hmac_get_digest(hmac, (guchar*)kSigning, &kSigningLen);
    g_hmac_unref(hmac);

    char signature[65];
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kSigning, kSigningLen);
    g_hmac_update(hmac, (guchar*)stringToSign, -1);
    g_strlcpy(signature, g_hmac_get_string(hmac), sizeof(signature));
    g_hmac_unref(hmac);

    if (config.debug >= 2)
      LOG("signature: %s", signature);

    snprintf(fullpath, sizeof(fullpath), "%s?%s", objectkey, qs);

    char strs[3][1000];
    char *headers[8];
    headers[0] = "Expect:";
    headers[1] = "Content-Type:";
    headers[2] = strs[0];
    headers[3] = strs[1];
    headers[4] = strs[2];

    int nextHeader = 5;

    snprintf(strs[0], sizeof(strs[0]),
            "Authorization: AWS4-HMAC-SHA256 Credential=%s/%8.8s/%s/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date%s%s,Signature=%s"
            ,
            s3AccessKeyId, datetime, s3Region,
            (s3Token?";x-amz-security-token":""),
            (specifyStorageClass?";x-amz-storage-class":""),
            signature
            );

    snprintf(strs[1], sizeof(strs[1]), "x-amz-content-sha256: %s" , bodyHash);
    snprintf(strs[2], sizeof(strs[2]), "x-amz-date: %s", datetime);

    if (s3Token) {
        snprintf(tokenHeader, sizeof(tokenHeader), "x-amz-security-token: %s", s3Token);
        headers[nextHeader++] = tokenHeader;
    }

    if (specifyStorageClass) {
        // Note the missing newline in this place
        snprintf(storageClassHeader, sizeof(storageClassHeader), "x-amz-storage-class: %s", s3StorageClass);
        headers[nextHeader++] = storageClassHeader;
    }

    headers[nextHeader] = NULL;

    inprogress++;
    moloch_http_send(s3Server, method, fullpath, strlen(fullpath), (char*)data, len, headers, FALSE, cb, uw);
}
/******************************************************************************/
LOCAL void make_new_block(void) {
    if (s3WriteGzip) {
        // We need to make a new block

        while (TRUE) {
            deflate(&z_strm, Z_FULL_FLUSH);
            if (z_strm.avail_out > 0) {
                break;
            }
            writer_s3_flush(FALSE);
        }

        outputActualFilePos = z_strm.total_out;
        outputLastBlockStart = outputActualFilePos;
        outputOffsetInBlock = 0;
        outputDataSinceLastMiniBlock = 0;

        outputFilePos = (outputLastBlockStart << COMPRESSED_WITHIN_BLOCK_BITS) + outputOffsetInBlock;
    }
}
/******************************************************************************/
LOCAL void ensure_space_for_output(size_t space) {
    if (s3WriteGzip) {
        size_t max_need_space = outputActualFilePos - outputLastBlockStart + 64 + deflateBound(&z_strm, space + outputDataSinceLastMiniBlock);
        if (max_need_space >= COMPRESSED_BLOCK_SIZE) {
            // Might not fit.
            // Do a normal flush
            while (TRUE) {
                deflate(&z_strm, Z_BLOCK);
                if (z_strm.avail_out > 0) {
                    outputActualFilePos = z_strm.total_out;
                    outputDataSinceLastMiniBlock = 0;
                    break;
                }
                writer_s3_flush(FALSE);
            }
            // Recompute after the flush
            max_need_space = outputActualFilePos - outputLastBlockStart + 64 + deflateBound(&z_strm, space + outputDataSinceLastMiniBlock);
            if (max_need_space >= 3 * COMPRESSED_BLOCK_SIZE / 4) {
                make_new_block();
            }
        }
    }
}
/******************************************************************************/
LOCAL void append_to_output(void *data, size_t length, gboolean packetHeader, size_t extra_space) {
    if (s3WriteGzip) {
        // outputActualFilePos is the offset in the compressed file
        // outputLastBLockStart is the offset in the compressed file of the most recent block
        // outputFilePos is the offset in the current decompressed block plus outputLastBlockStart << COMPRESSED_WITHIN_BLOCK_BITS
        // outputOffsetInBlock is the offset within the current decompressed block
        if (outputActualFilePos == 0) {
            memset(&z_strm, 0, sizeof(z_strm));
            z_strm.next_out = (Bytef *) outputBuffer;
            z_strm.avail_out = config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN;
            z_strm.zalloc = Z_NULL;
            z_strm.zfree = Z_NULL;

            deflateInit2(&z_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + 15, 8, Z_DEFAULT_STRATEGY);
        }

        // See if we can output this
        if (packetHeader) {
            ensure_space_for_output(length + extra_space);
        }

        z_strm.next_in = data;
        z_strm.avail_in = length;

        while (z_strm.avail_in != 0) {
            if (z_strm.avail_out == 0) {
                writer_s3_flush(FALSE);
            }

            deflate(&z_strm, Z_NO_FLUSH);
            outputActualFilePos = z_strm.total_out;
        }

        outputOffsetInBlock += length;
        outputDataSinceLastMiniBlock += length;

        if (!packetHeader &&
                (outputOffsetInBlock >= (1 << COMPRESSED_WITHIN_BLOCK_BITS) - 16 ||
                outputActualFilePos > outputLastBlockStart + COMPRESSED_BLOCK_SIZE)) {
            // We need to make a new block
            make_new_block();
        }

        outputFilePos = (outputLastBlockStart << COMPRESSED_WITHIN_BLOCK_BITS) + outputOffsetInBlock;
    } else {
        memcpy(outputBuffer + outputPos, data, length);
        outputFilePos += length;
        outputActualFilePos += length;
        outputPos += length;

        if (outputPos > config.pcapWriteSize) {
            writer_s3_flush(FALSE);
        }
    }
}
/******************************************************************************/
void writer_s3_flush(gboolean all)
{
    if (!currentFile)
        return;

    if (s3WriteGzip) {
      if (all) {
        deflate(&z_strm, Z_FINISH);

        if (z_strm.avail_out == 0) {
          writer_s3_flush(FALSE);
          deflate(&z_strm, Z_FINISH);
        }
      }

      outputPos = z_strm.next_out - (Bytef *) outputBuffer;
    }

    if (currentFile->uploadId) {
        char qs[1000];

        snprintf(qs, sizeof(qs), "partNumber=%d&uploadId=%s", currentFile->partNumber, currentFile->uploadId);
        writer_s3_request("PUT", currentFile->outputPath, qs, (unsigned char *)outputBuffer, outputPos, FALSE, writer_s3_part_cb, currentFile);
        if (config.debug)
            LOG("Part-Request: %s %s", currentFile->outputFileName, qs);
        currentFile->partNumber++;
    } else {
        SavepcapS3Output_t *output = MOLOCH_TYPE_ALLOC0(SavepcapS3Output_t);
        output->buf = (unsigned char *)outputBuffer;
        output->len = outputPos;
        DLL_PUSH_TAIL(os3_, &currentFile->outputQ, output);
    }

    if (all) {
        currentFile->doClose = TRUE;
        currentFile = NULL;
    } else {
        outputBuffer = moloch_http_get_buffer(config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
        outputPos = 0;

        if (s3WriteGzip) {
          z_strm.next_out = (Bytef *) outputBuffer;
          z_strm.avail_out = config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN;
        }
    }
}
/******************************************************************************/
void writer_s3_exit()
{
    writer_s3_flush(TRUE);
}
/******************************************************************************/
extern MolochPcapFileHdr_t pcapFileHeader;
void writer_s3_create(const MolochPacket_t *packet)
{
    char               filename[1000];
    struct tm          tmp;
    int                offset = 6 + strlen(s3Region) + strlen(s3Bucket);

    localtime_r(&packet->ts.tv_sec, &tmp);
    snprintf(filename, sizeof(filename), "s3://%s/%s/%s/#NUMHEX#-%02d%02d%02d-#NUM#.pcap%s", s3Region, s3Bucket, config.nodeName, tmp.tm_year%100, tmp.tm_mon+1, tmp.tm_mday, s3WriteGzip ? ".gz" : "");

    currentFile = MOLOCH_TYPE_ALLOC0(SavepcapS3File_t);
    DLL_INIT(os3_, &currentFile->outputQ);
    DLL_PUSH_TAIL(fs3_, &fileQ, currentFile);

    currentFile->outputFileName = moloch_db_create_file(packet->ts.tv_sec, filename, 0, 0, &outputId);
    currentFile->outputPath = currentFile->outputFileName + offset;
    clock_gettime(CLOCK_REALTIME_COARSE, &currentFile->outputFileTime);
    outputFilePos = 0;
    outputActualFilePos = 0;
    outputLastBlockStart = 0;
    outputLastBlockStart = 0;
    outputDataSinceLastMiniBlock = 0;

    outputBuffer = moloch_http_get_buffer(config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
    outputPos = 0;
    uint32_t linktype = moloch_packet_dlt_to_linktype(pcapFileHeader.dlt);
    append_to_output(&pcapFileHeader, 20, FALSE, 0);
    append_to_output(&linktype, 4, FALSE, 0);
    make_new_block();                   // So we can read the header in a small amount of data fetched

    if (config.debug)
        LOG("Init-Request: %s", currentFile->outputFileName);

    writer_s3_request("POST", currentFile->outputPath, "uploads=", 0, 0, TRUE, writer_s3_init_cb, currentFile);
}

/******************************************************************************/
struct pcap_timeval {
    int32_t tv_sec;             /* seconds */
    int32_t tv_usec;            /* microseconds */
};
struct pcap_sf_pkthdr {
    struct pcap_timeval ts;     /* time stamp */
    uint32_t caplen;            /* length of portion present */
    uint32_t len;               /* length this packet (off wire) */
};
void
writer_s3_write(const MolochSession_t *const UNUSED(session), MolochPacket_t * const packet)
{
    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.len        = packet->pktlen;

    MOLOCH_LOCK(output);
    if (!currentFile) {
        writer_s3_create(packet);
    }

    packet->writerFileNum = outputId;
    packet->writerFilePos = outputFilePos;

    append_to_output(&hdr, sizeof(hdr), TRUE, packet->pktlen);

    append_to_output(packet->pkt, packet->pktlen, FALSE, 0);

    if (outputActualFilePos >= config.maxFileSizeB) {
        writer_s3_flush(TRUE);
    }
    MOLOCH_UNLOCK(output);
}

/******************************************************************************/
void writer_s3_init(char *UNUSED(name))
{
    moloch_writer_queue_length = writer_s3_queue_length;
    moloch_writer_exit         = writer_s3_exit;
    moloch_writer_write        = writer_s3_write;

    s3Region              = moloch_config_str(NULL, "s3Region", "us-east-1");
    s3Host                = moloch_config_str(NULL, "s3Host", NULL);
    s3Bucket              = moloch_config_str(NULL, "s3Bucket", NULL);
    s3PathAccessStyle     = moloch_config_boolean(NULL, "s3PathAccessStyle", strchr(s3Bucket, '.') != NULL);
    s3AccessKeyId         = moloch_config_str(NULL, "s3AccessKeyId", NULL);
    s3SecretAccessKey     = moloch_config_str(NULL, "s3SecretAccessKey", NULL);
    s3Compress            = moloch_config_boolean(NULL, "s3Compress", FALSE);
    s3WriteGzip           = moloch_config_boolean(NULL, "s3WriteGzip", FALSE);
    s3StorageClass        = moloch_config_str(NULL, "s3StorageClass", "STANDARD");
    s3MaxConns            = moloch_config_int(NULL, "s3MaxConns", 20, 5, 1000);
    s3MaxRequests         = moloch_config_int(NULL, "s3MaxRequests", 500, 10, 5000);
    s3UseHttp             = moloch_config_boolean(NULL, "s3UseHttp", FALSE);
    s3UseTokenForMetadata = moloch_config_boolean(NULL, "s3UseTokenForMetadata", TRUE);
    s3Token               = NULL;
    s3TokenTime           = 0;
    s3Role                = NULL;

    if (!s3Bucket) {
        printf("Must set s3Bucket to save to s3\n");
        exit(1);
    }

    if (!s3AccessKeyId || !s3AccessKeyId[0]) {
        // Fetch the data from the EC2 metadata service
        size_t rlen;

        metadataServer = moloch_http_create_server("http://169.254.169.254", 10, 10, 0);
        moloch_http_set_print_errors(metadataServer);

        s3AccessKeyId = 0;

        unsigned char *rolename = moloch_get_instance_metadata(metadataServer, "/latest/meta-data/iam/security-credentials/", -1, &rlen);

        if (!rolename || !rlen || rolename[0] == '<') {
            printf("Cannot retrieve role name from metadata service\n");
            exit(1);
        }

        s3Role = g_strndup((const char *) rolename, rlen);
        free(rolename);

        writer_s3_refresh_s3credentials();
    }

    if (!s3SecretAccessKey) {
        printf("Must set s3SecretAccessKey to save to s3\n");
        exit(1);
    }

    if (config.pcapWriteSize < 5242880) {
        config.pcapWriteSize = 5242880;
    }

    if (!s3Host) {
        if (s3PathAccessStyle) {
            if (strcmp(s3Region, "us-east-1") == 0) {
                s3Host = g_strdup("s3.amazonaws.com");
            } else {
                s3Host = g_strjoin("", "s3-", s3Region, ".amazonaws.com", NULL);
            }
        } else {
            if (strcmp(s3Region, "us-east-1") == 0) {
                s3Host = g_strjoin("", s3Bucket, ".s3.amazonaws.com", NULL);
            } else {
                s3Host = g_strjoin("", s3Bucket, ".s3-", s3Region, ".amazonaws.com", NULL);
            }
        }
    }

    config.maxFileSizeB = MIN(config.maxFileSizeB, config.pcapWriteSize*2000);

    if (s3WriteGzip) {
      // We only have 33 bits of offset to play with. Limit the file size to that
      // minus a bit to allow for the last compressed chunk to be emitted
      config.maxFileSizeB = MIN(config.maxFileSizeB, 0x1fff00000LL);
    }

    char host[200];
    if (s3UseHttp) {
        snprintf(host, sizeof(host), "http://%s", s3Host);
    } else {
        snprintf(host, sizeof(host), "https://%s", s3Host);
    }
    s3Server = moloch_http_create_server(host, s3MaxConns, s3MaxRequests, s3Compress);
    moloch_http_set_print_errors(s3Server);
    moloch_http_set_header_cb(s3Server, writer_s3_header_cb);

    checksum = g_checksum_new(G_CHECKSUM_SHA256);
    DLL_INIT(fs3_, &fileQ);
}
/******************************************************************************/
LOCAL gboolean writer_s3_file_time_gfunc (gpointer UNUSED(user_data))
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);

    MOLOCH_LOCK(output);
    if (currentFile && outputFilePos > 24 && (ts.tv_sec - currentFile->outputFileTime.tv_sec) >= config.maxFileTimeM*60) {
        writer_s3_flush(TRUE);
    }
    MOLOCH_UNLOCK(output);

    return TRUE;
}

/******************************************************************************/
void moloch_plugin_init()
{
    moloch_writers_add("s3", writer_s3_init);

    if (config.maxFileTimeM > 0) {
        g_timeout_add_seconds( 30, writer_s3_file_time_gfunc, 0);
    }
}
