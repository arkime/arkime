/******************************************************************************/
/* writer-simple.c  -- Simple Writer Plugin
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
#include "moloch.h"
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#ifdef __FreeBSD__
#include <sys/stat.h>
#endif
#include "openssl/rand.h"
#include "openssl/evp.h"

#ifndef O_NOATIME
#define O_NOATIME 0
#endif

extern MolochConfig_t        config;
extern MolochPcapFileHdr_t   pcapFileHeader;
typedef struct {
    EVP_CIPHER_CTX      *cipher_ctx;
    uint64_t             pos;
    uint32_t             id;
    int                  fd;
    uint8_t              dek[256];
} MolochSimpleFile_t;

typedef struct molochsimple {
    struct molochsimple *simple_next, *simple_prev;
    char                *buf;
    MolochSimpleFile_t  *file;
    uint32_t             bufpos;
    uint8_t              closing;
    uint8_t              thread;
} MolochSimple_t;

typedef struct {
    struct molochsimple *simple_next, *simple_prev;
    int                  simple_count;
    MOLOCH_LOCK_EXTERN(lock);
} MolochSimpleHead_t;

static MolochSimpleHead_t simpleQ;
static MOLOCH_LOCK_DEFINE(simpleQ);
static MOLOCH_COND_DEFINE(simpleQ);

enum MolochSimpleMode { MOLOCH_SIMPLE_NORMAL, MOLOCH_SIMPLE_XOR2048, MOLOCH_SIMPLE_AES256CTR};

LOCAL MolochSimple_t        *currentInfo[MOLOCH_MAX_PACKET_THREADS];
LOCAL MolochSimpleHead_t     freeList[MOLOCH_MAX_PACKET_THREADS];
LOCAL int                    pageSize;
LOCAL enum MolochSimpleMode  simpleMode;
LOCAL char                  *simpleKEKId;
LOCAL uint8_t                simpleKEK[EVP_MAX_KEY_LENGTH];
LOCAL int                    simpleKEKLen;
LOCAL uint8_t                simpleIV[EVP_MAX_IV_LENGTH];
LOCAL const EVP_CIPHER      *cipher;
LOCAL int                    openOptions;

/******************************************************************************/
uint32_t writer_simple_queue_length()
{
    return DLL_COUNT(simple_, &simpleQ);
}
/******************************************************************************/
MolochSimple_t *writer_simple_alloc(int thread, MolochSimple_t *previous)
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
        info->thread = thread;
    } else {
        info->bufpos = 0;
        info->closing = 0;
    }

    if (previous) {
        info->file = previous->file;
    } else {
        info->file = MOLOCH_TYPE_ALLOC0(MolochSimpleFile_t);
        switch(simpleMode) {
        case MOLOCH_SIMPLE_NORMAL:
            break;
        case MOLOCH_SIMPLE_XOR2048:
            break;
        case MOLOCH_SIMPLE_AES256CTR:
            info->file->cipher_ctx = EVP_CIPHER_CTX_new();
            break;
        }
    }
    return info;
}
/******************************************************************************/
void writer_simple_free(MolochSimple_t *info)
{
    int thread = info->thread;

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
void writer_simple_process_buf(int thread, int closing)
{
    MolochSimple_t *info = currentInfo[thread];

    info->closing = closing;
    if (!closing) {
        currentInfo[thread] = writer_simple_alloc(thread, info);
        memcpy(currentInfo[thread]->buf, info->buf + config.pcapWriteSize, info->bufpos - config.pcapWriteSize);
        currentInfo[thread]->bufpos = info->bufpos - config.pcapWriteSize;
    } else {
        currentInfo[thread] = NULL;
    }
    MOLOCH_LOCK(simpleQ);
    DLL_PUSH_TAIL(simple_, &simpleQ, info);
    if ((DLL_COUNT(simple_, &simpleQ) % 100) == 0) {
        LOG("WARNING - Disk Q of %d is too large, check the Moloch FAQ about testing disk speed", DLL_COUNT(simple_, &simpleQ));
    }
    MOLOCH_COND_SIGNAL(simpleQ);
    MOLOCH_UNLOCK(simpleQ);
}
/******************************************************************************/
void writer_simple_encrypt_key(uint8_t *inkey, int inkeylen, char *outkeyhex)
{

    uint8_t ciphertext[1024];
    int len, ciphertext_len;

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_192_cbc(), NULL, simpleKEK, simpleIV);
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, inkey, inkeylen))
        LOGEXIT("Encrypting key failed");
    ciphertext_len = len;
    EVP_EncryptFinal_ex(ctx, ciphertext + len, &len);
    ciphertext_len += len;
    EVP_CIPHER_CTX_free(ctx);

    moloch_sprint_hex_string(outkeyhex, ciphertext, ciphertext_len);
}
/******************************************************************************/
struct pcap_timeval {
    int32_t tv_sec;		/* seconds */
    int32_t tv_usec;		/* microseconds */
};
struct pcap_sf_pkthdr {
    struct pcap_timeval ts;	/* time stamp */
    uint32_t caplen;		/* length of portion present */
    uint32_t pktlen;		/* length this packet (off wire) */
};
/******************************************************************************/
void writer_simple_write(const MolochSession_t * const session, MolochPacket_t * const packet)
{
    char    dekhex[1024];
    int thread = session->thread;
    char *name = 0;

    if (!currentInfo[thread]) {
        MolochSimple_t *info = currentInfo[thread] = writer_simple_alloc(thread, NULL);
        switch(simpleMode) {
        case MOLOCH_SIMPLE_NORMAL:
            name = moloch_db_create_file(packet->ts.tv_sec, NULL, 0, 0, &info->file->id);
            break;
        case MOLOCH_SIMPLE_XOR2048:
            RAND_bytes(info->file->dek, 256);
            writer_simple_encrypt_key(info->file->dek, 256, dekhex);
            name = moloch_db_create_file_full(packet->ts.tv_sec, NULL, 0, 0, &info->file->id,
                                              "encoding", "xor-2048",
                                              "dek", dekhex,
                                              "kekId", simpleKEKId,
                                              NULL);

            break;
        case MOLOCH_SIMPLE_AES256CTR: {
            uint8_t dek[32];
            uint8_t iv[16];
            char    ivhex[33];
            RAND_bytes(iv, 8);
            memset(iv+8, 0, 8);
            writer_simple_encrypt_key(dek, 32, dekhex);
            moloch_sprint_hex_string(ivhex, iv, 8);
            EVP_EncryptInit(info->file->cipher_ctx, cipher, dek, iv);
            name = moloch_db_create_file_full(packet->ts.tv_sec, NULL, 0, 0, &info->file->id,
                                              "encoding", "aes-256-ctr",
                                              "iv", ivhex,
                                              "dek", dekhex,
                                              "kekId", simpleKEKId,
                                              NULL);
            break;
        }
        default:
            LOGEXIT("Unknown simpleMode %d", simpleMode);
        }

        currentInfo[thread]->file->fd = open(name,  openOptions, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        if (currentInfo[thread]->file->fd < 0) {
            LOGEXIT("ERROR - pcap open failed - Couldn't open file: '%s' with %s  (%d)", name, strerror(errno), errno);
        }
        info->file->pos = currentInfo[thread]->bufpos = 24;
        memcpy(info->buf, &pcapFileHeader, 24);
        if (config.debug)
            LOG("opened %d %s %d", thread, name, info->file->fd);
        g_free(name);
    }

    packet->writerFileNum = currentInfo[thread]->file->id;
    packet->writerFilePos = currentInfo[thread]->file->pos;

    struct pcap_sf_pkthdr hdr;

    hdr.ts.tv_sec  = packet->ts.tv_sec;
    hdr.ts.tv_usec = packet->ts.tv_usec;
    hdr.caplen     = packet->pktlen;
    hdr.pktlen     = packet->pktlen;

    memcpy(currentInfo[thread]->buf+currentInfo[thread]->bufpos, &hdr, 16);
    currentInfo[thread]->bufpos += 16;
    memcpy(currentInfo[thread]->buf+currentInfo[thread]->bufpos, packet->pkt, packet->pktlen);
    currentInfo[thread]->bufpos += packet->pktlen;
    currentInfo[thread]->file->pos += 16 + packet->pktlen;

    if (currentInfo[thread]->bufpos > config.pcapWriteSize) {
        writer_simple_process_buf(thread, 0);
    } else if (currentInfo[thread]->file->pos >= config.maxFileSizeB) {
        writer_simple_process_buf(thread, 1);
    }
}
/******************************************************************************/
void *writer_simple_thread(void *UNUSED(arg))
{
    MolochSimple_t *info;

    LOG("THREAD %p", (gpointer)pthread_self());
    while (1) {
        MOLOCH_LOCK(simpleQ);
        while (DLL_COUNT(simple_, &simpleQ) == 0) {
            MOLOCH_COND_WAIT(simpleQ);
        }
        DLL_POP_HEAD(simple_, &simpleQ, info);
        MOLOCH_UNLOCK(simpleQ);

        uint32_t pos = 0;
        uint32_t total;
        if (info->closing) {
            total = info->bufpos;
            if (total % pageSize != 0) {
                total = (total - (total % pageSize) + pageSize);
            }
        } else {
            total = config.pcapWriteSize;
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
                LOGEXIT("Encrypting data failed");
            if ((int)total != outl)
                LOGEXIT("Encryption in (%d) and out (%d) didn't match", total, outl);
            break;
        }
        }

        while (pos < total) {
            int len = write(info->file->fd, info->buf + pos, total - pos);
            if (len >= 0) {
                pos += len;
            } else {
                LOGEXIT("ERROR writing - %d %s", len, strerror(errno));
            }
        }
        if (info->closing) {
            ftruncate(info->file->fd, info->file->pos);
            close(info->file->fd);
            moloch_db_update_filesize(info->file->id, info->file->pos);
        }

        writer_simple_free(info);
    }
    return NULL;
}
/******************************************************************************/
void writer_simple_exit()
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
}
/******************************************************************************/
void writer_simple_init(char *name)
{
    moloch_writer_queue_length = writer_simple_queue_length;
    moloch_writer_exit         = writer_simple_exit;
    moloch_writer_write        = writer_simple_write;

    char *mode = moloch_config_str(NULL, "simpleEncoding", NULL);
    simpleKEKId = moloch_config_str(NULL, "simpleKEKId", NULL);

    if (simpleKEKId) {
       char *key = moloch_config_section_str(NULL, "keks", simpleKEKId, NULL);
       if (!key) {
           LOGEXIT("No kek with id '%s' found in keks config section", simpleKEKId);
       }

        simpleKEKLen = EVP_BytesToKey(EVP_aes_192_cbc(), EVP_md5(), NULL, (uint8_t *)key, strlen(key), 1, simpleKEK, simpleIV);
    }

    if (mode == NULL) {
    } else if (strcmp(mode, "aes-256-ctr") == 0) {
        if (!simpleKEKId)
            LOGEXIT("Must set simpleKEKId");
        simpleMode = MOLOCH_SIMPLE_AES256CTR;
        cipher = EVP_aes_256_ctr();
    } else if (strcmp(mode, "xor-2048") == 0) {
        if (!simpleKEKId)
            LOGEXIT("Must set simpleKEKId");
        LOG("WARNING - simpleEncoding of xor-2048 is not actually secure");
        simpleMode = MOLOCH_SIMPLE_XOR2048;
    } else {
        LOGEXIT("Unknown simpleEncoding '%s'", mode);
    }

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

    DLL_INIT(simple_, &simpleQ);

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        DLL_INIT(simple_, &freeList[thread]);
        MOLOCH_LOCK_INIT(freeList[thread].lock);
    }

    g_thread_new("moloch-simple", &writer_simple_thread, NULL);
}
