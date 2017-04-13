/******************************************************************************/
/* writer-s3.c  -- S3 Writer Plugin
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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
    char                      *outputPath;
    SavepcapS3Output_t         outputQ;
    char                      *uploadId;
    int                        partNumber;
    int                        partNumberResponses;
    char                       doClose;
    char                      *partNumbers[2001];
} SavepcapS3File_t;

static char                 *outputBuffer;
static uint32_t              outputPos;
static uint32_t              outputId;
static uint64_t              outputFilePos = 0;

SavepcapS3File_t            *currentFile;
static SavepcapS3File_t      fileQ;


static void *                s3Server = 0;
static char                  *s3Region;
static char                   s3Host[100];
static char                  *s3Bucket;
static char                  *s3AccessKeyId;
static char                  *s3SecretAccessKey;
static char                   s3Compress;
static uint32_t               s3MaxConns;
static uint32_t               s3MaxRequests;

static int                    inprogress;

void writer_s3_request(char *method, char *path, char *qs, unsigned char *data, int len, gboolean reduce, MolochHttpResponse_cb cb, gpointer uw);

static MOLOCH_LOCK_DEFINE(output);
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
        LOG("Bad Response: %d %s", code, file->outputFileName);
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
void writer_s3_part_cb (int code, unsigned char *UNUSED(data), int UNUSED(len), gpointer uw)
{
    SavepcapS3File_t  *file = uw;

    inprogress--;

    if (code != 200) {
        LOG("Bad Response: %d %s", code, file->outputFileName);
    }

    if (config.debug)
        LOG("Part-Response: %s %d", file->outputFileName, len);

    file->partNumberResponses++;

    char qs[1000];
    if (file->doClose && file->partNumber == file->partNumberResponses) {
        snprintf(qs, sizeof(qs), "uploadId=%s", file->uploadId);
        char *buf = moloch_http_get_buffer(1000000);
        BSB bsb;

        BSB_INIT(bsb, buf, 1000000);
        BSB_EXPORT_cstr(bsb, "<CompleteMultipartUpload>\n");
        int i;
        for (i = 1; i < file->partNumber; i++) {
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
void writer_s3_init_cb (int UNUSED(code), unsigned char *data, int len, gpointer uw)
{
    SavepcapS3File_t   *file = uw;

    inprogress--;

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
        LOG("Unknown s3 response: %.*s", len, data);
        exit(1);
    }
    g_match_info_free(match_info);

    char qs[1000];
    while (DLL_POP_HEAD(os3_, &file->outputQ, output)) {
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
void writer_s3_request(char *method, char *path, char *qs, unsigned char *data, int len, gboolean reduce, MolochHttpResponse_cb cb, gpointer uw)
{
    char           canonicalRequest[1000];
    char           datetime[17];
    char           fullpath[1000];
    char           bodyHash[1000];
    struct timeval outputFileTime;

    gettimeofday(&outputFileTime, 0);
    struct tm         *gm = gmtime(&outputFileTime.tv_sec);
    snprintf(datetime, sizeof(datetime), 
            "%04d%02d%02dT%02d%02d%02dZ",
            gm->tm_year + 1900,
            gm->tm_mon+1,
            gm->tm_mday,
            gm->tm_hour,
            gm->tm_min,
            gm->tm_sec);



    g_checksum_reset(checksum);
    g_checksum_update(checksum, data, len);
    strcpy(bodyHash, g_checksum_get_string(checksum));
    snprintf(canonicalRequest, sizeof(canonicalRequest),
             "%s\n"       // HTTPRequestMethod
             "/%s%s\n"    // CanonicalURI
             "%s\n"       // CanonicalQueryString
             //CanonicalHeaders
             "host:%s\n"
             "x-amz-content-sha256:%s\n"
             "x-amz-date:%s\n"
             "%s"
             "\n"      
             // SignedHeaders
             "host;x-amz-content-sha256;x-amz-date%s\n" 
             "%s"     // HexEncode(Hash(RequestPayload))
             ,
             method,
             s3Bucket, path,
             qs,
             s3Host,
             bodyHash,
             datetime,
             (reduce?"x-amz-storage-class:REDUCED_REDUNDANCY\n":""),
             (reduce?";x-amz-storage-class":""),
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
    snprintf(kSecret, sizeof(kSecret), "AWS4%s", s3SecretAccessKey);

    char  kDate[1000];
    gsize kDateLen = sizeof(kDate);
    GHmac *hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kSecret, strlen(kSecret));
    g_hmac_update(hmac, (guchar*)datetime, 8);
    g_hmac_get_digest(hmac, (guchar*)kDate, &kDateLen);
    g_hmac_unref(hmac);

    char  kRegion[1000];
    gsize kRegionLen = sizeof(kRegion);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kDate, kDateLen);
    g_hmac_update(hmac, (guchar*)s3Region, -1);
    g_hmac_get_digest(hmac, (guchar*)kRegion, &kRegionLen);
    g_hmac_unref(hmac);

    char  kService[1000];
    gsize kServiceLen = sizeof(kService);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kRegion, kRegionLen);
    g_hmac_update(hmac, (guchar*)"s3", 2);
    g_hmac_get_digest(hmac, (guchar*)kService, &kServiceLen);
    g_hmac_unref(hmac);

    char kSigning[1000];
    gsize kSigningLen = sizeof(kSigning);
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kService, kServiceLen);
    g_hmac_update(hmac, (guchar*)"aws4_request", 12);
    g_hmac_get_digest(hmac, (guchar*)kSigning, &kSigningLen);
    g_hmac_unref(hmac);

    char signature[1000];
    hmac = g_hmac_new(G_CHECKSUM_SHA256, (guchar*)kSigning, kSigningLen);
    g_hmac_update(hmac, (guchar*)stringToSign, -1);
    strcpy(signature, g_hmac_get_string(hmac));
    g_hmac_unref(hmac);

    //LOG("signature: %s", signature);

    snprintf(fullpath, sizeof(fullpath), "/%s%s?%s", s3Bucket, path, qs);

    char strs[3][1000];
    char *headers[7];
    headers[0] = "Expect:";
    headers[1] = "Content-Type:";
    headers[2] = strs[0];
    headers[3] = strs[1];
    headers[4] = strs[2];
    headers[6] = NULL;

    snprintf(strs[0], 1000,
            "Authorization: AWS4-HMAC-SHA256 Credential=%s/%8.8s/%s/s3/aws4_request,SignedHeaders=host;x-amz-content-sha256;x-amz-date%s,Signature=%s"
            , 
            s3AccessKeyId, datetime, s3Region, 
            (reduce?";x-amz-storage-class":""),
            signature
            );

    snprintf(strs[1], 1000, "x-amz-content-sha256: %s" , bodyHash);
    snprintf(strs[2], 1000, "x-amz-date: %s", datetime);
    if (reduce) {
        headers[5] = "x-amz-storage-class: REDUCED_REDUNDANCY";
    } else {
        headers[5] = NULL;
    }

    inprogress++;
    moloch_http_send(s3Server, method, fullpath, strlen(fullpath), (char*)data, len, headers, FALSE, cb, uw);
}
/******************************************************************************/
void writer_s3_flush(gboolean all)
{
    char qs[1000];

    if (!currentFile)
        return;

    if (currentFile->uploadId) {
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
    struct tm         *tmp = localtime(&packet->ts.tv_sec);
    int                offset = 0;

    snprintf(filename, sizeof(filename), "s3://%s/%s/%s/#NUMHEX#-%02d%02d%02d-#NUM#.pcap", s3Region, s3Bucket, config.nodeName, tmp->tm_year%100, tmp->tm_mon+1, tmp->tm_mday);
    if (offset == 0)
        offset = 6 + strlen(s3Region) + strlen(s3Bucket);
    
    currentFile = MOLOCH_TYPE_ALLOC0(SavepcapS3File_t);
    DLL_INIT(os3_, &currentFile->outputQ);
    DLL_PUSH_TAIL(fs3_, &fileQ, currentFile);

    currentFile->outputFileName = moloch_db_create_file(packet->ts.tv_sec, filename, 0, 0, &outputId);
    currentFile->outputPath = currentFile->outputFileName + offset;
    outputFilePos = 24;

    outputBuffer = moloch_http_get_buffer(config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
    outputPos = 24;
    memcpy(outputBuffer, &pcapFileHeader, 24);

    if (config.debug)
        LOG("Init-Request: %s", currentFile->outputFileName);

    writer_s3_request("POST", currentFile->outputPath, "uploads=", 0, 0, TRUE, writer_s3_init_cb, currentFile);
}

/******************************************************************************/
struct pcap_timeval {
    int32_t tv_sec;		/* seconds */
    int32_t tv_usec;		/* microseconds */
};
struct pcap_sf_pkthdr {
    struct pcap_timeval ts;	/* time stamp */
    uint32_t caplen;		/* length of portion present */
    uint32_t len;		/* length this packet (off wire) */
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

    memcpy(outputBuffer + outputPos, (char *)&hdr, sizeof(hdr));
    outputPos += sizeof(hdr);

    memcpy(outputBuffer + outputPos, packet->pkt, packet->pktlen);
    outputPos += packet->pktlen;

    if(outputPos > config.pcapWriteSize) {
        writer_s3_flush(FALSE);
    }

    packet->writerFileNum = outputId;
    packet->writerFilePos = outputFilePos;
    outputFilePos += 16 + packet->pktlen;

    if (outputFilePos >= config.maxFileSizeB) {
        writer_s3_flush(TRUE);
    }
    MOLOCH_LOCK(output);
}
/******************************************************************************/
void writer_s3_init(char *UNUSED(name))
{
    moloch_writer_queue_length = writer_s3_queue_length;
    moloch_writer_exit         = writer_s3_exit;
    moloch_writer_write        = writer_s3_write;

    s3Region              = moloch_config_str(NULL, "s3Region", "us-east-1");
    s3Bucket              = moloch_config_str(NULL, "s3Bucket", NULL);
    s3AccessKeyId         = moloch_config_str(NULL, "s3AccessKeyId", NULL);
    s3SecretAccessKey     = moloch_config_str(NULL, "s3SecretAccessKey", NULL);
    s3Compress            = moloch_config_boolean(NULL, "s3Compress", FALSE);
    s3MaxConns            = moloch_config_int(NULL, "s3MaxConns", 20, 5, 1000);
    s3MaxRequests         = moloch_config_int(NULL, "s3MaxRequests", 500, 10, 5000);

    if (!s3Bucket) {
        printf("Must set s3Bucket to save to s3\n");
        exit(1);
    }

    if (!s3AccessKeyId) {
        printf("Must set s3AccessKeyId to save to s3\n");
        exit(1);
    }

    if (!s3SecretAccessKey) {
        printf("Must set s3SecretAccessKey to save to s3\n");
        exit(1);
    }

    if (config.pcapWriteSize < 5242880) {
        config.pcapWriteSize = 5242880;
    }

    if (strcmp(s3Region, "us-east-1") == 0) {
        strcpy(s3Host, "s3.amazonaws.com");
    } else {
        snprintf(s3Host, sizeof(s3Host), "s3-%s.amazonaws.com", s3Region);
    }

    config.maxFileSizeB = MIN(config.maxFileSizeB, config.pcapWriteSize*2000);

    char host[200];
    snprintf(host, sizeof(host), "https://%s", s3Host);
    s3Server = moloch_http_create_server(host, s3MaxConns, s3MaxRequests, s3Compress);
    moloch_http_set_header_cb(s3Server, writer_s3_header_cb);

    checksum = g_checksum_new(G_CHECKSUM_SHA256);
    DLL_INIT(fs3_, &fileQ);
}
/******************************************************************************/
void moloch_plugin_init()
{
    moloch_writers_add("s3", writer_s3_init);
}
