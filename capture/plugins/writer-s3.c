/******************************************************************************/
/* writer-s3.c  -- S3 Writer Plugin
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "arkime.h"
#include "arkimeconfig.h"
#ifdef HAVE_ZSTD
#include <zstd.h>
#endif

extern ArkimeConfig_t        config;

typedef struct writer_s3_output {
    struct writer_s3_output   *os3_next, *os3_prev;
    uint16_t                   os3_count;

    uint8_t                   *buf;
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

    char                      *outputBuffer;
    uint32_t                   outputPos;
    uint32_t                   outputId;
    uint32_t                   packets;
    uint64_t                   packetBytesWritten;

    // outputActualFilePos is the offset in the compressed file
    // outputLastBLockStart is the offset in the compressed file of the most recent block
    // outputOffsetInBlock is the offset within the current decompressed block

    uint64_t                   outputActualFilePos;
    uint64_t                   outputLastBlockStart;
    uint32_t                   outputOffsetInBlock;
    uint32_t                   outputDataSinceLastMiniBlock;

    z_stream                   z_strm;

#ifdef HAVE_ZSTD
    ZSTD_CStream              *zstd_strm;
    ZSTD_outBuffer             zstd_out;
    ZSTD_inBuffer              zstd_in;
    uint64_t                   zstd_saved; // How much we've "saved" not including current buffer so
                                           // zstd_saved + s3file->zstd_out.pos == z_strm.total_out
#endif
} SavepcapS3File_t;

SavepcapS3File_t            *currentFiles[ARKIME_MAX_PACKET_THREADS];

LOCAL  ARKIME_LOCK_DEFINE(fileQ);
LOCAL  SavepcapS3File_t      fileQ;

LOCAL  void *                s3Server = 0;
LOCAL  void *                metadataServer = 0;
LOCAL  char                  *s3Region;
LOCAL  char                  *s3Host;
LOCAL  char                  *s3Bucket;
LOCAL  char                  s3PathAccessStyle;
LOCAL  char                   s3Compress;
LOCAL  char                  *s3StorageClass;
LOCAL  uint32_t               s3MaxConns;
LOCAL  uint32_t               s3MaxRequests;
LOCAL  char                   s3UseHttp;
LOCAL  char                   s3UseTokenForMetadata;
LOCAL  int                    s3CompressionLevel;

LOCAL  char                   credURL[1024];

LOCAL  int                    inprogress;


void writer_s3_flush(SavepcapS3File_t *s3file, gboolean all);

typedef enum {
    ARKIME_COMPRESSION_NONE,
    ARKIME_COMPRESSION_GZIP,
    ARKIME_COMPRESSION_ZSTD
} S3CompressionMode;

typedef struct {
    char                  *s3AccessKeyId;
    char                  *s3SecretAccessKey;
    char                  *s3Token;
} S3Credentials;

LOCAL S3Credentials *s3MetaCreds;   // Creds from meta service, use if non NULL
LOCAL S3Credentials  s3ConfigCreds; // Creds from config file

LOCAL S3CompressionMode compressionMode = ARKIME_COMPRESSION_NONE;
LOCAL uint32_t s3CompressionBlockSize;

// These must agree with the index.js
#define COMPRESSED_WITHIN_BLOCK_BITS  20



void writer_s3_request(char *method, char *path, char *qs, uint8_t *data, int len, gboolean specifyStorageClass, ArkimeHttpResponse_cb cb, gpointer uw);
/******************************************************************************/
uint32_t writer_s3_queue_length()
{
    int q = 0;

    ARKIME_LOCK(fileQ);

    SavepcapS3File_t *file;
    DLL_FOREACH(fs3_, &fileQ, file)
    {
        if (config.debug && DLL_COUNT(os3_, &file->outputQ) > 0)
            LOG("Waiting: %s - %d", file->outputFileName, DLL_COUNT(os3_, &file->outputQ));
        q += DLL_COUNT(os3_, &file->outputQ);
    }

    if (config.debug) {
        LOG("queue length: http Q:%d in progress: %d waiting:%d", arkime_http_queue_length(s3Server), inprogress, q);
    }

    q += arkime_http_queue_length(s3Server) + inprogress;
    ARKIME_UNLOCK(fileQ);

    return q;
}
/******************************************************************************/
void writer_s3_complete_cb (int code, uint8_t *data, int len, gpointer uw)
{
    ARKIME_LOCK(fileQ);

    SavepcapS3File_t  *file = uw;
    inprogress--;

    if (code != 200) {
        LOG("Bad Response: %d %s %.*s", code, file->outputFileName, len, data);
    }

    if (config.debug)
        LOG("Complete-Response: %s %d %.*s", file->outputFileName, len, len, data);

    uint64_t size;
    switch (compressionMode) {
    case ARKIME_COMPRESSION_NONE:
        size = file->outputActualFilePos;
        break;
    case ARKIME_COMPRESSION_GZIP:
        size = file->z_strm.total_out;
        break;
    case ARKIME_COMPRESSION_ZSTD:
#ifdef HAVE_ZSTD
        size = file->zstd_saved;
#endif
        break;
    }

    arkime_db_update_filesize(file->outputId, size, file->packetBytesWritten, file->packets);


    DLL_REMOVE(fs3_, &fileQ, file);
    if (file->uploadId)
        g_free(file->uploadId);
    g_free(file->outputFileName);
#ifdef HAVE_ZSTD
    if (file->zstd_strm)
        ZSTD_freeCStream(file->zstd_strm);
#endif


    ARKIME_TYPE_FREE(SavepcapS3File_t, file);

    ARKIME_UNLOCK(fileQ);
}
/******************************************************************************/
void writer_s3_part_cb (int code, uint8_t *data, int len, gpointer uw)
{
    SavepcapS3File_t  *file = uw;

    inprogress--;

    if (code != 200) {
        LOG("Bad Response: %d %s %.*s", code, file->outputFileName, len, data);
    }

    if (config.debug)
        LOG("Part-Response: %d %s %d", code, file->outputFileName, len);

    file->partNumberResponses++;

    if (file->doClose && file->partNumber == file->partNumberResponses) {
        char qs[1000];
        snprintf(qs, sizeof(qs), "uploadId=%s", file->uploadId);
        char *buf = arkime_http_get_buffer(1000000);
        BSB bsb;

        BSB_INIT(bsb, buf, 1000000);
        BSB_EXPORT_cstr(bsb, "<CompleteMultipartUpload>\n");
        int i;
        for (i = 1; i < file->partNumber; i++) {
            BSB_EXPORT_sprintf(bsb, "<Part><PartNumber>%d</PartNumber><ETag>%s</ETag></Part>\n", i, file->partNumbers[i]);
            g_free(file->partNumbers[i]);
        }
        BSB_EXPORT_cstr(bsb, "</CompleteMultipartUpload>\n");

        writer_s3_request("POST", file->outputPath, qs, (uint8_t*)buf, BSB_LENGTH(bsb), FALSE, writer_s3_complete_cb, file);
        if (config.debug > 1)
            LOG("Complete-Request: %s %.*s", file->outputFileName, (int)BSB_LENGTH(bsb), buf);
    }

}
/******************************************************************************/
uint8_t *arkime_get_instance_metadata(void *serverV, char *key, int key_len, size_t *mlen)
{
    char *requestHeaders[2];
    char  tokenHeader[200];
    requestHeaders[1] = NULL;
    if (s3UseTokenForMetadata) {
        char *tokenRequestHeaders[2] = {"X-aws-ec2-metadata-token-ttl-seconds: 30", NULL};
        if (config.debug)
            LOG("Requesting IMDSv2 metadata token");
        uint8_t *token = arkime_http_send_sync(serverV, "PUT", "/latest/api/token", -1, NULL, 0, tokenRequestHeaders, mlen, NULL);
        if (config.debug)
            LOG("IMDSv2 metadata token received");
        snprintf(tokenHeader, sizeof(tokenHeader), "X-aws-ec2-metadata-token: %s", token);
        requestHeaders[0] = tokenHeader;
    } else {
        if (config.debug)
            LOG("Using IMDSv1");
        requestHeaders[0] = NULL;
    }
    return arkime_http_send_sync(serverV, "GET", key, key_len, NULL, 0, requestHeaders, mlen, NULL);
}
/******************************************************************************/
void writer_s3_free_creds(S3Credentials *creds)
{
    g_free(creds->s3AccessKeyId);
    g_free(creds->s3SecretAccessKey);
    g_free(creds->s3Token);
    ARKIME_TYPE_FREE(S3Credentials, creds);
}
/******************************************************************************/
/* Timer callback to refresh our creds. We fetch them into new structure
 * and free the old structure later incase a thread is using them.
 */
LOCAL gboolean writer_s3_refresh_creds_gfunc (gpointer UNUSED(user_data))
{
    size_t clen;

    S3Credentials *newCreds = ARKIME_TYPE_ALLOC0(S3Credentials);

    uint8_t *credentials = arkime_get_instance_metadata(metadataServer, credURL, -1, &clen);

    if (credentials && clen) {
        // Now need to extract access key, secret key and token
        newCreds->s3AccessKeyId = arkime_js0n_get_str(credentials, clen, "AccessKeyId");
        newCreds->s3SecretAccessKey = arkime_js0n_get_str(credentials, clen, "SecretAccessKey");
        newCreds->s3Token = arkime_js0n_get_str(credentials, clen, "Token");
        if (config.debug)
            LOG("Found AccessKeyId %s", newCreds->s3AccessKeyId);
    }

    if (newCreds->s3AccessKeyId && newCreds->s3SecretAccessKey && newCreds->s3Token) {
        arkime_free_later(s3MetaCreds, (GDestroyNotify)writer_s3_free_creds);
        s3MetaCreds = newCreds;
    } else {
        LOGEXIT("Cannot retrieve credentials from metadata service at %s\n", credURL);
    }

    free(credentials);

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
void writer_s3_init_cb (int code, uint8_t *data, int len, gpointer uw)
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
        ARKIME_TYPE_FREE(SavepcapS3Output_t, output);
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
void writer_s3_request(char *method, char *path, char *qs, uint8_t *data, int len, gboolean specifyStorageClass, ArkimeHttpResponse_cb cb, gpointer uw)
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

    S3Credentials *creds = s3MetaCreds ? s3MetaCreds : &s3ConfigCreds;

    snprintf(storageClassHeader, sizeof(storageClassHeader), "x-amz-storage-class:%s\n", s3StorageClass);
    if (creds->s3Token) {
      snprintf(tokenHeader, sizeof(tokenHeader), "x-amz-security-token:%s\n", creds->s3Token);
    }

    GChecksum *checksum = g_checksum_new(G_CHECKSUM_SHA256);
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
            (creds->s3Token?tokenHeader:""),
            (specifyStorageClass?storageClassHeader:""),
            (creds->s3Token?";x-amz-security-token":""),
            (specifyStorageClass?";x-amz-storage-class":""),
            bodyHash);
    //LOG("canonicalRequest: %s", canonicalRequest);

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
    //LOG("stringToSign: %s", stringToSign);

    char kSecret[1000];
    snprintf(kSecret, sizeof(kSecret), "AWS4%s", creds->s3SecretAccessKey);

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

    //LOG("signature: %s", signature);

    snprintf(fullpath, sizeof(fullpath), "%s?%s", objectkey, qs);
    //LOG("fullpath: %s", fullpath);

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
            creds->s3AccessKeyId, datetime, s3Region,
            (creds->s3Token?";x-amz-security-token":""),
            (specifyStorageClass?";x-amz-storage-class":""),
            signature
            );

    snprintf(strs[1], sizeof(strs[1]), "x-amz-content-sha256: %s" , bodyHash);
    snprintf(strs[2], sizeof(strs[2]), "x-amz-date: %s", datetime);

    if (creds->s3Token) {
        snprintf(tokenHeader, sizeof(tokenHeader), "x-amz-security-token: %s", creds->s3Token);
        headers[nextHeader++] = tokenHeader;
    }

    if (specifyStorageClass) {
        // Note the missing newline in this place
        snprintf(storageClassHeader, sizeof(storageClassHeader), "x-amz-storage-class: %s", s3StorageClass);
        headers[nextHeader++] = storageClassHeader;
    }

    headers[nextHeader] = NULL;

    inprogress++;
    arkime_http_send(s3Server, method, fullpath, strlen(fullpath), (char*)data, len, headers, FALSE, cb, uw);
    g_checksum_free(checksum);
}
/******************************************************************************/
/* Make a new encryption full block/frame.
 * This will cause the encryption to fully flush any waiting data
 * and the next data written will cause a new block header.
 */
LOCAL void make_new_block(SavepcapS3File_t *s3file) {
    if (compressionMode == ARKIME_COMPRESSION_GZIP) {
        while (TRUE) {
            deflate(&s3file->z_strm, Z_FULL_FLUSH);
            if (s3file->z_strm.avail_out > 0) {
                break;
            }
            writer_s3_flush(s3file, FALSE);
        }

        s3file->outputActualFilePos = s3file->z_strm.total_out;
        s3file->outputLastBlockStart = s3file->outputActualFilePos;
        s3file->outputOffsetInBlock = 0;
        s3file->outputDataSinceLastMiniBlock = 0;
    } else if (compressionMode == ARKIME_COMPRESSION_ZSTD) {
#ifdef HAVE_ZSTD
        while (TRUE) {
            ZSTD_compressStream2(s3file->zstd_strm, &s3file->zstd_out, &s3file->zstd_in, ZSTD_e_end);
            if (s3file->zstd_out.pos < s3file->zstd_out.size)
                break;

            // Out of space, flush and try again
            writer_s3_flush(s3file, FALSE);
        }

        s3file->outputActualFilePos = s3file->zstd_saved + s3file->zstd_out.pos;
        s3file->outputLastBlockStart = s3file->outputActualFilePos;
        s3file->outputOffsetInBlock = 0;
        s3file->outputDataSinceLastMiniBlock = 0;
#endif
    }
}
/******************************************************************************/
/* Make sure there is enough space in encryption buffers for incoming data and
 * data that is waiting to be written out. Because encryption lib does its
 * own buffer we do our best guess here.
 */
LOCAL void ensure_space_for_output(SavepcapS3File_t *s3file, size_t space) {
    if (compressionMode == ARKIME_COMPRESSION_GZIP) {
        size_t max_need_space = s3file->outputActualFilePos - s3file->outputLastBlockStart + 64 + deflateBound(&s3file->z_strm, space + s3file->outputDataSinceLastMiniBlock);

        if (max_need_space >= s3CompressionBlockSize) {
            while (TRUE) {
                deflate(&s3file->z_strm, Z_BLOCK);
                if (s3file->z_strm.avail_out > 0) {
                    s3file->outputActualFilePos = s3file->z_strm.total_out;
                    s3file->outputDataSinceLastMiniBlock = 0;
                    break;
                }
                writer_s3_flush(s3file, FALSE);
            }
            // Recompute after the flush
            max_need_space = s3file->outputActualFilePos - s3file->outputLastBlockStart + 64 + deflateBound(&s3file->z_strm, space + s3file->outputDataSinceLastMiniBlock);
            if (max_need_space >= 3 * s3CompressionBlockSize / 4) {
                make_new_block(s3file);
            }
        }
    } else if (compressionMode == ARKIME_COMPRESSION_ZSTD) {
#ifdef HAVE_ZSTD
        size_t max_need_space = s3file->outputActualFilePos - s3file->outputLastBlockStart + 64 + ZSTD_compressBound(space + s3file->outputDataSinceLastMiniBlock);

        if (max_need_space >= s3CompressionBlockSize) {
            while (TRUE) {
                ZSTD_compressStream2(s3file->zstd_strm, &s3file->zstd_out, &s3file->zstd_in, ZSTD_e_flush);
                if (s3file->zstd_out.pos < s3file->zstd_out.size) {
                    s3file->outputActualFilePos = s3file->zstd_saved + s3file->zstd_out.pos;
                    s3file->outputDataSinceLastMiniBlock = 0;
                    break;
                }
                writer_s3_flush(s3file, FALSE);
            }
            // Recompute after the flush
            max_need_space = s3file->outputActualFilePos - s3file->outputLastBlockStart + 64 + ZSTD_compressBound(space + s3file->outputDataSinceLastMiniBlock);
            if (max_need_space >= 3 * s3CompressionBlockSize / 4) {
                make_new_block(s3file);
            }
        }
#endif
    }
}
/******************************************************************************/
/* Add data:length to the output.
 * This is call for file headers and packets. If for packets it will be called twice,
 * once with packetHeader true (with the header) and a second time with packetHeader false and the packet.
 *
 * When packetHeader is true the return value is where the writerFilePos of the start of the packetHeader.
 * This value might be encoded if using encryption.
 */
LOCAL uint64_t append_to_output(SavepcapS3File_t *s3file, void *data, size_t length, gboolean packetHeader, size_t extra_space) {
    uint64_t pos;

    if (compressionMode == ARKIME_COMPRESSION_GZIP) {
        if (s3file->outputActualFilePos == 0) {
            memset(&s3file->z_strm, 0, sizeof(s3file->z_strm));
            s3file->z_strm.next_out = (Bytef *) s3file->outputBuffer;
            s3file->z_strm.avail_out = config.pcapWriteSize + ARKIME_PACKET_MAX_LEN;
            s3file->z_strm.zalloc = Z_NULL;
            s3file->z_strm.zfree = Z_NULL;

            if (s3CompressionLevel == 0)
                deflateInit2(&s3file->z_strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + 15, 8, Z_DEFAULT_STRATEGY);
            else
                deflateInit2(&s3file->z_strm, MIN(s3CompressionLevel, Z_BEST_COMPRESSION), Z_DEFLATED, 16 + 15, 9, Z_DEFAULT_STRATEGY);
        }

        // Make room for the header and packet
        if (packetHeader) {
            ensure_space_for_output(s3file, length + extra_space);
        }

        pos = (s3file->outputLastBlockStart << COMPRESSED_WITHIN_BLOCK_BITS) + s3file->outputOffsetInBlock;

        s3file->z_strm.next_in = data;
        s3file->z_strm.avail_in = length;

        while (s3file->z_strm.avail_in != 0) {
            if (s3file->z_strm.avail_out == 0) {
                writer_s3_flush(s3file, FALSE);
            }

            deflate(&s3file->z_strm, Z_NO_FLUSH);
            s3file->outputActualFilePos = s3file->z_strm.total_out;
        }

        s3file->outputOffsetInBlock += length;
        s3file->outputDataSinceLastMiniBlock += length;

        if (!packetHeader &&
               (s3file->outputOffsetInBlock >= (1 << COMPRESSED_WITHIN_BLOCK_BITS) - 16 ||
                s3file->outputActualFilePos > s3file->outputLastBlockStart + s3CompressionBlockSize)) {
            // We need to make a new block
            make_new_block(s3file);
        }
    } else if (compressionMode == ARKIME_COMPRESSION_ZSTD) {
#ifdef HAVE_ZSTD
        if (!s3file->zstd_strm) {
            s3file->zstd_strm = ZSTD_createCStream();
            if (s3CompressionLevel != 0)
              ZSTD_CCtx_setParameter(s3file->zstd_strm, ZSTD_c_compressionLevel, MIN(s3CompressionLevel, ZSTD_maxCLevel()));
            s3file->zstd_out.dst = s3file->outputBuffer;
            s3file->zstd_out.size = config.pcapWriteSize + ARKIME_PACKET_MAX_LEN;
            s3file->zstd_out.pos = 0;
        }

        // Make room for the header and packet
        if (packetHeader) {
            ensure_space_for_output(s3file, length + extra_space);
        }

        pos = (s3file->outputLastBlockStart << COMPRESSED_WITHIN_BLOCK_BITS) + s3file->outputOffsetInBlock;

        s3file->zstd_in.src = (Bytef *)data;
        s3file->zstd_in.size = length;
        s3file->zstd_in.pos = 0;

        while (s3file->zstd_in.pos < s3file->zstd_in.size) {
            if (s3file->zstd_out.pos == s3file->zstd_out.size) {
                writer_s3_flush(s3file, FALSE);
            }
            ZSTD_compressStream2(s3file->zstd_strm, &s3file->zstd_out, &s3file->zstd_in, ZSTD_e_continue);
            s3file->outputActualFilePos = s3file->zstd_saved + s3file->zstd_out.pos;
        }


        s3file->outputOffsetInBlock += length;
        s3file->outputDataSinceLastMiniBlock += length;

        if (!packetHeader &&
               (s3file->outputOffsetInBlock >= (1 << COMPRESSED_WITHIN_BLOCK_BITS) - 16 ||
                s3file->outputActualFilePos > s3file->outputLastBlockStart + s3CompressionBlockSize)) {
            // We need to make a new block
            make_new_block(s3file);
        }
#endif
    } else {
        memcpy(s3file->outputBuffer + s3file->outputPos, data, length);

        pos = s3file->outputActualFilePos;

        s3file->outputActualFilePos += length;
        s3file->outputPos += length;

        if (s3file->outputPos > config.pcapWriteSize) {
            writer_s3_flush(s3file, FALSE);
        }
    }

    if (packetHeader)
        return pos;
    return 0;
}
/******************************************************************************/
/* Called when the buffer we are saving to is full and needs to be
 * sent along. Encryption blocks can cross buffers.
 */
void writer_s3_flush(SavepcapS3File_t *s3file, gboolean end)
{
    if (!s3file)
        return;

    if (compressionMode == ARKIME_COMPRESSION_GZIP) {
      if (end) {
        deflate(&s3file->z_strm, Z_FINISH);

        if (s3file->z_strm.avail_out == 0) {
          writer_s3_flush(s3file, FALSE);
          deflate(&s3file->z_strm, Z_FINISH);
        }
      }

      s3file->outputPos = s3file->z_strm.next_out - (Bytef *) s3file->outputBuffer;
    } else if (compressionMode == ARKIME_COMPRESSION_ZSTD) {
#ifdef HAVE_ZSTD
        if (end) {
            ZSTD_endStream(s3file->zstd_strm, &s3file->zstd_out);
            if (s3file->zstd_out.pos == s3file->zstd_out.size) {
                writer_s3_flush(s3file, FALSE);
                ZSTD_endStream(s3file->zstd_strm, &s3file->zstd_out);
            }
        }

      s3file->outputPos = s3file->zstd_out.pos;
      s3file->zstd_saved += s3file->zstd_out.pos;
#endif
    }

    if (s3file->uploadId) {
        char qs[1000];

        snprintf(qs, sizeof(qs), "partNumber=%d&uploadId=%s", s3file->partNumber, s3file->uploadId);
        writer_s3_request("PUT", s3file->outputPath, qs, (uint8_t *)s3file->outputBuffer, s3file->outputPos, FALSE, writer_s3_part_cb, s3file);
        if (config.debug)
            LOG("Part-Request: %s %s", s3file->outputFileName, qs);
        s3file->partNumber++;
    } else {
        SavepcapS3Output_t *output = ARKIME_TYPE_ALLOC0(SavepcapS3Output_t);
        output->buf = (uint8_t *)s3file->outputBuffer;
        output->len = s3file->outputPos;
        DLL_PUSH_TAIL(os3_, &s3file->outputQ, output);
    }

    if (end) {
        s3file->doClose = TRUE;
    } else {
        s3file->outputBuffer = arkime_http_get_buffer(config.pcapWriteSize + ARKIME_PACKET_MAX_LEN);
        s3file->outputPos = 0;

        if (compressionMode == ARKIME_COMPRESSION_GZIP) {
            s3file->z_strm.next_out = (Bytef *) s3file->outputBuffer;
            s3file->z_strm.avail_out = config.pcapWriteSize + ARKIME_PACKET_MAX_LEN;
        } else if (compressionMode == ARKIME_COMPRESSION_ZSTD) {
#ifdef HAVE_ZSTD
            s3file->zstd_out.dst = s3file->outputBuffer;
            s3file->zstd_out.size = config.pcapWriteSize + ARKIME_PACKET_MAX_LEN;
            s3file->zstd_out.pos = 0;
#endif
        }
    }
}
/******************************************************************************/
void writer_s3_exit()
{
    for (int thread = 0; thread < config.packetThreads; thread++) {
        if (currentFiles[thread]) {
            writer_s3_flush(currentFiles[thread], TRUE);
            currentFiles[thread] = NULL;
        }
    }
}
/******************************************************************************/
extern ArkimePcapFileHdr_t pcapFileHeader;
SavepcapS3File_t *writer_s3_create(const ArkimePacket_t *packet)
{
    char               filename[1000];
    static char       *extension[3] = {"", ".gz", ".zst"};
    struct tm          tmp;
    int                offset = 6 + strlen(s3Region) + strlen(s3Bucket);
    char              *compressionBlockSizeArg = ARKIME_VAR_ARG_INT_SKIP;
    char              *packetPosEncoding = ARKIME_VAR_ARG_STR_SKIP;

    localtime_r(&packet->ts.tv_sec, &tmp);
    snprintf(filename, sizeof(filename), "s3://%s/%s/%s/#NUMHEX#-%02d%02d%02d-#NUM#.pcap%s", s3Region, s3Bucket, config.nodeName, tmp.tm_year%100, tmp.tm_mon+1, tmp.tm_mday, extension[compressionMode]);

    SavepcapS3File_t *s3file = ARKIME_TYPE_ALLOC0(SavepcapS3File_t);
    DLL_INIT(os3_, &s3file->outputQ);

    ARKIME_LOCK(fileQ);
    DLL_PUSH_TAIL(fs3_, &fileQ, s3file);
    ARKIME_UNLOCK(fileQ);

    if (compressionMode != ARKIME_COMPRESSION_NONE) {
        compressionBlockSizeArg = (char *)(uint64_t)s3CompressionBlockSize;
    }

    if (config.gapPacketPos) {
        packetPosEncoding = "gap0";
    }

    s3file->outputFileName = arkime_db_create_file_full(packet->ts.tv_sec, filename, 0, 0, &s3file->outputId,
            "packetPosEncoding", packetPosEncoding,
            "#compressionBlockSize", compressionBlockSizeArg,
            NULL);
    s3file->outputPath = s3file->outputFileName + offset;
    clock_gettime(CLOCK_REALTIME_COARSE, &s3file->outputFileTime);

    s3file->outputBuffer = arkime_http_get_buffer(config.pcapWriteSize + ARKIME_PACKET_MAX_LEN);
    s3file->outputPos = 0;
    uint32_t linktype = arkime_packet_dlt_to_linktype(pcapFileHeader.dlt);
    append_to_output(s3file, &pcapFileHeader, 20, FALSE, 0);
    append_to_output(s3file, &linktype, 4, FALSE, 0);
    make_new_block(s3file);                   // So we can read the header in a small amount of data fetched

    if (config.debug)
        LOG("Init-Request: %s", s3file->outputFileName);

    writer_s3_request("POST", s3file->outputPath, "uploads=", 0, 0, TRUE, writer_s3_init_cb, s3file);
    return s3file;
}

/******************************************************************************/
// Called inside each packet thread
LOCAL void writer_s3_file_time_check(ArkimeSession_t *session, void *UNUSED(uw1), void *UNUSED(uw2))
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME_COARSE, &ts);

    SavepcapS3File_t *s3file = currentFiles[session->thread];
    if (s3file && s3file->outputActualFilePos > 24 && (ts.tv_sec - s3file->outputFileTime.tv_sec) >= config.maxFileTimeM * 60) {
        writer_s3_flush(s3file, TRUE);
        currentFiles[session->thread] = NULL;
    }
}
/******************************************************************************/
/* This function is called every 30 second on the main thread. It
 * schedules writer_s3_check to be called on the packet thread
 */
LOCAL gboolean writer_s3_file_time_gfunc (gpointer UNUSED(user_data))
{
    for (int thread = 0; thread < config.packetThreads; thread++) {
        arkime_session_add_cmd_thread(thread, NULL, NULL, writer_s3_file_time_check);
    }

    return G_SOURCE_CONTINUE;
}

/******************************************************************************/
struct pcap_timeval {
    uint32_t tv_sec;             /* seconds */
    uint32_t tv_usec;            /* microseconds */
};
struct pcap_sf_pkthdr {
    struct pcap_timeval ts;     /* time stamp */
    uint32_t caplen;            /* length of portion present */
    uint32_t len;               /* length this packet (off wire) */
};
void
writer_s3_write(const ArkimeSession_t *const session, ArkimePacket_t * const packet)
{
    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.len        = packet->pktlen;

    SavepcapS3File_t *s3file = currentFiles[session->thread];
    if (!s3file) {
        currentFiles[session->thread] = s3file = writer_s3_create(packet);
    }

    s3file->packets++;
    s3file->packetBytesWritten += packet->pktlen;

    uint64_t pos = append_to_output(s3file, &hdr, sizeof(hdr), TRUE, packet->pktlen);
    append_to_output(s3file, packet->pkt, packet->pktlen, FALSE, 0);

    packet->writerFileNum = s3file->outputId;
    packet->writerFilePos = pos;

    if (s3file->outputActualFilePos >= config.maxFileSizeB) {
        writer_s3_flush(s3file, TRUE);
        currentFiles[session->thread] = NULL;
    }
}

/******************************************************************************/
void writer_s3_init(char *UNUSED(name))
{
    arkime_writer_queue_length = writer_s3_queue_length;
    arkime_writer_exit         = writer_s3_exit;
    arkime_writer_write        = writer_s3_write;

    s3Region              = arkime_config_str(NULL, "s3Region", "us-east-1");
    s3Host                = arkime_config_str(NULL, "s3Host", NULL);
    s3Bucket              = arkime_config_str(NULL, "s3Bucket", NULL);
    s3PathAccessStyle     = arkime_config_boolean(NULL, "s3PathAccessStyle", strchr(s3Bucket, '.') != NULL);
    s3Compress            = arkime_config_boolean(NULL, "s3Compress", FALSE);
    REMOVEDCONFIG("s3WriteGzip", "use s3Compression=gzip");
    char *s3Compression   = arkime_config_str(NULL, "s3Compression", "zstd");
    s3CompressionLevel    = arkime_config_int(NULL, "s3CompressionLevel", 0, 0, 22);
    s3CompressionBlockSize= arkime_config_int(NULL, "s3CompressionBlockSize", 100000, 0xffff, 0x7ffff);
    s3StorageClass        = arkime_config_str(NULL, "s3StorageClass", "STANDARD");
    s3MaxConns            = arkime_config_int(NULL, "s3MaxConns", 20, 5, 1000);
    s3MaxRequests         = arkime_config_int(NULL, "s3MaxRequests", 500, 10, 5000);
    s3UseHttp             = arkime_config_boolean(NULL, "s3UseHttp", FALSE);
    s3UseTokenForMetadata = arkime_config_boolean(NULL, "s3UseTokenForMetadata", TRUE);
    int s3UseECSEnv       = arkime_config_boolean(NULL, "s3UseECSEnv", FALSE);

    s3ConfigCreds.s3AccessKeyId     = arkime_config_str(NULL, "s3AccessKeyId", NULL);
    s3ConfigCreds.s3SecretAccessKey = arkime_config_str(NULL, "s3SecretAccessKey", NULL);

    config.gapPacketPos = arkime_config_boolean(NULL, "s3GapPacketPos", TRUE);

    if (!s3Bucket) {
        CONFIGEXIT("Must set s3Bucket to save to s3\n");
    }

    if (s3Compression != NULL) {
        if (strcmp(s3Compression, "none") == 0) {
            compressionMode = ARKIME_COMPRESSION_NONE;
        } else if (strcmp(s3Compression, "gzip") == 0) {
            compressionMode = ARKIME_COMPRESSION_GZIP;
        } else if (strcmp(s3Compression, "zstd") == 0) {
#ifdef HAVE_ZSTD
            compressionMode = ARKIME_COMPRESSION_ZSTD;
#else
            CONFIGEXIT("Arkime capture was not compiled with zstd support");
#endif
        } else {
            CONFIGEXIT("Unknown s3Compression value %s", s3Compression);
        }
    }

    if (s3Compress && compressionMode != ARKIME_COMPRESSION_NONE) {
        LOG("Setting s3Compress to false since compressing pcap");
        s3Compress = FALSE;
    }

    if (s3UseECSEnv) {
        char *uri = getenv("ECS_CONTAINER_METADATA_URI_V4");
        if (!uri)
            LOGEXIT("ECS_CONTAINER_METADATA_URI_V4 not set");
        uri = g_strdup(uri);

        // Find slash after https://
        char *slash = strchr(uri + 8, '/');

        if (slash) {
            *slash = 0;
        }


        char *relativeURI = getenv("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI");
        if (!relativeURI)
            LOGEXIT("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI not set");

        g_strlcpy(credURL, relativeURI, sizeof(credURL));

        metadataServer = arkime_http_create_server(uri, 10, 10, 0);

        if (config.debug) {
            LOG("metadata base: %s cred uri: %s", uri, credURL);
        }
        g_free(uri);

        g_timeout_add_seconds( 280, writer_s3_refresh_creds_gfunc, 0);
        writer_s3_refresh_creds_gfunc(NULL);
    } else if (!s3ConfigCreds.s3AccessKeyId || !s3ConfigCreds.s3AccessKeyId[0]) {
        // Fetch the data from the EC2 metadata service
        size_t rlen;

        metadataServer = arkime_http_create_server("http://169.254.169.254", 10, 10, 0);
        arkime_http_set_print_errors(metadataServer);

        s3ConfigCreds.s3AccessKeyId = NULL;

        uint8_t *rolename = arkime_get_instance_metadata(metadataServer, "/latest/meta-data/iam/security-credentials/", -1, &rlen);

        if (!rolename || !rlen || rolename[0] == '<') {
            LOGEXIT("Cannot retrieve role name from metadata service\n");
        }

        snprintf(credURL, sizeof(credURL), "/latest/meta-data/iam/security-credentials/%.*s", (int)rlen, rolename);
        free(rolename);

        g_timeout_add_seconds( 280, writer_s3_refresh_creds_gfunc, 0);
        writer_s3_refresh_creds_gfunc(NULL);
    } else if (s3ConfigCreds.s3AccessKeyId && !s3ConfigCreds.s3SecretAccessKey) {
        CONFIGEXIT("Must set s3SecretAccessKey to save to s3\n");
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

    // Support up to 1000 S3 parts
    config.maxFileSizeB = MIN(config.maxFileSizeB, config.pcapWriteSize * 1000LL);

    // S3 has a 5TiB max size
    config.maxFileSizeB = MIN(config.maxFileSizeB, 0x50000000000LL);

    if (compressionMode != ARKIME_COMPRESSION_NONE) {
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
    s3Server = arkime_http_create_server(host, s3MaxConns, s3MaxRequests, s3Compress);
    arkime_http_set_print_errors(s3Server);
    arkime_http_set_header_cb(s3Server, writer_s3_header_cb);

    DLL_INIT(fs3_, &fileQ);

    if (config.maxFileTimeM > 0) {
        g_timeout_add_seconds( 30, writer_s3_file_time_gfunc, 0);
    }
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_writers_add("s3", writer_s3_init);
}
