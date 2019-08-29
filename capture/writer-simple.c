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

/******************************************************************************/
LOCAL uint32_t writer_simple_queue_length()
{
    return DLL_COUNT(simple_, &simpleQ);
}
/******************************************************************************/
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
LOCAL void writer_simple_free(MolochSimple_t *info)
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
LOCAL void writer_simple_process_buf(int thread, int closing)
{
    MolochSimple_t *info = currentInfo[thread];
    static uint32_t lastError;

    info->closing = closing;
    if (!closing) {
        // Round down to nearest pagesize
        int writeSize = (info->bufpos/pageSize) * pageSize;

        // Create next buffer
        currentInfo[thread] = writer_simple_alloc(thread, info);

        // Copy what we aren't going to write to next buffer
        memcpy(currentInfo[thread]->buf, info->buf + writeSize, info->bufpos - writeSize);
        currentInfo[thread]->bufpos = info->bufpos - writeSize;

        // Set what we are going to write
        info->bufpos = writeSize;
    } else {
        currentInfo[thread] = NULL;
    }
    MOLOCH_LOCK(simpleQ);
    gettimeofday(&lastSave[thread], NULL);
    DLL_PUSH_TAIL(simple_, &simpleQ, info);
    if (DLL_COUNT(simple_, &simpleQ) > 100 && lastSave[thread].tv_sec > lastError + 60) {
        lastError = lastSave[thread].tv_sec;
        LOG("WARNING - Disk Q of %d is too large, check the Moloch FAQ about (https://molo.ch/faq#why-am-i-dropping-packets) testing disk speed", DLL_COUNT(simple_, &simpleQ));
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
        LOGEXIT("simpleKEKId must be set");

   char *kekstr = moloch_config_section_str(NULL, "keks", kekId, NULL);
   if (!kekstr)
       LOGEXIT("No kek with id '%s' found in keks config section", kekId);

    EVP_BytesToKey(EVP_aes_192_cbc(), EVP_md5(), NULL, (uint8_t *)kekstr, strlen(kekstr), 1, kek, kekiv);
    g_free(kekstr);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_192_cbc(), NULL, kek, kekiv);
    if (!EVP_EncryptUpdate(ctx, ciphertext, &len, dek, deklen))
        LOGEXIT("Encrypting key failed");
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
            memcpy(okek+j, config.nodeName, strlen(config.nodeName));
            j += strlen(config.nodeName);
            break;
        }
    }
    g_free(kek);
    return g_strndup(okek, j);
}
/******************************************************************************/
LOCAL void writer_simple_write(const MolochSession_t * const session, MolochPacket_t * const packet)
{
    static uint32_t lastError;
    static uint32_t notSaved;

    if (DLL_COUNT(simple_, &simpleQ) > simpleMaxQ) {
        packet->writerFilePos = 0;
        notSaved++;
        if (packet->ts.tv_sec > lastError + 60) {
            lastError = packet->ts.tv_sec;
            LOG("WARNING - Disk Q of %d is too large and exceed simpleMaxQ setting so not saving %u packets. Check the Moloch FAQ about (https://molo.ch/faq#why-am-i-dropping-packets) testing disk speed", DLL_COUNT(simple_, &simpleQ), notSaved);
        }
        return;
    }

    int thread = session->thread;

    if (!currentInfo[thread]) {
        char  dekhex[1024];
        char *name = 0;
        char *kekId;

        MolochSimple_t *info = currentInfo[thread] = writer_simple_alloc(thread, NULL);
        switch(simpleMode) {
        case MOLOCH_SIMPLE_NORMAL:
            name = moloch_db_create_file(packet->ts.tv_sec, NULL, 0, 0, &info->file->id);
            break;
        case MOLOCH_SIMPLE_XOR2048:
            kekId = writer_simple_get_kekId();
            RAND_bytes(info->file->dek, 256);
            writer_simple_encrypt_key(kekId, info->file->dek, 256, dekhex);
            name = moloch_db_create_file_full(packet->ts.tv_sec, NULL, 0, 0, &info->file->id,
                                              "encoding", "xor-2048",
                                              "dek", dekhex,
                                              "kekId", kekId,
                                              (char *)NULL);

            break;
        case MOLOCH_SIMPLE_AES256CTR: {
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
            name = moloch_db_create_file_full(packet->ts.tv_sec, NULL, 0, 0, &info->file->id,
                                              "encoding", "aes-256-ctr",
                                              "iv", ivhex,
                                              "dek", dekhex,
                                              "kekId", kekId,
                                              (char *)NULL);
            break;
        }
        default:
            LOGEXIT("Unknown simpleMode %d", simpleMode);
        }

        /* If offline pcap honor umask, otherwise disable other RW */
        if (config.pcapReadOffline) {
            currentInfo[thread]->file->fd = open(name,  openOptions, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        } else {
            currentInfo[thread]->file->fd = open(name,  openOptions, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        }
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

    struct moloch_pcap_sf_pkthdr hdr;

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
                LOGEXIT("Encrypting data failed");
            if ((int)total != outl)
                LOGEXIT("Encryption in (%u) and out (%d) didn't match", total, outl);
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
            if (ftruncate(info->file->fd, info->file->pos) < 0 && config.debug)
                LOG("Truncate failed");
            close(info->file->fd);
            moloch_db_update_filesize(info->file->id, info->file->pos);
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

    // Last add must be 10 seconds ago and have more then pageSize bytes
    if (now.tv_sec - lastSave[session->thread].tv_sec < 10)
        return;

    writer_simple_process_buf(session->thread, 0);
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

    return TRUE;
}
/******************************************************************************/
void writer_simple_init(char *name)
{
    moloch_writer_queue_length = writer_simple_queue_length;
    moloch_writer_exit         = writer_simple_exit;
    moloch_writer_write        = writer_simple_write;

    simpleMaxQ = moloch_config_int(NULL, "simpleMaxQ", 2000, 50, 0xffff);
    char *mode = moloch_config_str(NULL, "simpleEncoding", NULL);

    if (mode && !mode[0]) {
        g_free(mode);
        mode = NULL;
    }

    if (mode == NULL) {
    } else if (strcmp(mode, "aes-256-ctr") == 0) {
        simpleMode = MOLOCH_SIMPLE_AES256CTR;
        cipher = EVP_aes_256_ctr();
        if (config.maxFileSizeB > 64*1024LL*1024LL*1024LL) {
            LOG ("INFO: Reseting maxFileSizeG since %lf is greater then the max 64G in aes-256-ctr mode", config.maxFileSizeG);
            config.maxFileSizeG = 64.0;
            config.maxFileSizeB = 64LL*1024LL*1024LL*1024LL;
        }
    } else if (strcmp(mode, "xor-2048") == 0) {
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

    struct timeval now;
    gettimeofday(&now, NULL);

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        lastSave[thread] = now;
        DLL_INIT(simple_, &freeList[thread]);
        MOLOCH_LOCK_INIT(freeList[thread].lock);
    }

    g_thread_unref(g_thread_new("moloch-simple", &writer_simple_thread, NULL));

    g_timeout_add_seconds(1, writer_simple_check_gfunc, 0);
}
