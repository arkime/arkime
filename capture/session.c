/* session.c  -- Session functions
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

#include <arpa/inet.h>
#include "moloch.h"

/******************************************************************************/
extern MolochConfig_t        config;
extern uint32_t              pluginsCbs;
extern time_t                lastPacketSecs[MOLOCH_MAX_PACKET_THREADS];
extern MolochProtocol_t      mProtocols[0x100];

/******************************************************************************/

LOCAL int                   protocolField;
extern uint32_t             hashSalt;

LOCAL MolochSessionHead_t   closingQ[MOLOCH_MAX_PACKET_THREADS];
MolochSessionHead_t         tcpWriteQ[MOLOCH_MAX_PACKET_THREADS];

typedef HASHP_VAR(h_, MolochSessionHash_t, MolochSessionHead_t);

LOCAL MolochSessionHead_t   sessionsQ[MOLOCH_MAX_PACKET_THREADS][SESSION_MAX];
LOCAL MolochSessionHash_t   sessions[MOLOCH_MAX_PACKET_THREADS][SESSION_MAX];
LOCAL int needSave[MOLOCH_MAX_PACKET_THREADS];

typedef struct molochsescmd {
    struct molochsescmd *cmd_next, *cmd_prev;

    MolochSession_t *session;
    MolochSesCmd     cmd;
    gpointer         uw1;
    gpointer         uw2;
    MolochCmd_func   func;
} MolochSesCmd_t;

typedef struct {
    struct molochsescmd *cmd_next, *cmd_prev;
    int                  cmd_count;
    MOLOCH_LOCK_EXTERN(lock);
} MolochSesCmdHead_t;

LOCAL MolochSesCmdHead_t   sessionCmds[MOLOCH_MAX_PACKET_THREADS];

struct {
    GHashTable  *old;
    GHashTable  *new;
    MOLOCH_LOCK_EXTERN(lock);
} stoppedSessions[MOLOCH_MAX_PACKET_THREADS];
LOCAL char                 stoppedFilename[PATH_MAX];

/******************************************************************************/
#ifdef FUZZLOCH
// If FUZZLOCH mode we just use a unique sessionid for each input
extern uint64_t fuzzloch_sessionid;
void moloch_session_id (uint8_t *buf, uint32_t UNUSED(addr1), uint16_t UNUSED(port1), uint32_t UNUSED(addr2), uint16_t UNUSED(port2))
{
    buf[0] = 13;
    memcpy(buf + 1, &fuzzloch_sessionid, sizeof(fuzzloch_sessionid));
    memset(buf + 1 + sizeof(fuzzloch_sessionid), 0, 13 - 1 - sizeof(fuzzloch_sessionid));
}
void moloch_session_id6 (uint8_t *buf, uint8_t UNUSED(*addr1), uint16_t UNUSED(port1), uint8_t UNUSED(*addr2), uint16_t UNUSED(port2))
{
    buf[0] = 37;
    memcpy(buf + 1, &fuzzloch_sessionid, sizeof(fuzzloch_sessionid));
    memset(buf + 1 + sizeof(fuzzloch_sessionid), 0, 37 - 1 - sizeof(fuzzloch_sessionid));
}
#else
/******************************************************************************/
void moloch_session_id (uint8_t *buf, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2)
{
    buf[0] = 13;
    if (addr1 < addr2) {
        memcpy(buf+1, &addr1, 4);
        memcpy(buf+5, &port1, 2);
        memcpy(buf+7, &addr2, 4);
        memcpy(buf+11, &port2, 2);
    } else if (addr1 > addr2) {
        memcpy(buf+1, &addr2, 4);
        memcpy(buf+5, &port2, 2);
        memcpy(buf+7, &addr1, 4);
        memcpy(buf+11, &port1, 2);
    } else if (ntohs(port1) < ntohs(port2)) {
        memcpy(buf+1, &addr1, 4);
        memcpy(buf+5, &port1, 2);
        memcpy(buf+7, &addr2, 4);
        memcpy(buf+11, &port2, 2);
    } else {
        memcpy(buf+1, &addr2, 4);
        memcpy(buf+5, &port2, 2);
        memcpy(buf+7, &addr1, 4);
        memcpy(buf+11, &port1, 2);
    }
}
/******************************************************************************/
void moloch_session_id6 (uint8_t *buf, uint8_t *addr1, uint16_t port1, uint8_t *addr2, uint16_t port2)
{
    buf[0] = 37;
    int cmp = memcmp(addr1, addr2, 16);
    if (cmp < 0) {
        memcpy(buf+1, addr1, 16);
        memcpy(buf+17, &port1, 2);
        memcpy(buf+19, addr2, 16);
        memcpy(buf+35, &port2, 2);
    } else if (cmp > 0) {
        memcpy(buf+1, addr2, 16);
        memcpy(buf+17, &port2, 2);
        memcpy(buf+19, addr1, 16);
        memcpy(buf+35, &port1, 2);
    } else if (ntohs(port1) < ntohs(port2)) {
        memcpy(buf+1, addr1, 16);
        memcpy(buf+17, &port1, 2);
        memcpy(buf+19, addr2, 16);
        memcpy(buf+35, &port2, 2);
    } else {
        memcpy(buf+1, addr2, 16);
        memcpy(buf+17, &port2, 2);
        memcpy(buf+19, addr1, 16);
        memcpy(buf+35, &port1, 2);
    }
}
#endif
/******************************************************************************/
char *moloch_session_id_string (uint8_t *sessionId, char *buf)
{
    // ALW: Rewrite to make pretty
    return moloch_sprint_hex_string(buf, sessionId, sessionId[0]);
}
/******************************************************************************/
char *moloch_session_pretty_string (MolochSession_t *session, char *buf, int len)
{
    BSB bsb;
    BSB_INIT(bsb, buf, len);

    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        uint32_t ip1 = MOLOCH_V6_TO_V4(session->addr1);
        uint32_t ip2 = MOLOCH_V6_TO_V4(session->addr2);
        BSB_EXPORT_sprintf(bsb, "%u.%u.%u.%u:%u => %u.%u.%u.%u:%u", ip1 & 0xff, (ip1 >> 8) & 0xff, (ip1 >> 16) & 0xff, (ip1 >> 24) & 0xff, session->port1,
            ip2 & 0xff, (ip2 >> 8) & 0xff, (ip2 >> 16) & 0xff, (ip2 >> 24) & 0xff, session->port2);
    } else {
        BSB_EXPORT_inet_ntop(bsb, AF_INET6, &session->addr1);
        BSB_EXPORT_sprintf(bsb, ".%u", session->port1);
        BSB_EXPORT_cstr(bsb, " => ");
        BSB_EXPORT_inet_ntop(bsb, AF_INET6, &session->addr2);
        BSB_EXPORT_sprintf(bsb, ".%u", session->port2);
    }
    return buf;
}
#ifndef NEWHASH
/******************************************************************************/
/* https://github.com/aappleby/smhasher/blob/master/src/MurmurHash1.cpp
 * MurmurHash based
 */
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t moloch_session_hash(const void *key)
{
    uint32_t *p = (uint32_t *)key;
    uint32_t *end = (uint32_t *)((unsigned char *)key + ((unsigned char *)key)[0] - 4);
    uint32_t h = ((uint8_t *)key)[((uint8_t *)key)[0]-1];  // There is one extra byte at the end

    while (p < end) {
        h = (h + *p) * 0xc6a4a793;
        h ^= h >> 16;
        p += 1;
    }

    h ^= hashSalt;

    return h;
}
#else
/* http://academic-pub.org/ojs/index.php/ijecs/article/viewFile/1346/297
 * XOR32
 */
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t moloch_session_hash(const void *key)
{
    uint32_t *p = (uint32_t *)key;
    const uint32_t *end = (uint32_t *)((unsigned char *)key + ((unsigned char *)key)[0] - 4);
    uint32_t h = ((uint8_t *)key)[((uint8_t *)key)[0]-1];  // There is one extra byte at the end

    while (p < end) {
        h ^= *p;
        p += 1;
    }

    h ^= hashSalt;

    return h;
}
#endif

/******************************************************************************/
LOCAL gboolean moloch_session_equal(const unsigned char *a, const unsigned char *b)
{
    if (a[0] != b[0])
        return FALSE;

    return memcmp(a, b, a[0]) == 0;
}
/******************************************************************************/
LOCAL int moloch_session_cmp(const void *keyv, const MolochSession_t *session)
{
    return memcmp(keyv, session->sessionId, MIN(((uint8_t *)keyv)[0], session->sessionId[0])) == 0;
}
/******************************************************************************/
void moloch_session_add_cmd(MolochSession_t *session, MolochSesCmd sesCmd, gpointer uw1, gpointer uw2, MolochCmd_func func)
{
    MolochSesCmd_t *cmd = MOLOCH_TYPE_ALLOC(MolochSesCmd_t);
    cmd->cmd = sesCmd;
    cmd->session = session;
    cmd->uw1 = uw1;
    cmd->uw2 = uw2;
    cmd->func = func;
    MOLOCH_LOCK(sessionCmds[session->thread].lock);
    DLL_PUSH_TAIL(cmd_, &sessionCmds[session->thread], cmd);
    moloch_packet_thread_wake(session->thread);
    MOLOCH_UNLOCK(sessionCmds[session->thread].lock);
}
/******************************************************************************/
void moloch_session_add_cmd_thread(int thread, gpointer uw1, gpointer uw2, MolochCmd_func func)
{
    static MolochSession_t fakeSessions[MOLOCH_MAX_PACKET_THREADS];

    fakeSessions[thread].thread = thread;

    MolochSesCmd_t *cmd = MOLOCH_TYPE_ALLOC(MolochSesCmd_t);
    cmd->cmd = MOLOCH_SES_CMD_FUNC;
    cmd->session = &fakeSessions[thread];
    cmd->uw1 = uw1;
    cmd->uw2 = uw2;
    cmd->func = func;
    MOLOCH_LOCK(sessionCmds[thread].lock);
    DLL_PUSH_TAIL(cmd_, &sessionCmds[thread], cmd);
    moloch_packet_thread_wake(thread);
    MOLOCH_UNLOCK(sessionCmds[thread].lock);
}
/******************************************************************************/
void moloch_session_add_protocol(MolochSession_t *session, const char *protocol)
{
    moloch_field_string_add(protocolField, session, protocol, -1, TRUE);
}
/******************************************************************************/
gboolean moloch_session_has_protocol(MolochSession_t *session, const char *protocol)
{
    if (!session->fields[protocolField])
        return FALSE;

    MolochString_t          *hstring;
    HASH_FIND(s_, *(session->fields[protocolField]->shash), protocol, hstring);
    return hstring != 0;
}
/******************************************************************************/
void moloch_session_add_tag(MolochSession_t *session, const char *tag) {
    moloch_field_string_add(config.tagsStringField, session, tag, -1, TRUE);
}
/******************************************************************************/
void moloch_session_mark_for_close (MolochSession_t *session, SessionTypes ses)
{
    if (session->closingQ)
        return;

    session->closingQ = 1;
    session->saveTime = session->lastPacket.tv_sec + 5;
    DLL_REMOVE(q_, &sessionsQ[session->thread][ses], session);
    DLL_PUSH_TAIL(q_, &closingQ[session->thread], session);

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ[session->thread], session);
    }
}
/******************************************************************************/
LOCAL void moloch_session_free (MolochSession_t *session)
{
    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ[session->thread], session);
    }

    g_array_free(session->filePosArray, TRUE);
    if (config.enablePacketLen) {
        g_array_free(session->fileLenArray, TRUE);
    }
    g_array_free(session->fileNumArray, TRUE);

    if (session->rootId && session->rootId != (void *)1L)
        g_free(session->rootId);

    if (session->parserInfo) {
        int i;
        for (i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserFreeFunc)
                session->parserInfo[i].parserFreeFunc(session, session->parserInfo[i].uw);
        }
        free(session->parserInfo);
    }

    if (session->pluginData)
        MOLOCH_SIZE_FREE(pluginData, session->pluginData);
    moloch_field_free(session);

    if (mProtocols[session->mProtocol].sFree)
        mProtocols[session->mProtocol].sFree(session);

    if (session->pq)
        moloch_pq_free(session);

    if (session->inStoppedSave) {
        MOLOCH_LOCK(stoppedSessions[session->thread].lock);
        g_hash_table_remove(stoppedSessions[session->thread].new, session->sessionId);
        MOLOCH_UNLOCK(stoppedSessions[session->thread].lock);
    }

    MOLOCH_TYPE_FREE(MolochSession_t, session);
}
/******************************************************************************/
void moloch_session_save(MolochSession_t *session)
{
    if (session->h_next) {
        HASH_REMOVE(h_, sessions[session->thread][session->ses], session);
    }

    if (session->closingQ) {
        DLL_REMOVE(q_, &closingQ[session->thread], session);
    } else
        DLL_REMOVE(q_, &sessionsQ[session->thread][session->ses], session);

    if (mProtocols[session->mProtocol].sFree)
        mProtocols[session->mProtocol].sFree(session);

    if (session->parserInfo) {
        int i;
        for (i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserSaveFunc)
                session->parserInfo[i].parserSaveFunc(session, session->parserInfo[i].uw, TRUE);
        }
    }

    if (pluginsCbs & MOLOCH_PLUGIN_PRE_SAVE)
        moloch_plugins_cb_pre_save(session, TRUE);

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ[session->thread], session);
    }

    if (session->outstandingQueries > 0) {
        session->needSave = 1;
        needSave[session->thread]++;
        return;
    }

    moloch_rules_run_before_save(session, 1);
    moloch_db_save_session(session, TRUE);
    moloch_session_free(session);
}
/******************************************************************************/
void moloch_session_mid_save(MolochSession_t *session, uint32_t tv_sec)
{
    if (session->parserInfo) {
        int i;
        for (i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserSaveFunc)
                session->parserInfo[i].parserSaveFunc(session, session->parserInfo[i].uw, FALSE);
        }
    }

    if (pluginsCbs & MOLOCH_PLUGIN_PRE_SAVE)
        moloch_plugins_cb_pre_save(session, FALSE);

    if (!session->rootId) {
        session->rootId = (void *)1L;
    }

    moloch_rules_run_before_save(session, 0);
    moloch_db_save_session(session, FALSE);
    g_array_set_size(session->filePosArray, 0);
    if (config.enablePacketLen) {
        g_array_set_size(session->fileLenArray, 0);
    }
    g_array_set_size(session->fileNumArray, 0);
    session->lastFileNum = 0;

    if (session->tcp_next) {
        DLL_MOVE_TAIL(tcp_, &tcpWriteQ[session->thread], session);
    }

    // Don't change change saveTime if already closing
    if (!session->closingQ) {
        session->saveTime = tv_sec + config.tcpSaveTimeout;
    }

    session->bytes[0] = 0;
    session->bytes[1] = 0;
    session->databytes[0] = 0;
    session->databytes[1] = 0;
    session->packets[0] = 0;
    session->packets[1] = 0;
    session->midSave = 0;
    session->ackTime = 0;
    session->synTime = 0;
    memset(session->tcpFlagCnt, 0, sizeof(session->tcpFlagCnt));
}
/******************************************************************************/
gboolean moloch_session_decr_outstanding(MolochSession_t *session)
{
    session->outstandingQueries--;
    if (session->needSave && session->outstandingQueries == 0) {
        needSave[session->thread]--;
        session->needSave = 0; /* Stop endless loop if plugins add tags */

        moloch_rules_run_before_save(session, 1);
        moloch_db_save_session(session, TRUE);
        moloch_session_free(session);
        return FALSE;
    }

    return TRUE;
}
/******************************************************************************/
int moloch_session_close_outstanding()
{
    int count = 0;
    int t;
    for (t = 0; t < config.packetThreads; t++) {
        count += DLL_COUNT(q_, &closingQ[t]);
    }
    return count;
}
/******************************************************************************/
int moloch_session_cmd_outstanding()
{
    int count = 0;
    int t;
    for (t = 0; t < config.packetThreads; t++) {
        if (DLL_COUNT(cmd_, &sessionCmds[t]))
            moloch_packet_thread_wake(t);
        count += DLL_COUNT(cmd_, &sessionCmds[t]);
    }
    return count;
}
/******************************************************************************/
int moloch_session_need_save_outstanding()
{
    int count = 0;
    int t;
    for (t = 0; t < config.packetThreads; t++) {
        count += needSave[t];
    }
    return count;
}
/******************************************************************************/
void moloch_session_set_stop_saving(MolochSession_t *session)
{
    moloch_session_add_tag(session, "truncated-pcap");

    MOLOCH_LOCK(stoppedSessions[session->thread].lock);
    uint64_t result = (uint64_t)g_hash_table_lookup(stoppedSessions[session->thread].new, session->sessionId);
    if ((result & 0x02) == 0) {
        result |= 0x02;
        g_hash_table_insert(stoppedSessions[session->thread].new, session->sessionId, (gpointer)result);
        session->inStoppedSave = 1;
    }
    MOLOCH_UNLOCK(stoppedSessions[session->thread].lock);
}
/******************************************************************************/
void moloch_session_set_stop_spi(MolochSession_t *session, int value)
{
    session->stopSPI = value;

    MOLOCH_LOCK(stoppedSessions[session->thread].lock);
    uint64_t result = (uint64_t)g_hash_table_lookup(stoppedSessions[session->thread].new, session->sessionId);
    if (value) {
        if ((result & 0x01) == 0) {
            result |= 0x01;
            g_hash_table_insert(stoppedSessions[session->thread].new, session->sessionId, (gpointer)result);
            session->inStoppedSave = 1;
        }
    } else {
        if ((result & 0x01) == 0x01) {
            result &= ~0x01;
            if (result) {
                g_hash_table_insert(stoppedSessions[session->thread].new, session->sessionId, (gpointer)result);
                session->inStoppedSave = 1;
            } else {
                g_hash_table_remove(stoppedSessions[session->thread].new, session->sessionId);
                session->inStoppedSave = 0;
            }
        }
    }
    MOLOCH_UNLOCK(stoppedSessions[session->thread].lock);
}
/******************************************************************************/
LOCAL void moloch_session_load_stopped()
{
    if (!g_file_test(stoppedFilename, G_FILE_TEST_EXISTS))
        return;

    FILE *fp;
    if (!(fp = fopen(stoppedFilename, "r"))) {
        LOG("ERROR - Couldn't open `%s` to load stopped sessions", stoppedFilename);
        return;
    }

    int ver;
    if (!fread(&ver, 4, 1, fp)) {
        fclose(fp);
        LOG("ERROR - `%s` corrupt", stoppedFilename);
        return;
    }

    if (ver != 1) {
        fclose(fp);
        LOG("ERROR - Unknown save file version %d for `%s`", ver, stoppedFilename);
        return;
    }

    uint32_t cnt;
    if (!fread(&cnt, 4, 1, fp)) {
        fclose(fp);
        LOG("ERROR - `%s` corrupt", stoppedFilename);
        return;
    }

    if (config.debug)
        LOG("Load %u", cnt);
    for (uint32_t i = 0; i < cnt; i++) {
        int read = 0;
        uint8_t  key[MOLOCH_SESSIONID_LEN];
        uint32_t value;
        read += fread(key, 1, 1, fp);
        if (key[0] > MOLOCH_SESSIONID_LEN) {
            LOG("WARNING - `%s` corrupt", stoppedFilename);
            break;
        }
        read += fread(key+1, key[0] - 1, 1, fp);
        read += fread(&value, 4, 1, fp);

        if (read != 3) {
            LOG("WARNING - `%s` corrupt %d", stoppedFilename, read);
            break;
        }

        const uint32_t hash = moloch_session_hash(key);
        const int      thread = hash % config.packetThreads;

        g_hash_table_insert(stoppedSessions[thread].old, g_memdup(key, key[0]), (gpointer)(long)value);
    }
    fclose(fp);
}
/******************************************************************************/
LOCAL gboolean moloch_session_save_stopped(gpointer UNUSED(user_data))
{
    int t;

    // If quitting don't update since sessions are removed when not actually done
    if (config.quitting)
        return G_SOURCE_REMOVE;

    // Free old table first time this is called
    if (stoppedSessions[0].old) {
        for (t = 0; t < config.packetThreads; t++) {
            moloch_free_later(stoppedSessions[t].old, (GDestroyNotify)g_hash_table_destroy);
            stoppedSessions[t].old = NULL;
        }
    }

    FILE *fp;
    if (!(fp = fopen(stoppedFilename, "w"))) {
        LOG("ERROR - Couldn't open `%s` to save stopped sessions", stoppedFilename);
        return G_SOURCE_CONTINUE;
    }
    uint32_t ver = 1;
    uint32_t cnt = 0;
    fwrite(&ver, 4, 1, fp);

    // Skip the count
    fseek(fp, 4, SEEK_CUR);

    for (t = 0; t < config.packetThreads; t++) {
        MOLOCH_LOCK(stoppedSessions[t].lock);

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, stoppedSessions[t].new);
        unsigned char *ikey;
        gpointer ivalue;
        while (g_hash_table_iter_next (&iter, (gpointer *)&ikey, &ivalue)) {
            cnt++;
            fwrite(ikey, ikey[0], 1, fp);
            uint32_t val = (long)ivalue;
            fwrite(&val, 4, 1, fp);
        }
        MOLOCH_UNLOCK(stoppedSessions[t].lock);
    }

    // Now write the count
    fseek(fp, 4, SEEK_SET);
    fwrite(&cnt, 4, 1, fp);

    if (config.debug)
        LOG("Saved %u", cnt);

    fclose(fp);
    return G_SOURCE_CONTINUE;
}
/******************************************************************************/
MolochSession_t *moloch_session_find(int ses, uint8_t *sessionId)
{
    MolochSession_t *session;

    uint32_t hash = moloch_session_hash(sessionId);
    int      thread = hash % config.packetThreads;

    HASH_FIND_HASH(h_, sessions[thread][ses], hash, sessionId, session);
    return session;
}
/******************************************************************************/
// Should only be used by packet, lots of side effects
MolochSession_t *moloch_session_find_or_create(int mProtocol, uint32_t hash, uint8_t *sessionId, int *isNew)
{
    MolochSession_t *session;

    if (hash == 0) {
        hash = moloch_session_hash(sessionId);
    }

    int          thread = hash % config.packetThreads;
    SessionTypes ses = mProtocols[mProtocol].ses;

    HASH_FIND_HASH(h_, sessions[thread][ses], hash, sessionId, session);

    if (session) {
        if (!session->closingQ) {
            DLL_MOVE_TAIL(q_, &sessionsQ[thread][ses], session);
        }
        *isNew = 0;
        return session;
    }
    *isNew = 1;

    session = MOLOCH_TYPE_ALLOC0(MolochSession_t);
    session->ses = ses;
    session->mProtocol = mProtocol;
    session->stopSaving = 0xffff;

    memcpy(session->sessionId, sessionId, sessionId[0]);

    HASH_ADD_HASH(h_, sessions[thread][ses], hash, sessionId, session);
    DLL_PUSH_TAIL(q_, &sessionsQ[thread][ses], session);

    if (HASH_BUCKET_COUNT(h_, sessions[thread][ses], hash) > 15) {
        struct timeval  currentTime;
        static uint32_t lastError;

        gettimeofday(&currentTime, NULL);
        if (currentTime.tv_sec - lastError > 30) {
            lastError = currentTime.tv_sec;
            char buf[100];
            LOG("ERROR - Large number of chains: id:%s hash:%u bucket:%u thread:%d ses:%d count:%d size:%d maxStreams[ses]:%d - might want to increase maxStreams see https://arkime.com/settings#maxstreams", moloch_session_id_string(sessionId, buf), hash, hash % sessions[thread][ses].size, thread, ses, HASH_BUCKET_COUNT(h_, sessions[thread][ses], hash), sessions[thread][ses].size, config.maxStreams[ses]);
        }
    }

    session->filePosArray = g_array_sized_new(FALSE, FALSE, sizeof(uint64_t), 100);
    if (config.enablePacketLen) {
        session->fileLenArray = g_array_sized_new(FALSE, FALSE, sizeof(uint16_t), 100);
    }
    session->fileNumArray = g_array_new(FALSE, FALSE, 4);
    session->fields = MOLOCH_SIZE_ALLOC0(fields, sizeof(MolochField_t *)*config.maxField);
    session->maxFields = config.maxField;
    session->thread = thread;
    DLL_INIT(td_, &session->tcpData);
    if (config.numPlugins > 0)
        session->pluginData = MOLOCH_SIZE_ALLOC0(pluginData, sizeof(void *)*config.numPlugins);

    if (stoppedSessions[thread].old) {
        uint64_t result = (uint64_t)g_hash_table_lookup(stoppedSessions[session->thread].old, session->sessionId);
        if (result & 0x01) {
            session->stopSPI = 1;
        }
        if (result & 0x02) {
            session->stopSaving = 0;
        }
    }

    return session;
}
/******************************************************************************/
uint32_t moloch_session_monitoring()
{
    uint32_t count = 0;
    int      t, s;

    for (t = 0; t < config.packetThreads; t++) {
        for (s = 0; s < SESSION_MAX; s++) {
            count += HASH_COUNT(h_, sessions[t][s]);
        }
    }
    return count;
}
/******************************************************************************/
void moloch_session_process_commands(int thread)
{
    // Commands
    int count;
    for (count = 0; count < 50; count++) {
        MolochSesCmd_t *cmd = 0;
        MOLOCH_LOCK(sessionCmds[thread].lock);
        DLL_POP_HEAD(cmd_, &sessionCmds[thread], cmd);
        MOLOCH_UNLOCK(sessionCmds[thread].lock);
        if (!cmd)
            break;

        switch (cmd->cmd) {
        case MOLOCH_SES_CMD_FUNC:
            cmd->func(cmd->session, cmd->uw1, cmd->uw2);
            break;
        default:
            LOG ("Unknown cmd %d", cmd->cmd);
        }
        MOLOCH_TYPE_FREE(MolochSesCmd_t, cmd);
    }

    // Closing Q
    for (count = 0; count < 10; count++) {
        MolochSession_t *session = DLL_PEEK_HEAD(q_, &closingQ[thread]);

        if (session && session->saveTime < (uint64_t)lastPacketSecs[thread]) {
            moloch_session_save(session);
        } else {
            break;
        }
    }

    // Sessions Idle Long Time
    int ses;
    for (ses = 0; ses < SESSION_MAX; ses++) {
        for (count = 0; count < 10; count++) {
            MolochSession_t *session = DLL_PEEK_HEAD(q_, &sessionsQ[thread][ses]);

            if (session && (DLL_COUNT(q_, &sessionsQ[thread][ses]) > (int)config.maxStreams[ses] ||
                            ((uint64_t)session->lastPacket.tv_sec + config.timeouts[ses] < (uint64_t)lastPacketSecs[thread]))) {

                moloch_session_save(session);
            } else {
                break;
            }
        }
    }

    // TCP Sessions Open Long Time
    for (count = 0; count < 50; count++) {
        MolochSession_t *session = DLL_PEEK_HEAD(tcp_, &tcpWriteQ[thread]);

        if (session && (uint64_t)session->saveTime < (uint64_t)lastPacketSecs[thread]) {
            moloch_session_mid_save(session, lastPacketSecs[thread]);
        } else {
            break;
        }
    }
}

/******************************************************************************/
int moloch_session_watch_count(SessionTypes ses)
{
    int count = 0;
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        count += DLL_COUNT(q_, &sessionsQ[t][ses]);
    }
    return count;
}

/******************************************************************************/
int moloch_session_idle_seconds(SessionTypes ses)
{
    int idle = 0;
    int tmp;
    int t;

    for (t = 0; t < config.packetThreads; t++) {
        MolochSession_t *session = DLL_PEEK_HEAD(q_, &sessionsQ[t][ses]);
        if (!session)
            continue;

        tmp = lastPacketSecs[t] - (session->lastPacket.tv_sec + config.timeouts[ses]);
        if (tmp > idle)
            idle = tmp;
    }
    return idle;
}

/******************************************************************************/
void moloch_session_init()
{
    protocolField = moloch_field_define("general", "termfield",
        "protocols", "Protocols", "protocol",
        "Protocols set for session",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT | MOLOCH_FIELD_FLAG_LINKED_SESSIONS,
        (char *)NULL);

    int primes[SESSION_MAX];
    int s;
    for (s = 0; s < SESSION_MAX; s++) {
        primes[s] = moloch_get_next_prime(config.maxStreams[s]);
    }

    if (config.debug)
        LOG("session hash size %d %d %d %d %d %d", primes[SESSION_ICMP], primes[SESSION_UDP], primes[SESSION_TCP], primes[SESSION_SCTP], primes[SESSION_ESP], primes[SESSION_OTHER]);

    int t;
    for (t = 0; t < config.packetThreads; t++) {
        for (s = 0; s < SESSION_MAX; s++) {
            HASHP_INIT(h_, sessions[t][s], primes[s], moloch_session_hash, (HASH_CMP_FUNC)moloch_session_cmp);
            DLL_INIT(q_, &sessionsQ[t][s]);
        }

        DLL_INIT(tcp_, &tcpWriteQ[t]);
        DLL_INIT(q_, &closingQ[t]);
        DLL_INIT(cmd_, &sessionCmds[t]);
        MOLOCH_LOCK_INIT(sessionCmds[t].lock);


        MOLOCH_LOCK_INIT(stoppedSessions[t].lock);
        stoppedSessions[t].old = g_hash_table_new_full(moloch_session_hash, (GEqualFunc)moloch_session_equal, g_free, NULL);
        stoppedSessions[t].new = g_hash_table_new(moloch_session_hash, (GEqualFunc)moloch_session_equal);
    }

    moloch_add_can_quit(moloch_session_cmd_outstanding, "session commands outstanding");
    moloch_add_can_quit(moloch_session_close_outstanding, "session close outstanding");
    moloch_add_can_quit(moloch_session_need_save_outstanding, "session save outstanding");

    g_timeout_add_seconds(10, moloch_session_save_stopped, 0);

    snprintf(stoppedFilename, sizeof(stoppedFilename), "/tmp/%s.stoppedsessions", config.nodeName);
    moloch_session_load_stopped();
}
/******************************************************************************/
LOCAL void moloch_session_flush_close(MolochSession_t *session, gpointer UNUSED(uw1), gpointer UNUSED(uw2))
{
    int thread = session->thread;
    int i;

    for (i = 0; i < SESSION_MAX; i++) {
        HASH_FORALL_POP_HEAD(h_, sessions[thread][i], session,
            moloch_session_save(session);
        );
    }
    moloch_pq_flush(thread);
}
/******************************************************************************/
/* Only called on main thread. Wait for all packet threads to be empty and then
 * start the save process on sessions.
 */
void moloch_session_flush()
{
    moloch_packet_flush();

    int thread;
    for (thread = 0; thread < config.packetThreads; thread++) {
        moloch_session_add_cmd_thread(thread, NULL, NULL, moloch_session_flush_close);
    }
}
/******************************************************************************/
void moloch_session_exit()
{
    uint32_t counts[SESSION_MAX] = {0, 0, 0, 0, 0, 0};

    int t, s;

    for (t = 0; t < config.packetThreads; t++) {
        for (s = 0; s < SESSION_MAX; s++) {
            counts[s] += sessionsQ[t][s].q_count;
        }
    }

    if (!config.pcapReadOffline || config.debug)
        LOG("sessions: %u tcp: %u udp: %u icmp: %u sctp: %u esp: %u other: %u",
            moloch_session_monitoring(),
            counts[SESSION_TCP],
            counts[SESSION_UDP],
            counts[SESSION_ICMP],
            counts[SESSION_SCTP],
            counts[SESSION_ESP],
            counts[SESSION_OTHER]
            );

    moloch_session_flush();
}
