/******************************************************************************/
/* writer-simple.c  -- Simple Writer
 *
 * This writer just creates a file per packet thread and queues buffers
 * to be written to disk in a single output thread.
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
#define HAVE_ZSTD 1
#include "moloch.h"
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <zlib.h>
#ifdef HAVE_ZSTD
#include <zstd.h>
#endif
#include <math.h>
#include "openssl/rand.h"
#include "openssl/evp.h"

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

typedef enum {
    MOLOCH_COMPRESSION_NONE,
    MOLOCH_COMPRESSION_GZIP,
    MOLOCH_COMPRESSION_ZSTD
} MolochCompressionMode;

extern MolochConfig_t        config;
extern MolochPcapFileHdr_t   pcapFileHeader;
LOCAL  gboolean              localPcapIndex;
LOCAL  MolochCompressionMode compressionMode = MOLOCH_COMPRESSION_NONE;
LOCAL  gboolean              simpleShortHeader;
LOCAL  int                   simpleGzipLevel;

// Information about the current file being written to, all items that are constant per file should be here
typedef struct {
    EVP_CIPHER_CTX      *cipher_ctx;
    uint64_t             pos;
    uint64_t             blockStart;
    uint64_t             packetBytesWritten;
    uint32_t             packets;
    uint32_t             posInBlock;
    uint32_t             id;
    int                  fd;
    uint8_t              dek[256];
    z_stream             z_strm;
    uint8_t              thread;
#ifdef HAVE_ZSTD
    ZSTD_CStream        *zstd_strm;
    ZSTD_outBuffer       zstd_out;
    ZSTD_inBuffer        zstd_in;
#endif
} MolochSimpleFile_t;

// Information about the current buffer being written to, there can be multiple buffers per file
// NOTE this points to the file structure, kind of backwards
typedef struct molochsimple {
    struct molochsimple *simple_next, *simple_prev;
    char                *buf;     // mmap buffer, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN
    MolochSimpleFile_t  *file;
    uint32_t             bufpos;  // Where in buf we are writing to
    uint8_t              closing; // This is the last block, close file when done
} MolochSimple_t;

typedef struct {
    struct molochsimple *simple_next, *simple_prev;
    int                  simple_count;
    MOLOCH_LOCK_EXTERN(lock);
} MolochSimpleHead_t;

LOCAL  MolochSimpleHead_t simpleQ;
LOCAL  MOLOCH_LOCK_DEFINE(simpleQ);
LOCAL  MOLOCH_COND_DEFINE(simpleQ);

enum MolochSimpleMode { MOLOCH_SIMPLE_NORMAL, MOLOCH_SIMPLE_XOR2048, MOLOCH_SIMPLE_AES256CTR};

LOCAL MolochSimple_t        *currentInfo[MOLOCH_MAX_PACKET_THREADS];
LOCAL MolochSimpleHead_t     freeList[MOLOCH_MAX_PACKET_THREADS];
LOCAL uint32_t               pageSize;
LOCAL enum MolochSimpleMode  simpleMode;
LOCAL int                    simpleMaxQ;
LOCAL const EVP_CIPHER      *cipher;
LOCAL int                    openOptions;
LOCAL struct timeval         lastSave[MOLOCH_MAX_PACKET_THREADS];
LOCAL struct timeval         fileAge[MOLOCH_MAX_PACKET_THREADS];
LOCAL uint32_t               firstPacket[MOLOCH_MAX_PACKET_THREADS];

#define INDEX_FILES_CACHE_SIZE (MOLOCH_MAX_PACKET_THREADS-1)
struct {
    int64_t  fileNum;
    FILE    *fp;
} indexFiles[MOLOCH_MAX_PACKET_THREADS][INDEX_FILES_CACHE_SIZE];

/*
 * Compression design inspired by Philip Gladstone and others.
 * A compressed file is made up of compressed blocks.
 * You can only start reading a compressed file at the beginning of a block.
 * Blocks are variable sized, with the max UNCOMPRESED data per block
 * controlled by simpleGzipBlockSize.
 * uncompressedBits is calculated so it can hold simpleGzipBlockSize.
 * The file pos for each packet is made of two parts
 *   X the location in the file of the start of the compress block, which
 *   is shifted uncompressedBits
 *   Y the location inside the uncompressed block of the packet start
 * A larger simpleGzipBlockSize leads to better compression but slower read time.
 */
LOCAL int      uncompressedBits;    // Number of bits used in filepos to store location in block
LOCAL uint32_t simpleGzipBlockSize; // Max data that we try and compress, can be represented by uncompressedBits

/******************************************************************************/
LOCAL uint32_t writer_simple_queue_length()
{
    return DLL_COUNT(simple_, &simpleQ);
}
/******************************************************************************/
/*
 * Get a new buffer structure, and copy the old file pointer if needed
 */
LOCAL MolochSimple_t *writer_simple_alloc(int thread, MolochSimple_t *previous)
{
    MolochSimple_t *info;

    MOLOCH_LOCK(freeList[thread].lock);
    DLL_POP_HEAD(simple_, &freeList[thread], info);
    MOLOCH_UNLOCK(freeList[thread].lock);

    if (!info) {
        info = MOLOCH_TYPE_ALLOC0(MolochSimple_t);
        info->buf = mmap (0, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN, PROT_READ|PROT_WRITE, MAP_ANON|MAP_PRIVATE, -1, 0);
        if (unlikely(info->buf == MAP_FAILED)) {
            LOGEXIT("ERROR - MMap failure in writer_simple_alloc, %d: %s", errno, strerror(errno));
        }
    } else {
        info->bufpos = 0;
        info->closing = 0;
    }

    if (previous) {
        info->file = previous->file;
    }
    return info;
}
/******************************************************************************/
LOCAL void writer_simple_free(MolochSimple_t *info)
{
    int thread = info->file->thread;

    if (info->closing) {
        switch(simpleMode) {
        case MOLOCH_SIMPLE_NORMAL:
            break;
        case MOLOCH_SIMPLE_XOR2048:
            break;
        case MOLOCH_SIMPLE_AES256CTR:
            EVP_CIPHER_CTX_free(info->file->cipher_ctx);
            break;
        }
        switch(compressionMode) {
        case MOLOCH_COMPRESSION_GZIP:
            deflateEnd(&info->file->z_strm);
            break;
        default:
            break;
        }
        MOLOCH_TYPE_FREE(MolochSimpleFile_t, info->file);
    }
    info->file = 0;

    if (DLL_COUNT(simple_, &freeList[thread]) < 16) {
        MOLOCH_LOCK(freeList[thread].lock);
        DLL_PUSH_TAIL(simple_, &freeList[thread], info);
        MOLOCH_UNLOCK(freeList[thread].lock);
    } else {
        munmap(info->buf, config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN);
        MOLOCH_TYPE_FREE(MolochSimple_t, info);
    }
}

/******************************************************************************/
LOCAL void writer_simple_process_buf(int thread, int closing)
{
    MolochSimple_t *info = currentInfo[thread];
    static uint32_t lastError;

    info->closing = closing;
    if (!closing) {
        // Round down to nearest pagesize
        int writeSize = (info->bufpos/pageSize) * pageSize;

        // Create next buffer
        MolochSimple_t *ninfo = currentInfo[thread] = writer_simple_alloc(thread, info);

        // Copy what we aren't going to write to next buffer
        memcpy(ninfo->buf, info->buf + writeSize, info->bufpos - writeSize);
        ninfo->bufpos = info->bufpos - writeSize;

        switch(compressionMode) {
        case MOLOCH_COMPRESSION_GZIP:
            // Start the gzip buffer after what we copied from previous buffer.
            ninfo->file->z_strm.next_out = (Bytef *) ninfo->buf + ninfo->bufpos;
            ninfo->file->z_strm.avail_out = config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN - ninfo->bufpos;
            break;
        default:
            break;
        }

        // Set what we are going to write
        info->bufpos = writeSize;

        switch(compressionMode) {
        case MOLOCH_COMPRESSION_GZIP:
            info->file->pos += info->bufpos;
            break;
        default:
            break;
        }

    } else {
        switch(compressionMode) {
        case MOLOCH_COMPRESSION_GZIP:
            deflate(&info->file->z_strm, Z_FINISH);
            info->bufpos = (char *)info->file->z_strm.next_out - info->buf;
            info->file->pos += info->bufpos;
            break;
        default:
            break;
        }
        currentInfo[thread] = NULL; // This will cause a new file to be allocated on next packet
    }

    // Send to write q to actually write to disk
    MOLOCH_LOCK(simpleQ);
    gettimeofday(&lastSave[thread], NULL);
    DLL_PUSH_TAIL(simple_, &simpleQ, info);
    if (DLL_COUNT(simple_, &simpleQ) > 100 && lastSave[thread].tv_sec > lastError + 60) {
        lastError = lastSave[thread].tv_sec;
        LOG("WARNING - Disk Q of %d is too large, check the Arkime FAQ about (https://arkime.com/faq#why-am-i-dropping-packets) testing disk speed", DLL_COUNT(simple_, &simpleQ));
    }
    MOLOCH_COND_SIGNAL(simpleQ);
    MOLOCH_UNLOCK(simpleQ);
}
/******************************************************************************/
LOCAL void writer_simple_encrypt_key(char *kekId, uint8_t *dek, int deklen, char *outkeyhex)
{

    uint8_t ciphertext[1024];
    int     len, ciphertext_len;
    uint8_t kek[EVP_MAX_KEY_LENGTH];
    uint8_t kekiv[EVP_MAX_IV_LENGTH];

    if (!kekId)
        CONFIGEXIT("simpleKEKId must be set");

   char *kekstr = moloch_config_section_str(NULL, "keks", kekId, NULL);
   if (!kekstr)
       CONFIGEXIT("No kek with id '%s' found in keks config section", kekId);

    EVP_BytesToKey(EVP_aes_192_cbc(), EVP_md5(), NULL, (uint8_t *)kekstr, strlen(kekstr), 1, kek, kekiv);
    g_free(kekstr);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_192_cbc(), NULL, kek, kekiv);
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, dek, deklen))
        LOGEXIT("ERROR - Encrypting key failed");
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    moloch_sprint_hex_string(outkeyhex, ciphertext, ciphertext_len);
}
/******************************************************************************/
LOCAL char *writer_simple_get_kekId ()
{
    char *kek = moloch_config_str(NULL, "simpleKEKId", NULL);

    if(!kek) {
        return NULL;
    }

    if (!kek[0]) {
        g_free(kek);
        return NULL;
    }

    if (strchr(kek, '%') == 0) {
        return kek;
    }

    struct timeval now;
    gettimeofday(&now, NULL);
    struct tm tmp;
    gmtime_r(&now.tv_sec, &tmp);

    char okek[2000];
    int i,j;

    for (i = j = 0; kek[i] && j+2 < 1999; i++) {
        if (kek[i] != '%') {
            okek[j] = kek[i];
            j++;
            continue;
        }
        i++;
        switch(kek[i]) {
        case 'y':
            okek[j] = '0' + (tmp.tm_year % 100)/10;
            okek[j+1] = '0' + tmp.tm_year%10;
            j += 2;
            break;
        case 'm':
            okek[j] = '0' + (tmp.tm_mon+1)/10;
            okek[j+1] = '0' + (tmp.tm_mon+1)%10;
            j += 2;
            break;
        case 'd':
            okek[j] = '0' + tmp.tm_mday/10;
            okek[j+1] = '0' + tmp.tm_mday%10;
            j += 2;
            break;
        case 'H':
            okek[j] = '0' + tmp.tm_hour/10;
            okek[j+1] = '0' + tmp.tm_hour%10;
            j += 2;
            break;
        case 'N':
        {
            int namelen = strlen(config.nodeName);
            int bufboundary = j + namelen;

            if(bufboundary >= (int) sizeof(okek)) {
                LOGEXIT("ERROR - node name '%s' is too long", config.nodeName);
            }
            memcpy(okek+j, config.nodeName, namelen);
            j = bufboundary;
            break;
        }
        }
    }
    g_free(kek);
    return g_strndup(okek, j);
}
/******************************************************************************/
LOCAL void writer_simple_write_output(MolochSimple_t *info, const unsigned char *data, int len)
{
    switch(compressionMode) {
    case MOLOCH_COMPRESSION_NONE:
        memcpy(info->buf + info->bufpos, data, len);
        info->bufpos += len;
        info->file->pos += len;
        break;

    case MOLOCH_COMPRESSION_GZIP:
        info->file->z_strm.next_in = (Bytef *)data;
        info->file->z_strm.avail_in = len;

        while (info->file->z_strm.avail_in != 0) {
            deflate(&info->file->z_strm, Z_NO_FLUSH);
        }
        info->file->posInBlock += len;
        info->bufpos = (char *)info->file->z_strm.next_out - info->buf;
        break;
    default:
        break;
    }

    info->file->packetBytesWritten += len;
}
/******************************************************************************/
LOCAL void writer_simple_gzip_make_new_block(MolochSimple_t *info)
{
    deflate(&info->file->z_strm, Z_FULL_FLUSH);
    info->bufpos = (char *)info->file->z_strm.next_out - info->buf;
    info->file->blockStart = info->file->z_strm.total_out;
    info->file->posInBlock = 0;
}
/******************************************************************************/
LOCAL void writer_simple_write(const MolochSession_t * const session, MolochPacket_t * const packet)
{
    MolochSimple_t *info;

    if (DLL_COUNT(simple_, &simpleQ) > simpleMaxQ) {
        static uint32_t lastError;
        static uint32_t notSaved;
        packet->writerFilePos = 0;
        notSaved++;
        if (packet->ts.tv_sec > lastError + 60) {
            lastError = packet->ts.tv_sec;
            LOG("WARNING - Disk Q of %d is too large and exceed simpleMaxQ setting so not saving %u packets. Check the Arkime FAQ about (https://arkime.com/faq#why-am-i-dropping-packets) testing disk speed", DLL_COUNT(simple_, &simpleQ), notSaved);
        }
        return;
    }

    int thread = session->thread;

    // Need to open a new file
    if (!currentInfo[thread]) {
        char  dekhex[1024];
        char *name = 0;
        char *kekId;
        char *packetPosEncoding = MOLOCH_VAR_ARG_SKIP;
        char *uncompressedBitsArg = MOLOCH_VAR_ARG_SKIP;
        char  indexFilename[1024];

        indexFilename[0] = 0;
        if (localPcapIndex) {
            packetPosEncoding = "localIndex";
            snprintf(indexFilename, sizeof(indexFilename), "%s/%s-#NUM#.index", config.pcapDir[0], config.nodeName);
        } else if (config.gapPacketPos) {
            packetPosEncoding = "gap0";
        }

        info = currentInfo[thread] = writer_simple_alloc(thread, NULL);
        info->file = MOLOCH_TYPE_ALLOC0(MolochSimpleFile_t);
        info->file->thread = thread;

        switch(compressionMode) {
        case MOLOCH_COMPRESSION_GZIP:
            uncompressedBitsArg = (gpointer)(long)uncompressedBits;

            info->file->z_strm.next_out = (Bytef *) info->buf;
            info->file->z_strm.avail_out = config.pcapWriteSize + MOLOCH_PACKET_MAX_LEN;
            deflateInit2(&info->file->z_strm, simpleGzipLevel, Z_DEFLATED, 16 + 15, 9, Z_DEFAULT_STRATEGY);
            break;
        default:
            break;
        }

        switch(simpleMode) {
        case MOLOCH_SIMPLE_NORMAL:
            if (simpleShortHeader)
                name = ".arkime";
            else if (compressionMode == MOLOCH_COMPRESSION_GZIP)
                name = ".pcap.gz";
            else
                name = ".pcap";
            name = moloch_db_create_file_full(packet->ts.tv_sec, name, 0, 0, &info->file->id,
                                              "packetPosEncoding", packetPosEncoding,
                                              "uncompressedBits", uncompressedBitsArg,
                                              "indexFilename", indexFilename[0] ? indexFilename : MOLOCH_VAR_ARG_SKIP,
                                              (char *)NULL);
            break;
        case MOLOCH_SIMPLE_XOR2048:
            name = ".arkime";
            kekId = writer_simple_get_kekId();
            RAND_bytes(info->file->dek, 256);
            writer_simple_encrypt_key(kekId, info->file->dek, 256, dekhex);
            name = moloch_db_create_file_full(packet->ts.tv_sec, name, 0, 0, &info->file->id,
                                              "encoding", "xor-2048",
                                              "dek", dekhex,
                                              "kekId", kekId,
                                              "packetPosEncoding", packetPosEncoding,
                                              "uncompressedBits", uncompressedBitsArg,
                                              "indexFilename", indexFilename[0] ? indexFilename : MOLOCH_VAR_ARG_SKIP,
                                              (char *)NULL);
            g_free(kekId);
            break;
        case MOLOCH_SIMPLE_AES256CTR: {
            info->file->cipher_ctx = EVP_CIPHER_CTX_new();
            name = ".arkime";
            uint8_t dek[32];
            uint8_t iv[16];
            char    ivhex[33];
            RAND_bytes(iv, 12);
            RAND_bytes(dek, 32);
            memset(iv+12, 0, 4);
            kekId = writer_simple_get_kekId();
            writer_simple_encrypt_key(kekId, dek, 32, dekhex);
            moloch_sprint_hex_string(ivhex, iv, 12);
            EVP_EncryptInit(info->file->cipher_ctx, cipher, dek, iv);
            name = moloch_db_create_file_full(packet->ts.tv_sec, name, 0, 0, &info->file->id,
                                              "encoding", "aes-256-ctr",
                                              "iv", ivhex,
                                              "dek", dekhex,
                                              "kekId", kekId,
                                              "packetPosEncoding", packetPosEncoding,
                                              "uncompressedBits", uncompressedBitsArg,
                                              "indexFilename", indexFilename[0] ? indexFilename : MOLOCH_VAR_ARG_SKIP,
                                              (char *)NULL);
            g_free(kekId);
            break;
        }
        default:
            CONFIGEXIT("Unknown simpleMode %d", simpleMode);
        }

        /* If offline pcap honor umask, otherwise disable other RW */
        if (config.pcapReadOffline) {
            currentInfo[thread]->file->fd = open(name,  openOptions, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        } else {
            currentInfo[thread]->file->fd = open(name,  openOptions, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        }
        if (currentInfo[thread]->file->fd < 0) {
            LOGEXIT("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d) -- You may need to check directory permissions or set pcapWriteMethod=simple-nodirect in config.ini file.  See https://arkime.com/settings#pcapwritemethod", name, strerror(errno), errno);
        }

        if (simpleShortHeader) {
            firstPacket[thread] = packet->ts.tv_sec - 60; // Allow slightly out of sync clocks
            MolochPcapFileHdr_t   pcapFileHeader2;
            memcpy(&pcapFileHeader2, &pcapFileHeader, 24);
            pcapFileHeader2.magic = 0xa1b2c3d5;
            pcapFileHeader2.thiszone = firstPacket[thread];
            writer_simple_write_output(info, (unsigned char *)&pcapFileHeader2, 20);
        } else {
            writer_simple_write_output(info, (unsigned char *)&pcapFileHeader, 20);
        }

        uint32_t linktype = moloch_packet_dlt_to_linktype(pcapFileHeader.dlt);
        writer_simple_write_output(info, (unsigned char *)&linktype, 4);
        if (config.debug)
            LOG("opened %d %s %d", thread, name, info->file->fd);
        g_free(name);

        // Make a new block for start of packets
        if (compressionMode == MOLOCH_COMPRESSION_GZIP)
            writer_simple_gzip_make_new_block(info);
        gettimeofday(&fileAge[thread], NULL);
    } else {
        info = currentInfo[thread];
    }

    packet->writerFileNum = info->file->id;

    if (compressionMode == MOLOCH_COMPRESSION_GZIP) {
        if (info->file->posInBlock >= simpleGzipBlockSize) {
            writer_simple_gzip_make_new_block(info);
        }

        packet->writerFilePos = (info->file->blockStart << uncompressedBits) + info->file->posInBlock;
    } else {
        packet->writerFilePos = info->file->pos;
    }

    info->file->packets++;
    if (simpleShortHeader) {
        char header[6];
        // LLLL LLLL LLLL LLLL
        memcpy(header, &packet->pktlen, 2);

        // SSSS SSSS SSSS MMMM MMMM MMMM MMMM MMMM
        uint32_t t;
        if (firstPacket[thread] > packet->ts.tv_sec) {
            LOG("WARNING - timing moving backwards, simpleShortHeader should be disabled");
            // Time stamp is too early, just prented its at firstPacket time
            t = packet->ts.tv_usec;
        } else {
            t = ((packet->ts.tv_sec - firstPacket[thread]) << 20) | packet->ts.tv_usec;
        }

        memcpy(header+2, &t, 4);

        writer_simple_write_output(info, (unsigned char *)&header, 6);
    } else {
        struct moloch_pcap_sf_pkthdr hdr;

        hdr.ts.tv_sec  = packet->ts.tv_sec;
        hdr.ts.tv_usec = packet->ts.tv_usec;
        hdr.caplen     = packet->pktlen;
        hdr.pktlen     = packet->pktlen;
        writer_simple_write_output(info, (unsigned char *)&hdr, 16);
    }
    writer_simple_write_output(info, packet->pkt, packet->pktlen);

    if (info->bufpos > config.pcapWriteSize) {
        writer_simple_process_buf(thread, 0);
    } else if (info->file->packetBytesWritten >= config.maxFileSizeB) {
        writer_simple_process_buf(thread, 1);
    }
}
/******************************************************************************/
LOCAL void *writer_simple_thread(void *UNUSED(arg))
{
    MolochSimple_t *info;

    if (config.debug)
        LOG("THREAD %p", (gpointer)pthread_self());

    while (1) {
        MOLOCH_LOCK(simpleQ);
        while (DLL_COUNT(simple_, &simpleQ) == 0) {
            MOLOCH_COND_WAIT(simpleQ);
        }
        DLL_POP_HEAD(simple_, &simpleQ, info);
        MOLOCH_UNLOCK(simpleQ);

        uint32_t pos = 0;
        uint32_t total = info->bufpos;
        if (info->closing) {
            // Round up to next page size
            if (total % pageSize != 0)
                total = ((total/pageSize)+1)*pageSize;
        }

        switch(simpleMode) {
        case MOLOCH_SIMPLE_NORMAL:
            break;
        case MOLOCH_SIMPLE_XOR2048: {
            uint32_t i;
            for (i = 0; i < total; i++)
                info->buf[i] ^= info->file->dek[i % 256];
            break;
        }
        case MOLOCH_SIMPLE_AES256CTR: {
            int outl;
            if (!EVP_EncryptUpdate(info->file->cipher_ctx, (uint8_t *)info->buf, &outl, (uint8_t *)info->buf, total))
                LOGEXIT("ERROR - Encrypting data failed");
            if ((int)total != outl)
                LOGEXIT("ERROR - Encryption in (%u) and out (%d) didn't match", total, outl);
            break;
        }
        }

        while (pos < total) {
            int len = write(info->file->fd, info->buf + pos, total - pos);
            if (len >= 0) {
                pos += len;
            } else {
                LOGEXIT("ERROR - writing %d %s", len, strerror(errno));
            }
        }
        if (info->closing) {
            if (ftruncate(info->file->fd, info->file->pos) < 0 && config.debug)
                LOG("Truncate failed");
            close(info->file->fd);
            moloch_db_update_filesize(info->file->id, info->file->pos, info->file->packetBytesWritten, info->file->packets);
        }

        writer_simple_free(info);
    }
    return NULL;
}
/******************************************************************************/
LOCAL void writer_simple_exit()
{
    int thread;

    for (thread = 0; thread < config.packetThreads; thread++) {
        if (currentInfo[thread]) {
            writer_simple_process_buf(thread, 1);
        }
    }

    // Pause the main thread until our thread finishes
    while (writer_simple_queue_length() > 0) {
        usleep(10000);
    }

    for (thread = 0; thread < config.packetThreads; thread++) {
        for (int p = 0; p < INDEX_FILES_CACHE_SIZE; p++) {
            if (indexFiles[thread][p].fp) {
                fclose(indexFiles[thread][p].fp);
                indexFiles[thread][p].fp = 0;
            }
        }
    }
}
/******************************************************************************/
// Called inside each packet thread
LOCAL void writer_simple_check(MolochSession_t *session, void *UNUSED(uw1), void *UNUSED(uw2))
{
    struct timeval now;
    gettimeofday(&now, NULL);

    // No data or not enough bytes, reset the time
    if (!currentInfo[session->thread] || currentInfo[session->thread]->bufpos < (uint32_t)pageSize) {
        lastSave[session->thread] = now;
        return;
    }

    if (config.maxFileTimeM > 0 && now.tv_sec - fileAge[session->thread].tv_sec >= config.maxFileTimeM*60) {
        writer_simple_process_buf(session->thread, 1);
        return;
    }

    // Last add must be 10 seconds ago and have more then pageSize bytes
    if (now.tv_sec - lastSave[session->thread].tv_sec < 10)
        return;

    // Don't force writes for gzip for now
    if (compressionMode != MOLOCH_COMPRESSION_GZIP) {
        writer_simple_process_buf(session->thread, 0);
    }
}
/******************************************************************************/
/* Called in the main thread.  Check all the timestamps, and if out of date
 * schedule something in each writer thread to do the partial write since there
 * is no locks around buffering.
 */
LOCAL gboolean writer_simple_check_gfunc (gpointer UNUSED(user_data))
{
    struct timeval now;
    gettimeofday(&now, NULL);

    MOLOCH_LOCK(simpleQ);
    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        if (now.tv_sec - lastSave[thread].tv_sec >= 10) {
            moloch_session_add_cmd_thread(thread, NULL, NULL, writer_simple_check);
        }
    }
    MOLOCH_UNLOCK(simpleQ);

    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
FILE *writer_simple_get_index(int thread, int64_t fileNum)
{
    const int p = fileNum % INDEX_FILES_CACHE_SIZE;

    if (indexFiles[thread][p].fp) {
        // This is the fileNum we are looking for
        if (indexFiles[thread][p].fileNum == fileNum) {
            return indexFiles[thread][p].fp;
        }

        // This isn't it, close the old one
        fclose(indexFiles[thread][p].fp);
    }

    char     filename[1024];
    snprintf(filename, sizeof(filename), "%s/%s-%" PRId64 ".index", config.pcapDir[0], config.nodeName, fileNum);

    if ((indexFiles[thread][p].fp = fopen(filename, "a")) == NULL) {
        LOGEXIT("ERROR - Couldn't open file %s", filename);
    }

    if (!config.pcapReadOffline) {
        fchmod(fileno(indexFiles[thread][p].fp), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    }

    return indexFiles[thread][p].fp;
}
/******************************************************************************/
void writer_simple_index (MolochSession_t * session)
{
    uint8_t  buf[0xffff*5];
    BSB      bsb;
    int      files = 0;
    int64_t  filePos[1024];

    BSB_INIT(bsb, buf, sizeof(buf));

    FILE *fp = 0;
    uint64_t last = 0;
    uint64_t lastgap = 0;
    for(guint i = 0; i < session->filePosArray->len; i++) {
        int64_t packetPos = (int64_t)g_array_index(session->filePosArray, int64_t, i);
        if (packetPos < 0) {
            if (fp) {
                filePos[(files-1)*3 + 2] = BSB_LENGTH(bsb);
                fwrite(buf, BSB_LENGTH(bsb), 1, fp);
                last = 0;
                lastgap = 0;
            }
            fp = writer_simple_get_index(session->thread, -packetPos);

            filePos[files*3] = packetPos;      // Which file
            filePos[files*3 + 1] = ftell(fp);  // Where in index file
            files++;

            BSB_INIT(bsb, buf, sizeof(buf));
        } else {
            uint64_t val = packetPos - last;
            if (val == lastgap) {
                val = 0;
            } else {
                lastgap = val;
            }
            last = packetPos;

            if (val <= 0x7f) {
                BSB_EXPORT_u08(bsb, 0x80 | val);
                continue;
            }
            BSB_EXPORT_u08(bsb, (val & 0x7f));

            if (val <= 0x3fff) {
                BSB_EXPORT_u08(bsb, 0x80 | ((val >> 7) & 0x7f));
                continue;
            }
            BSB_EXPORT_u08(bsb, ((val >> 7) & 0x7f));

            if (val <= 0x1fffff) {
                BSB_EXPORT_u08(bsb, 0x80 | ((val >> 14) & 0x7f));
                continue;
            }
            BSB_EXPORT_u08(bsb, ((val >> 14) & 0x7f));

            if (val <= 0x0fffffff) {
                BSB_EXPORT_u08(bsb, 0x80 | ((val >> 21) & 0x7f));
                continue;
            }
            BSB_EXPORT_u08(bsb, ((val >> 21) & 0x7f));

            if (val <= 0x07ffffffff) {
                BSB_EXPORT_u08(bsb, 0x80 | ((val >> 28) & 0x7f));
                continue;
            }
            BSB_EXPORT_u08(bsb, ((val >> 28) & 0x7f));

            if (val <= 0x03ffffffffff) {
                BSB_EXPORT_u08(bsb, 0x80 | ((val >> 35) & 0x7f));
                continue;
            }
            BSB_EXPORT_u08(bsb, ((val >> 35) & 0x7f));

            if (val <= 0x01ffffffffffff) {
                BSB_EXPORT_u08(bsb, 0x80 | ((val >> 42) & 0x7f));
                continue;
            }
            BSB_EXPORT_u08(bsb, ((val >> 42) & 0x7f));

            // support up to 0xffffffffffffffUL (2^56-1)
            BSB_EXPORT_u08(bsb, 0x80 | ((val >> 49) & 0x7f));
        }
    }

    if (fp) {
        filePos[(files-1)*3 + 2] = BSB_LENGTH(bsb);
        fwrite(buf, BSB_LENGTH(bsb), 1, fp);
    }

    g_array_set_size(session->filePosArray, 0);
    g_array_append_vals(session->filePosArray, filePos, files*3);
}
/******************************************************************************/
void writer_simple_init(char *name)
{
    moloch_writer_queue_length = writer_simple_queue_length;
    moloch_writer_exit         = writer_simple_exit;
    moloch_writer_write        = writer_simple_write;

    simpleMaxQ = moloch_config_int(NULL, "simpleMaxQ", 2000, 50, 0xffff);
    simpleGzipLevel = moloch_config_int(NULL, "simpleGzipLevel", 5, 1, 9);
    char *mode = moloch_config_str(NULL, "simpleEncoding", NULL);

    simpleGzipBlockSize = moloch_config_int(NULL, "simpleGzipBlockSize", 32000, 0, 0xfffff);
    if (simpleGzipBlockSize > 0) {
        compressionMode = MOLOCH_COMPRESSION_GZIP;
        if (simpleGzipBlockSize < 8192) {
            simpleGzipBlockSize = 8191;
            LOG ("INFO: Reseting simpleGzipBlockSize to %u, the minimum value", simpleGzipBlockSize);
        }
        uncompressedBits = ceil(log2(simpleGzipBlockSize));

        // simpleGzipBlockSize can't be a power of 2
        if ((uint32_t)pow(2, uncompressedBits) == simpleGzipBlockSize)
            simpleGzipBlockSize--;

        if (simpleGzipBlockSize > config.pcapWriteSize) {
            config.pcapWriteSize = simpleGzipBlockSize + 1;
            LOG ("INFO: Reseting pcapWriteSize to %u, so it is larger than simpleGzipBlockSize", config.pcapWriteSize);
        }

        if (config.debug)
            LOG("Will gzip - blocksize: %u bits: %d", simpleGzipBlockSize, uncompressedBits);
    }

    if (mode == NULL || !mode[0]) {
    } else if (strcmp(mode, "aes-256-ctr") == 0) {
        simpleMode = MOLOCH_SIMPLE_AES256CTR;
        cipher = EVP_aes_256_ctr();
        if (config.maxFileSizeB > 64*1024LL*1024LL*1024LL) {
            LOG ("INFO: Reseting maxFileSizeG since %lf is greater then the max 64G in aes-256-ctr mode", config.maxFileSizeG);
            config.maxFileSizeG = 64.0;
            config.maxFileSizeB = 64LL*1024LL*1024LL*1024LL;
        }
    } else if (strcmp(mode, "xor-2048") == 0) {
        LOG("WARNING - simpleEncoding of xor-2048 is NOT actually secure");
        simpleMode = MOLOCH_SIMPLE_XOR2048;
    } else {
        CONFIGEXIT("Unknown simpleEncoding '%s'", mode);
    }

    if (mode) {
        g_free(mode);
    }

    // Since we are doing direct IO must be a multiple of pagesize;
    pageSize = getpagesize();
    if (config.pcapWriteSize % pageSize != 0) {
        config.pcapWriteSize = ((config.pcapWriteSize + pageSize - 1) / pageSize) * pageSize;
        LOG ("INFO: Reseting pcapWriteSize to %u since it must be a multiple of %u", config.pcapWriteSize, pageSize);
    }

    openOptions = O_NOATIME | O_WRONLY | O_CREAT | O_TRUNC;
    if (strcmp(name, "simple") == 0) {
#ifdef O_DIRECT
        openOptions |= O_DIRECT;
#else
        LOG("No O_DIRECT defined, skipping");
#endif
    } else {
        LOG("Not using O_DIRECT by config");
    }

    config.gapPacketPos = moloch_config_boolean(NULL, "gapPacketPos", TRUE);

    simpleShortHeader = moloch_config_boolean(NULL, "simpleShortHeader", FALSE);
    if (simpleShortHeader && config.maxFileTimeM > 60) {
        config.maxFileTimeM = 60;
        LOG ("INFO: Reseting maxFileTimeM to 60 since using simpleShortHeader");
    }

    localPcapIndex = moloch_config_boolean(NULL, "localPcapIndex", FALSE);
    if (localPcapIndex) {
        if (config.pcapDir[1]) {
            LOG("WARNING - Will always use first pcap directory for local index");
        }

        if (compressionMode == MOLOCH_COMPRESSION_GZIP) {
            config.maxFileSizeB = MIN(config.maxFileSizeB, 0xffffffffffffffUL >> (uncompressedBits + 1));
        }

        config.gapPacketPos = FALSE;
        moloch_writer_index = writer_simple_index;
    }

    DLL_INIT(simple_, &simpleQ);

    struct timeval now;
    gettimeofday(&now, NULL);

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        lastSave[thread] = now;
        fileAge[thread] = now;
        DLL_INIT(simple_, &freeList[thread]);
        MOLOCH_LOCK_INIT(freeList[thread].lock);
    }

    g_thread_unref(g_thread_new("moloch-simple", &writer_simple_thread, NULL));

    g_timeout_add_seconds(1, writer_simple_check_gfunc, 0);
}
