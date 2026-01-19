/* session.c  -- Session functions
 *
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>
#include <sys/socket.h>
#include "arkime.h"

/******************************************************************************/
extern ArkimeConfig_t        config;
extern uint32_t              pluginsCbs;
extern time_t                lastPacketSecs[ARKIME_MAX_PACKET_THREADS];
extern int                   mProtocolCnt;
extern ArkimeProtocol_t      mProtocols[ARKIME_MPROTOCOL_MAX];

/******************************************************************************/

LOCAL int                   protocolField;
extern uint32_t             hashSalt;

LOCAL ArkimeSessionHead_t   closingQ[ARKIME_MAX_PACKET_THREADS];
ArkimeSessionHead_t         tcpWriteQ[ARKIME_MAX_PACKET_THREADS];

#if ARKIME_SESSION_HASH == ARKIME_SESSION_HASH_CTRL_PROBE
#define PROBE_EMPTY   0x80
#define PROBE_DELETED 0xFF
typedef struct {
    uint8_t *ctrl;
    ArkimeSession_t **sessions;
    uint32_t count;
    uint32_t mask;
    uint32_t size;
} ArkimeSessionHash_t;
#elif ARKIME_SESSION_HASH == ARKIME_SESSION_HASH_SLL
typedef struct {
    ArkimeSession_t **sessions;
    uint32_t count;
    uint32_t mask;
    uint32_t size;
} ArkimeSessionHash_t;
#elif ARKIME_SESSION_HASH == ARKIME_SESSION_HASH_DLL
typedef struct {
    ArkimeSessionHead_t *buckets;
    uint32_t count;
    uint32_t mask;
    uint32_t size;
} ArkimeSessionHash_t;
#endif

LOCAL ArkimeSessionHead_t   sessionsQ[ARKIME_MAX_PACKET_THREADS][ARKIME_MPROTOCOL_MAX];
LOCAL ArkimeSessionHash_t   sessions[ARKIME_MAX_PACKET_THREADS][SESSION_MAX];
LOCAL int needSave[ARKIME_MAX_PACKET_THREADS];
LOCAL int tcpClosingTimeout;

typedef struct arkimesescmd {
    struct arkimesescmd *cmd_next, *cmd_prev;

    ArkimeSession_t *session;
    ArkimeSesCmd     cmd;
    gpointer         uw1;
    gpointer         uw2;
    ArkimeCmd_func   func;
} ArkimeSesCmd_t;

typedef struct {
    struct arkimesescmd *cmd_next, *cmd_prev;
    int                  cmd_count;
    ARKIME_LOCK_EXTERN(lock);
} ArkimeSesCmdHead_t;

LOCAL ArkimeSesCmdHead_t   sessionCmds[ARKIME_MAX_PACKET_THREADS];

struct {
    GHashTable  *old;
    GHashTable  *new;
    ARKIME_LOCK_EXTERN(lock);
} stoppedSessions[ARKIME_MAX_PACKET_THREADS];
LOCAL char                 stoppedFilename[PATH_MAX];

typedef enum {
    ARKIME_TRACKING_NONE,
    ARKIME_TRACKING_VLAN,
    ARKIME_TRACKING_VNI
} ArkimeSessionIdTracking;

LOCAL ArkimeSessionIdTracking sessionIdTracking = ARKIME_TRACKING_NONE;
LOCAL GHashTable *collapseTable;

LOCAL int arkime_session_pre_save_func;

void arkime_session_save(ArkimeSession_t *session);
/******************************************************************************/
#if defined(FUZZLOCH) && !defined(SFUZZLOCH)
// If FUZZLOCH mode we just use a unique sessionid for each input
extern uint64_t fuzzloch_sessionid;
void arkime_session_id (uint8_t *buf, uint32_t UNUSED(addr1), uint16_t UNUSED(port1), uint32_t UNUSED(addr2), uint16_t UNUSED(port2), uint16_t UNUSED(vlan), uint32_t UNUSED(vni))
{
    buf[0] = ARKIME_SESSIONID4_LEN;
    memcpy(buf + 1, &fuzzloch_sessionid, sizeof(fuzzloch_sessionid));
    memset(buf + 1 + sizeof(fuzzloch_sessionid), 0, ARKIME_SESSIONID4_LEN - 1 - sizeof(fuzzloch_sessionid));
}
void arkime_session_id6 (uint8_t *buf, const uint8_t UNUSED(*addr1), uint16_t UNUSED(port1), const uint8_t UNUSED(*addr2), uint16_t UNUSED(port2), uint16_t UNUSED(vlan), uint32_t UNUSED(vni))
{
    buf[0] = ARKIME_SESSIONID6_LEN;
    memcpy(buf + 1, &fuzzloch_sessionid, sizeof(fuzzloch_sessionid));
    memset(buf + 1 + sizeof(fuzzloch_sessionid), 0, ARKIME_SESSIONID6_LEN - 1 - sizeof(fuzzloch_sessionid));
}
#else
/******************************************************************************/
void arkime_session_id (uint8_t *buf, uint32_t addr1, uint16_t port1, uint32_t addr2, uint16_t port2, uint16_t vlan, uint32_t vni)
{
    // Layout: [len:1][vlan/vni:3][addr1:4][addr2:4][port1:2][port2:2] = 16 bytes
    // This layout ensures 4-byte values are aligned at offsets 4 and 8
    buf[0] = ARKIME_SESSIONID4_LEN;

    // Write vlan/vni at bytes 1-3 (unaligned, but only 3 bytes)
    switch (sessionIdTracking) {
    case ARKIME_TRACKING_NONE:
        buf[1] = buf[2] = buf[3] = 0;
        break;
    case ARKIME_TRACKING_VLAN:
        buf[1] = 0;
        if (collapseTable) {
            uint16_t value = GPOINTER_TO_UINT(g_hash_table_lookup(collapseTable, GINT_TO_POINTER(vlan)));
            if (value) {
                value--;
                buf[2] = value & 0xff;
                buf[3] = (value >> 8) & 0xff;
                break;
            }
        }
        buf[2] = vlan & 0xff;
        buf[3] = (vlan >> 8) & 0xff;
        break;
    case ARKIME_TRACKING_VNI:
        if (collapseTable) {
            uint32_t value = GPOINTER_TO_UINT(g_hash_table_lookup(collapseTable, GINT_TO_POINTER(vni)));
            if (value) {
                value--;
                buf[1] = value & 0xff;
                buf[2] = (value >> 8) & 0xff;
                buf[3] = (value >> 16) & 0xff;
                break;
            }
        }
        buf[1] = vni & 0xff;
        buf[2] = (vni >> 8) & 0xff;
        buf[3] = (vni >> 16) & 0xff;
        break;
    } /* switch */

    // Write addresses and ports at aligned offsets
    if (addr1 < addr2) {
        *(uint32_t *)(buf + 4) = addr1;
        *(uint32_t *)(buf + 8) = addr2;
        *(uint16_t *)(buf + 12) = port1;
        *(uint16_t *)(buf + 14) = port2;
    } else if (addr1 > addr2) {
        *(uint32_t *)(buf + 4) = addr2;
        *(uint32_t *)(buf + 8) = addr1;
        *(uint16_t *)(buf + 12) = port2;
        *(uint16_t *)(buf + 14) = port1;
    } else if (ntohs(port1) < ntohs(port2)) {
        *(uint32_t *)(buf + 4) = addr1;
        *(uint32_t *)(buf + 8) = addr2;
        *(uint16_t *)(buf + 12) = port1;
        *(uint16_t *)(buf + 14) = port2;
    } else {
        *(uint32_t *)(buf + 4) = addr2;
        *(uint32_t *)(buf + 8) = addr1;
        *(uint16_t *)(buf + 12) = port2;
        *(uint16_t *)(buf + 14) = port1;
    }
}
/******************************************************************************/
void arkime_session_id6 (uint8_t *buf, const uint8_t *addr1, uint16_t port1, const uint8_t *addr2, uint16_t port2, uint16_t vlan, uint32_t vni)
{
    // Layout: [len:1][vlan/vni:3][addr1:16][addr2:16][port1:2][port2:2] = 40 bytes
    // This layout ensures addresses start at offset 4 (aligned) and ports at 36/38 (aligned)
    buf[0] = ARKIME_SESSIONID6_LEN;

    // Write vlan/vni at bytes 1-3 (unaligned, but only 3 bytes)
    switch (sessionIdTracking) {
    case ARKIME_TRACKING_NONE:
        buf[1] = buf[2] = buf[3] = 0;
        break;
    case ARKIME_TRACKING_VLAN:
        buf[1] = 0;
        if (collapseTable) {
            uint16_t value = GPOINTER_TO_UINT(g_hash_table_lookup(collapseTable, GINT_TO_POINTER(vlan)));
            if (value) {
                value--;
                buf[2] = value & 0xff;
                buf[3] = (value >> 8) & 0xff;
                break;
            }
        }
        buf[2] = vlan & 0xff;
        buf[3] = (vlan >> 8) & 0xff;
        break;
    case ARKIME_TRACKING_VNI:
        if (collapseTable) {
            uint32_t value = GPOINTER_TO_UINT(g_hash_table_lookup(collapseTable, GINT_TO_POINTER(vni)));
            if (value) {
                value--;
                buf[1] = value & 0xff;
                buf[2] = (value >> 8) & 0xff;
                buf[3] = (value >> 16) & 0xff;
                break;
            }
        }
        buf[1] = vni & 0xff;
        buf[2] = (vni >> 8) & 0xff;
        buf[3] = (vni >> 16) & 0xff;
        break;
    } /* switch */

    // Write addresses and ports at aligned offsets
    int cmp = memcmp(addr1, addr2, 16);
    if (cmp < 0) {
        memcpy(buf + 4, addr1, 16);
        memcpy(buf + 20, addr2, 16);
        *(uint16_t *)(buf + 36) = port1;
        *(uint16_t *)(buf + 38) = port2;
    } else if (cmp > 0) {
        memcpy(buf + 4, addr2, 16);
        memcpy(buf + 20, addr1, 16);
        *(uint16_t *)(buf + 36) = port2;
        *(uint16_t *)(buf + 38) = port1;
    } else if (ntohs(port1) < ntohs(port2)) {
        memcpy(buf + 4, addr1, 16);
        memcpy(buf + 20, addr2, 16);
        *(uint16_t *)(buf + 36) = port1;
        *(uint16_t *)(buf + 38) = port2;
    } else {
        memcpy(buf + 4, addr2, 16);
        memcpy(buf + 20, addr1, 16);
        *(uint16_t *)(buf + 36) = port2;
        *(uint16_t *)(buf + 38) = port1;
    }
}
#endif
/******************************************************************************/
char *arkime_session_id_string (const uint8_t *sessionId, char *buf)
{
    // ALW: Rewrite to make pretty
    return arkime_sprint_hex_string(buf, sessionId, sessionId[0]);
}
/******************************************************************************/
char *arkime_session_pretty_string (ArkimeSession_t *session, char *buf, int len)
{
    BSB bsb;
    BSB_INIT(bsb, buf, len);

    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        BSB_EXPORT_ip4tostr(bsb, ARKIME_V6_TO_V4(session->addr1));
        BSB_EXPORT_sprintf(bsb, ":%u", session->port1);
        BSB_EXPORT_cstr(bsb, " => ");
        BSB_EXPORT_ip4tostr(bsb, ARKIME_V6_TO_V4(session->addr2));
        BSB_EXPORT_sprintf(bsb, ":%u", session->port2);
    } else {
        BSB_EXPORT_inet_ntop(bsb, AF_INET6, &session->addr1);
        BSB_EXPORT_sprintf(bsb, ".%u", session->port1);
        BSB_EXPORT_cstr(bsb, " => ");
        BSB_EXPORT_inet_ntop(bsb, AF_INET6, &session->addr2);
        BSB_EXPORT_sprintf(bsb, ".%u", session->port2);
    }
    return buf;
}

#ifndef MURMUR3
/******************************************************************************/
/* https://github.com/aappleby/smhasher/blob/master/src/MurmurHash1.cpp
 * MurmurHash based
 */
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t arkime_session_hash(const void *key)
{
    uint32_t *p = (uint32_t *)key;
    const uint32_t *end = (uint32_t *)((uint8_t *)key + ((uint8_t *)key)[0]);
    uint32_t h = 0;

    while (p < end) {
        h = (h + *p) * 0xc6a4a793;
        h ^= h >> 16;
        p += 1;
    }

    h ^= hashSalt;

    return h;
}
#else
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
// MurmurHash3 based
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
uint32_t arkime_session_hash(const void *key)
{
    uint32_t *p = (uint32_t *)key;
    const uint32_t *end = (uint32_t *)((uint8_t *)key + ((uint8_t *)key)[0]);

    uint32_t h1 = hashSalt;

    while (p < end) {
        uint32_t k1 = *p;
        k1 *= 0xcc9e2d51;
        k1 = (k1 << 15) | (k1 >> 17); // Rotate left 15 bits
        k1 *= 0x1b873531;

        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> 19); // Rotate left 13 bits
        h1 = h1 * 5 + 0xe6546b64;
        p++;
    }

    // Final mixing step
    h1 ^= ((uint8_t *)key)[0];
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

#endif

/******************************************************************************/
LOCAL gboolean arkime_session_equal(const uint8_t *a, const uint8_t *b)
{
    if (a[0] != b[0])
        return FALSE;

    return memcmp(a, b, a[0]) == 0;
}
/******************************************************************************/
void arkime_session_add_cmd(ArkimeSession_t *session, ArkimeSesCmd sesCmd, gpointer uw1, gpointer uw2, ArkimeCmd_func func)
{
    ArkimeSesCmd_t *cmd = ARKIME_TYPE_ALLOC(ArkimeSesCmd_t);
    cmd->cmd = sesCmd;
    cmd->session = session;
    cmd->uw1 = uw1;
    cmd->uw2 = uw2;
    cmd->func = func;
    ARKIME_LOCK(sessionCmds[session->thread].lock);
    DLL_PUSH_TAIL(cmd_, &sessionCmds[session->thread], cmd);
    arkime_packet_thread_wake(session->thread);
    ARKIME_UNLOCK(sessionCmds[session->thread].lock);
}
/******************************************************************************/
void arkime_session_add_cmd_thread(int thread, gpointer uw1, gpointer uw2, ArkimeCmd_func func)
{
    ArkimeSesCmd_t *cmd = ARKIME_TYPE_ALLOC(ArkimeSesCmd_t);
    cmd->cmd = ARKIME_SES_CMD_FUNC;
    cmd->session = NULL;
    cmd->uw1 = uw1;
    cmd->uw2 = uw2;
    cmd->func = func;

    ARKIME_LOCK(sessionCmds[thread].lock);
    DLL_PUSH_TAIL(cmd_, &sessionCmds[thread], cmd);
    arkime_packet_thread_wake(thread);
    ARKIME_UNLOCK(sessionCmds[thread].lock);
}
/******************************************************************************/
void arkime_session_add_protocol(ArkimeSession_t *session, const char *protocol)
{
    arkime_field_string_add(protocolField, session, protocol, -1, TRUE);
}
/******************************************************************************/
gboolean arkime_session_has_protocol(ArkimeSession_t *session, const char *protocol)
{
    if (!session->fields[protocolField])
        return FALSE;

    ArkimeString_t          *hstring;
    HASH_FIND(s_, *(session->fields[protocolField]->shash), protocol, hstring);
    return hstring != 0;
}
/******************************************************************************/
void arkime_session_add_tag(ArkimeSession_t *session, const char *tag)
{
    arkime_field_string_add(config.tagsStringField, session, tag, -1, TRUE);
}
/******************************************************************************/
void arkime_session_mark_for_close (ArkimeSession_t *session)
{
    if (session->closingQ)
        return;

    session->closingQ = 1;
    session->saveTime = session->lastPacket.tv_sec + tcpClosingTimeout;
    DLL_REMOVE(q_, &sessionsQ[session->thread][session->mProtocol], session);
    DLL_PUSH_TAIL(q_, &closingQ[session->thread], session);

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ[session->thread], session);
    }
}
/******************************************************************************/
void arkime_session_flip_src_dst (ArkimeSession_t *session)
{
    struct in6_addr        addr;
    uint16_t               port;

    addr = session->addr1;
    session->addr1 = session->addr2;
    session->addr2 = addr;

    port = session->port1;
    session->port1 = session->port2;
    session->port2 = port;
}
/******************************************************************************/
LOCAL void arkime_session_free (ArkimeSession_t *session)
{
    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ[session->thread], session);
    }

    g_array_free(session->filePosArray, TRUE);
    if (config.enablePacketLen) {
        g_array_free(session->fileLenArray, TRUE);
    }
    g_array_free(session->fileNumArray, TRUE);

    if (session->rootId && session->rootId != GINT_TO_POINTER(1))
        g_free(session->rootId);

    if (session->parserInfo) {
        for (int i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserFreeFunc)
                session->parserInfo[i].parserFreeFunc(session, session->parserInfo[i].uw);
        }
        ARKIME_SIZE_FREE("parserInfo", session->parserInfo);
    }

    if (session->pluginData)
        ARKIME_SIZE_FREE("pluginData", session->pluginData);
    arkime_field_free(session);

    if (mProtocols[session->mProtocol].sFree)
        mProtocols[session->mProtocol].sFree(session);

    if (session->pq)
        arkime_pq_free(session);

    if (session->inStoppedSave) {
        ARKIME_LOCK(stoppedSessions[session->thread].lock);
        g_hash_table_remove(stoppedSessions[session->thread].new, session->sessionId);
        ARKIME_UNLOCK(stoppedSessions[session->thread].lock);
    }

#ifdef HAVE_PYTHON
    if (session->pythonAttrs)
        arkime_python_session_free(session);
#endif

    ARKIME_TYPE_FREE(ArkimeSession_t, session);
}
/******************************************************************************/
/** CTRL_PROBE Hash Implementation **/
/******************************************************************************/
#if ARKIME_SESSION_HASH == ARKIME_SESSION_HASH_CTRL_PROBE
#define IN_SESSION_TABLE(s) ((s)->ses_slot != 0xffffffff)
LOCAL void arkime_session_hash_init(ArkimeSessionHash_t *hash, uint32_t size)
{
    size = MAX(32, arkime_get_next_powerof2(size));
    hash->ctrl = ARKIME_SIZE_ALLOC("ctrl", size);
    memset(hash->ctrl, PROBE_EMPTY, size);
    hash->sessions = ARKIME_SIZE_ALLOC0("sessions", sizeof(ArkimeSessionHead_t *) * size);
    hash->size = size;
    hash->mask = size - 1;
    hash->count = 0;
}
/******************************************************************************/
LOCAL void arkime_session_hash_remove(ArkimeSessionHash_t *hash, ArkimeSession_t *session)
{
    hash->ctrl[session->ses_slot] = PROBE_DELETED;
    session->ses_slot = 0xffffffff;
    hash->count--;
}
/******************************************************************************/
LOCAL void arkime_session_hash_add(ArkimeSessionHash_t *hash, uint32_t h, ArkimeSession_t *session);
LOCAL void arkime_session_hash_resize(ArkimeSessionHash_t *UNUSED(hash))
{
    if (config.debug)
        LOG("Resizing session hash table from %u to %u with %u items", hash->size, hash->size << 1, hash->count);
    ArkimeSession_t **oldSessions = hash->sessions;
    uint8_t *oldCtrl = hash->ctrl;
    const uint32_t oldSize = hash->size;
    const uint32_t size = MAX(1024, hash->size << 1);

    hash->ctrl = ARKIME_SIZE_ALLOC("ctrl", size);
    memset(hash->ctrl, PROBE_EMPTY, size);
    hash->sessions = ARKIME_SIZE_ALLOC0("sessions", sizeof(ArkimeSessionHead_t *) * size);
    hash->size = size;
    hash->mask = size - 1;
    hash->count = 0;

    for (uint32_t s = 0; s < oldSize; s++) {
        if (oldCtrl[s] & PROBE_EMPTY)
            continue;
        arkime_session_hash_add(hash, oldSessions[s]->ses_hash, oldSessions[s]);
    }
    ARKIME_SIZE_FREE("sessions", oldSessions);
    ARKIME_SIZE_FREE("ctrl", oldCtrl);
}
/******************************************************************************/
LOCAL void arkime_session_hash_add(ArkimeSessionHash_t *hash, uint32_t h, ArkimeSession_t *session)
{
    // Resize when 50% full
    if (hash->count >= hash->size >> 1) {
        arkime_session_hash_resize(hash);
    }

    session->ses_hash = h;
    uint32_t s = (h >> 7) & hash->mask;
    for (uint32_t i = 0; i < hash->size; i++) {
        if (hash->ctrl[s] & PROBE_EMPTY) {
            hash->ctrl[s] = (uint8_t)(h & 0x7f);
            session->ses_slot = s;
            hash->sessions[s] = session;
            hash->count++;
            return;
        }
        s = (s + 1) & hash->mask;
    }
}
/******************************************************************************/
LOCAL ArkimeSession_t *arkime_session_hash_find(const ArkimeSessionHash_t *hash, uint32_t h, const uint8_t *sessionId)
{
    const uint8_t h2 = (uint8_t)(h & 0x7F);

    uint32_t s = (h >> 7) & hash->mask;
    for (uint32_t i = 0; i < hash->size; i++) {
        if (hash->ctrl[s] == PROBE_EMPTY) {
            return NULL;
        }
        if (h2 == hash->ctrl[s] && h == hash->sessions[s]->ses_hash && memcmp(sessionId, hash->sessions[s]->sessionId, sessionId[0]) == 0) {
            return hash->sessions[s];
        }
        s = (s + 1) & hash->mask;
    }

    return NULL;
}
/******************************************************************************/
LOCAL void arkime_session_flush_close(ArkimeSession_t *UNUSED(session), gpointer uw1, gpointer UNUSED(uw2))
{
    int thread = GPOINTER_TO_INT(uw1);

    for (int i = 0; i < SESSION_MAX; i++) {
        ArkimeSessionHash_t *hash = &sessions[thread][i];
        for (uint32_t s = 0; s < hash->size; s++) {
            if (hash->ctrl[s] & PROBE_EMPTY)
                continue;
            hash->ctrl[s] = PROBE_DELETED;
            hash->sessions[s]->ses_slot = 0xffffffff;
            hash->count--;
            arkime_session_save(hash->sessions[s]);
        }
    }
    arkime_pq_flush(thread);
}
/******************************************************************************/
/** SLL Hash Implementation **/
/******************************************************************************/
#elif ARKIME_SESSION_HASH == ARKIME_SESSION_HASH_SLL
#define IN_SESSION_TABLE(s) ((s)->inSessionTable)
LOCAL void arkime_session_hash_init(ArkimeSessionHash_t *hash, uint32_t size)
{
    size = MAX(32, arkime_get_next_powerof2(size));
    hash->sessions = ARKIME_SIZE_ALLOC0("sessions", sizeof(ArkimeSession_t *) * size);
    hash->size = size;
    hash->mask = size - 1;
    hash->count = 0;
}
/******************************************************************************/
LOCAL void arkime_session_hash_remove(ArkimeSessionHash_t *hash, ArkimeSession_t *session)
{
    uint32_t b = session->ses_hash & hash->mask;

    if (hash->sessions[b] == NULL) {
        return;
    }

    if (hash->sessions[b] == session) {
        hash->sessions[b] = session->ses_next;
        session->inSessionTable = 0;
        hash->count--;
        return;
    }

    ArkimeSession_t *s = hash->sessions[b];
    while (s->ses_next && s->ses_next != session) {
        s = s->ses_next;
    }

    if (!s->ses_next)
        return;

    s->ses_next = session->ses_next;
    session->inSessionTable = 0;
    hash->count--;
}
/******************************************************************************/
LOCAL void arkime_session_hash_resize(ArkimeSessionHash_t *hash)
{
    if (config.debug)
        LOG("Resizing session hash table from %u to %u with %u items", hash->size, hash->size << 1, hash->count);
    ArkimeSession_t **oldSessions = hash->sessions;
    const uint32_t oldSize = hash->size;
    const uint32_t size = MAX(1024, hash->size << 1);

    hash->sessions = ARKIME_SIZE_ALLOC0("sessions", sizeof(ArkimeSession_t *) * size);
    hash->size = size;
    hash->mask = size - 1;

    for (uint32_t i = 0; i < oldSize; i++) {
        if (!oldSessions[i])
            continue;

        ArkimeSession_t *session = oldSessions[i], *next;
        while (session) {
            next = session->ses_next;

            uint32_t b2 = session->ses_hash & hash->mask;
            session->ses_next = hash->sessions[b2];
            hash->sessions[b2] = session;

            session = next;
        }
    }
    ARKIME_SIZE_FREE("sessions", oldSessions);
}
/******************************************************************************/
LOCAL void arkime_session_hash_add(ArkimeSessionHash_t *hash, uint32_t h, ArkimeSession_t *session)
{
    // Resize when there are 4 entries per bucket on average
    if (hash->count >= hash->size << 2) {
        arkime_session_hash_resize(hash);
    }

    uint32_t b = h & hash->mask;

    session->ses_next = hash->sessions[b];
    hash->sessions[b] = session;

    session->ses_hash = h;
    session->inSessionTable = 1;
    hash->count++;
}
/******************************************************************************/
LOCAL ArkimeSession_t *arkime_session_hash_find(const ArkimeSessionHash_t *hash, uint32_t h, const uint8_t *sessionId)
{
    uint32_t b = h & hash->mask;

    ArkimeSession_t *session = hash->sessions[b];
    while (session) {
        if (h == session->ses_hash && memcmp(sessionId, session->sessionId, sessionId[0]) == 0) {
            return session;
        }
        session = session->ses_next;
    }

    return NULL;
}
/******************************************************************************/
LOCAL void arkime_session_flush_close(ArkimeSession_t *session, gpointer uw1, gpointer UNUSED(uw2))
{
    int thread = GPOINTER_TO_INT(uw1);

    for (int i = 0; i < SESSION_MAX; i++) {
        ArkimeSessionHash_t *hash = &sessions[thread][i];
        for (uint32_t b = 0; b < hash->size; b++) {
            while (hash->sessions[b]) {
                session = hash->sessions[b];
                hash->sessions[b] = session->ses_next;
                session->ses_next = NULL;
                hash->count--;
                session->inSessionTable = 0;
                arkime_session_save(session);
            }
        }
    }
    arkime_pq_flush(thread);
}
/******************************************************************************/
/** DLL Hash Implementation **/
/******************************************************************************/
#elif ARKIME_SESSION_HASH == ARKIME_SESSION_HASH_DLL
#define IN_SESSION_TABLE(s) ((s)->ses_next)
LOCAL void arkime_session_hash_init(ArkimeSessionHash_t *hash, uint32_t size)
{
    size = MAX(32, arkime_get_next_powerof2(size));
    hash->buckets = ARKIME_SIZE_ALLOC0("buckets", sizeof(ArkimeSessionHead_t) * size);
    hash->size = size;
    hash->mask = size - 1;
    hash->count = 0;
}
/******************************************************************************/
LOCAL void arkime_session_hash_remove(ArkimeSessionHash_t *hash, ArkimeSession_t *session)
{
    uint32_t b = session->ses_hash & hash->mask;

    if (hash->buckets[b].ses_next == NULL)
        return;

    DLL_REMOVE(ses_, &hash->buckets[b], session);
    hash->count--;
}
/******************************************************************************/
LOCAL void arkime_session_hash_resize(ArkimeSessionHash_t *hash)
{
    if (config.debug)
        LOG("Resizing session hash table from %u to %u with %u items", hash->size, hash->size << 1, hash->count);
    ArkimeSessionHead_t *oldBuckets = hash->buckets;
    const uint32_t oldSize = hash->size;
    const uint32_t size = MAX(1024, hash->size << 1);

    hash->buckets = ARKIME_SIZE_ALLOC0("buckets", sizeof(ArkimeSessionHead_t) * size);
    hash->size = size;
    hash->mask = size - 1;

    for (uint32_t i = 0; i < oldSize; i++) {
        if (oldBuckets[i].ses_next == NULL)
            continue;

        ArkimeSession_t *session;
        while (DLL_POP_HEAD(ses_, &oldBuckets[i], session)) {
            uint32_t b2 = session->ses_hash & hash->mask;
            if (hash->buckets[b2].ses_next == NULL)
                DLL_INIT(ses_, &hash->buckets[b2]);
            DLL_PUSH_HEAD(ses_, &hash->buckets[b2], session);
        }
    }
    ARKIME_SIZE_FREE("buckets", oldBuckets);
}
/******************************************************************************/
LOCAL void arkime_session_hash_add(ArkimeSessionHash_t *hash, uint32_t h, ArkimeSession_t *session)
{
    // Resize when there are 4 entries per bucket on average
    if (hash->count >= hash->size << 2) {
        arkime_session_hash_resize(hash);
    }

    uint32_t b = h & hash->mask;

    session->ses_hash = h;
    if (hash->buckets[b].ses_next == NULL)
        DLL_INIT(ses_, &hash->buckets[b]);
    DLL_PUSH_HEAD(ses_, &hash->buckets[b], session);
    hash->count++;
}
/******************************************************************************/
LOCAL ArkimeSession_t *arkime_session_hash_find(const ArkimeSessionHash_t *hash, uint32_t h, const uint8_t *sessionId)
{
    ArkimeSession_t *session;

    uint32_t b = h & hash->mask;

    if (hash->buckets[b].ses_next == NULL)
        return NULL;

    DLL_FOREACH(ses_, &hash->buckets[b], session) {
        if (h == session->ses_hash && memcmp(sessionId, session->sessionId, sessionId[0]) == 0)
            return session;
    }

    return NULL;
}
/******************************************************************************/
LOCAL void arkime_session_flush_close(ArkimeSession_t *session, gpointer uw1, gpointer UNUSED(uw2))
{
    int thread = GPOINTER_TO_INT(uw1);

    for (int i = 0; i < SESSION_MAX; i++) {
        ArkimeSessionHash_t *hash = &sessions[thread][i];
        for (uint32_t b = 0; b < hash->size; b++) {
            if (hash->buckets[b].ses_next == NULL)
                continue;
            while (DLL_POP_HEAD(ses_, &hash->buckets[b], session)) {
                hash->count--;
                arkime_session_save(session);
            }
        }
    }
    arkime_pq_flush(thread);
}
#endif
/******************************************************************************/
void arkime_session_save(ArkimeSession_t *session)
{
    if (IN_SESSION_TABLE(session)) {
        arkime_session_hash_remove(&sessions[session->thread][session->ses], session);
    }

    if (session->closingQ) {
        DLL_REMOVE(q_, &closingQ[session->thread], session);
    } else
        DLL_REMOVE(q_, &sessionsQ[session->thread][session->mProtocol], session);

    if (mProtocols[session->mProtocol].sFree)
        mProtocols[session->mProtocol].sFree(session);

    if (session->parserInfo) {
        for (int i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserSaveFunc)
                session->parserInfo[i].parserSaveFunc(session, session->parserInfo[i].uw, TRUE);
        }
    }

    if (pluginsCbs & ARKIME_PLUGIN_PRE_SAVE)
        arkime_plugins_cb_pre_save(session, TRUE);

    arkime_parsers_call_named_func(arkime_session_pre_save_func, session, NULL, 1, NULL);

    if (session->tcp_next) {
        DLL_REMOVE(tcp_, &tcpWriteQ[session->thread], session);
    }

    if (session->outstandingQueries > 0) {
        session->needSave = 1;
        needSave[session->thread]++;
        return;
    }

    arkime_rules_run_before_save(session, 1);
    arkime_db_save_session(session, TRUE);
    arkime_session_free(session);
}
/******************************************************************************/
void arkime_session_mid_save(ArkimeSession_t *session, uint32_t tv_sec)
{
    if (session->parserInfo) {
        for (int i = 0; i < session->parserNum; i++) {
            if (session->parserInfo[i].parserSaveFunc)
                session->parserInfo[i].parserSaveFunc(session, session->parserInfo[i].uw, FALSE);
        }
    }

    if (pluginsCbs & ARKIME_PLUGIN_PRE_SAVE)
        arkime_plugins_cb_pre_save(session, FALSE);

    arkime_parsers_call_named_func(arkime_session_pre_save_func, session, NULL, 0, NULL);

    if (!session->rootId) {
        session->rootId = GINT_TO_POINTER(1);
    }

    arkime_rules_run_before_save(session, 0);
    arkime_db_save_session(session, FALSE);
    g_array_set_size(session->filePosArray, 0);
    if (config.enablePacketLen) {
        g_array_set_size(session->fileLenArray, 0);
    }
    g_array_set_size(session->fileNumArray, 0);
    session->lastFileNum = 0;

    if (session->tcp_next) {
        DLL_MOVE_TAIL(tcp_, &tcpWriteQ[session->thread], session);
    }

    // Don't change saveTime if already closing
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


    if (mProtocols[session->mProtocol].midSave)
        mProtocols[session->mProtocol].midSave(session);
}
/******************************************************************************/
gboolean arkime_session_decr_outstanding(ArkimeSession_t *session)
{
    session->outstandingQueries--;
    if (session->needSave && session->outstandingQueries == 0) {
        needSave[session->thread]--;
        session->needSave = 0; /* Stop endless loop if plugins add tags */

        arkime_rules_run_before_save(session, 1);
        arkime_db_save_session(session, TRUE);
        arkime_session_free(session);
        return FALSE;
    }

    return TRUE;
}
/******************************************************************************/
int arkime_session_close_outstanding()
{
    int count = 0;
    for (int t = 0; t < config.packetThreads; t++) {
        count += DLL_COUNT(q_, &closingQ[t]);
    }
    return count;
}
/******************************************************************************/
int arkime_session_cmd_outstanding()
{
    int count = 0;
    for (int t = 0; t < config.packetThreads; t++) {
        if (DLL_COUNT(cmd_, &sessionCmds[t]))
            arkime_packet_thread_wake(t);
        count += DLL_COUNT(cmd_, &sessionCmds[t]);
    }
    return count;
}
/******************************************************************************/
int arkime_session_need_save_outstanding()
{
    int count = 0;
    for (int t = 0; t < config.packetThreads; t++) {
        count += needSave[t];
    }
    return count;
}
/******************************************************************************/
void arkime_session_set_stop_saving(ArkimeSession_t *session)
{
    arkime_session_add_tag(session, "truncated-pcap");

    ARKIME_LOCK(stoppedSessions[session->thread].lock);
    uint64_t result = (uint64_t)g_hash_table_lookup(stoppedSessions[session->thread].new, session->sessionId);
    if ((result & 0x02) == 0) {
        result |= 0x02;
        g_hash_table_insert(stoppedSessions[session->thread].new, session->sessionId, (gpointer)result);
        session->inStoppedSave = 1;
    }
    ARKIME_UNLOCK(stoppedSessions[session->thread].lock);
}
/******************************************************************************/
void arkime_session_set_stop_spi(ArkimeSession_t *session, int value)
{
    session->stopSPI = value;

    ARKIME_LOCK(stoppedSessions[session->thread].lock);
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
    ARKIME_UNLOCK(stoppedSessions[session->thread].lock);
}
/******************************************************************************/
LOCAL void arkime_session_load_stopped()
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
        uint8_t  key[ARKIME_SESSIONID_LEN];
        uint32_t value;
        read += fread(key, 1, 1, fp);
        if (key[0] > ARKIME_SESSIONID_LEN) {
            LOG("WARNING - `%s` corrupt", stoppedFilename);
            break;
        }
        read += fread(key + 1, key[0] - 1, 1, fp);
        read += fread(&value, 4, 1, fp);

        if (read != 3) {
            LOG("WARNING - `%s` corrupt %d", stoppedFilename, read);
            break;
        }

        const uint32_t hash = arkime_session_hash(key);
        const int      thread = hash % config.packetThreads;

        g_hash_table_insert(stoppedSessions[thread].old, g_memdup(key, key[0]), (gpointer)(long)value);
    }
    fclose(fp);
}
/******************************************************************************/
LOCAL gboolean arkime_session_save_stopped(gpointer UNUSED(user_data))
{
    // If quitting don't update since sessions are removed when not actually done
    if (config.quitting)
        return G_SOURCE_REMOVE;

    // Free old table first time this is called
    if (stoppedSessions[0].old) {
        for (int t = 0; t < config.packetThreads; t++) {
            arkime_free_later(stoppedSessions[t].old, (GDestroyNotify)g_hash_table_destroy);
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

    for (int t = 0; t < config.packetThreads; t++) {
        ARKIME_LOCK(stoppedSessions[t].lock);

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, stoppedSessions[t].new);
        uint8_t *ikey;
        gpointer ivalue;
        while (g_hash_table_iter_next (&iter, (gpointer *)&ikey, &ivalue)) {
            cnt++;
            fwrite(ikey, ikey[0], 1, fp);
            uint32_t val = (long)ivalue;
            fwrite(&val, 4, 1, fp);
        }
        ARKIME_UNLOCK(stoppedSessions[t].lock);
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
ArkimeSession_t *arkime_session_find(int ses, const uint8_t *sessionId)
{
    ArkimeSession_t *session;

    uint32_t hash = arkime_session_hash(sessionId);
    int      thread = hash % config.packetThreads;

    session = arkime_session_hash_find(&sessions[thread][ses], hash, sessionId);
    return session;
}
/******************************************************************************/
// Should only be used by packet, lots of side effects
ArkimeSession_t *arkime_session_find_or_create(int mProtocol, uint32_t hash, const uint8_t *sessionId, int *isNew)
{
    ArkimeSession_t *session;

    if (hash == 0) {
        hash = arkime_session_hash(sessionId);
    }

    int          thread = hash % config.packetThreads;
    SessionTypes ses = mProtocols[mProtocol].ses;

    session = arkime_session_hash_find(&sessions[thread][ses], hash, sessionId);

    if (session) {
        if (!session->closingQ) {
            DLL_MOVE_TAIL(q_, &sessionsQ[thread][session->mProtocol], session);
        }
        *isNew = 0;
        return session;
    }
    *isNew = 1;

    session = ARKIME_TYPE_ALLOC0(ArkimeSession_t);
    session->ses = ses;
    session->mProtocol = mProtocol;
    session->stopSaving = 0xffff;

    memcpy(session->sessionId, sessionId, sessionId[0]);

    arkime_session_hash_add(&sessions[thread][ses], hash, session);
    DLL_PUSH_TAIL(q_, &sessionsQ[thread][session->mProtocol], session);

    session->filePosArray = g_array_sized_new(FALSE, FALSE, sizeof(uint64_t), 100);
    if (config.enablePacketLen) {
        session->fileLenArray = g_array_sized_new(FALSE, FALSE, sizeof(uint16_t), 100);
    }
    session->fileNumArray = g_array_sized_new(FALSE, FALSE, sizeof(uint32_t), 2);
    session->fields = ARKIME_SIZE_ALLOC0("fields", sizeof(ArkimeField_t *) * config.maxDbField);
    session->maxFields = config.maxDbField;
    session->thread = thread;
    if (config.numPlugins > 0)
        session->pluginData = ARKIME_SIZE_ALLOC0("pluginData", sizeof(void *) * config.numPlugins);

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
uint32_t arkime_session_monitoring()
{
    uint32_t count = 0;

    for (int t = 0; t < config.packetThreads; t++) {
        for (int s = 0; s < SESSION_MAX; s++) {
            count += HASH_COUNT(h_, sessions[t][s]);
        }
    }
    return count;
}
/******************************************************************************/
void arkime_session_process_commands(int thread)
{
    // Commands
    for (int count = 0; count < 50; count++) {
        ArkimeSesCmd_t *cmd = 0;
        ARKIME_LOCK(sessionCmds[thread].lock);
        DLL_POP_HEAD(cmd_, &sessionCmds[thread], cmd);
        ARKIME_UNLOCK(sessionCmds[thread].lock);
        if (!cmd)
            break;

        switch (cmd->cmd) {
        case ARKIME_SES_CMD_FUNC:
            cmd->func(cmd->session, cmd->uw1, cmd->uw2);
            break;
        default:
            LOG("Unknown cmd %d", cmd->cmd);
        }
        ARKIME_TYPE_FREE(ArkimeSesCmd_t, cmd);
    }

    // Closing Q
    for (int count = 0; count < 10; count++) {
        ArkimeSession_t *session = DLL_PEEK_HEAD(q_, &closingQ[thread]);

        if (session && session->saveTime < (uint64_t)lastPacketSecs[thread]) {
            arkime_session_save(session);
        } else {
            break;
        }
    }

    // Sessions Idle Long Time
    for (int mProtocol = ARKIME_MPROTOCOL_MIN; mProtocol < mProtocolCnt; mProtocol++) {
        for (int count = 0; count < 10; count++) {
            ArkimeSession_t *session = DLL_PEEK_HEAD(q_, &sessionsQ[thread][mProtocol]);

            if (!session)
                break;

            if (DLL_COUNT(q_, &sessionsQ[thread][mProtocol]) > (int)config.maxStreams[session->ses]) {
                LOG_RATE(60, "ERROR - closing session early, increase maxStreams see https://arkime.com/settings#maxStreams");
                arkime_session_save(session);
            } else if (((uint64_t)session->lastPacket.tv_sec + mProtocols[mProtocol].sessionTimeout < (uint64_t)lastPacketSecs[thread])) {
                arkime_session_save(session);
            } else {
                break;
            }
        }
    }

    // TCP Sessions Open Long Time
    for (int count = 0; count < 50; count++) {
        ArkimeSession_t *session = DLL_PEEK_HEAD(tcp_, &tcpWriteQ[thread]);

        if (session && (uint64_t)session->saveTime < (uint64_t)lastPacketSecs[thread]) {
            arkime_session_mid_save(session, lastPacketSecs[thread]);
        } else {
            break;
        }
    }
}

/******************************************************************************/
int arkime_session_watch_count(SessionTypes ses)
{
    int count = 0;

    for (int t = 0; t < config.packetThreads; t++) {
        for (int mProtocol = ARKIME_MPROTOCOL_MIN; mProtocol < mProtocolCnt; mProtocol++) {
            if (mProtocols[mProtocol].ses == ses) {
                count += DLL_COUNT(q_, &sessionsQ[t][mProtocol]);
            }
        }
    }
    return count;
}

/******************************************************************************/
int arkime_session_idle_seconds(int mProtocol)
{
    int idle = 0;
    int tmp;

    for (int t = 0; t < config.packetThreads; t++) {
        ArkimeSession_t *session = DLL_PEEK_HEAD(q_, &sessionsQ[t][mProtocol]);
        if (!session)
            continue;

        tmp = lastPacketSecs[t] - (session->lastPacket.tv_sec + mProtocols[mProtocol].sessionTimeout);
        if (tmp > idle)
            idle = tmp;
    }
    return idle;
}

/******************************************************************************/
LOCAL void arkime_session_load_collapse()
{
    gsize keys_len;
    gchar **keys = arkime_config_section_keys(NULL, "vlan-vni-collapse", &keys_len);
    if (keys_len > 0) {
        collapseTable = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
    for (int i = 0; i < (int)keys_len; i++) {
        char *value = arkime_config_section_str(NULL, "vlan-vni-collapse", keys[i], NULL);
        char **values = g_strsplit(value, ",", 0);

        uint64_t key = atoi(keys[i]) + 1;
        for (int j = 0; values[j]; j++) {
            uint64_t ivalue = atoi(values[j]);
            g_hash_table_insert(collapseTable, GINT_TO_POINTER(ivalue), GINT_TO_POINTER(key));
        }
        g_strfreev(values);
        g_free(value);
    }
    g_strfreev(keys);
}
/******************************************************************************/
void arkime_session_init()
{
    protocolField = arkime_field_define("general", "termfield",
                                        "protocols", "Protocols", "protocol",
                                        "Protocols set for session",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT | ARKIME_FIELD_FLAG_LINKED_SESSIONS,
                                        (char *)NULL);

    tcpClosingTimeout = arkime_config_int(NULL, "tcpClosingTimeout", 5, 1, 255);

    char *str = arkime_config_str(NULL, "sessionIdTracking", "none");
    if (strcmp(str, "none") == 0) {
        sessionIdTracking = ARKIME_TRACKING_NONE;
    } else if (strcmp(str, "vlan") == 0) {
        sessionIdTracking = ARKIME_TRACKING_VLAN;
    } else if (strcmp(str, "vni") == 0) {
        sessionIdTracking = ARKIME_TRACKING_VNI;
    } else {
        CONFIGEXIT("sessionIdTracking must be none, vlan or vni not '%s'", str);
    }
    g_free(str);

    for (int t = 0; t < config.packetThreads; t++) {
        for (int s = 0; s < SESSION_MAX; s++) {
            arkime_session_hash_init(&sessions[t][s], config.maxStreams[s]);
        }

        for (int mProtocol = ARKIME_MPROTOCOL_MIN; mProtocol < ARKIME_MPROTOCOL_MAX; mProtocol++) {
            DLL_INIT(q_, &sessionsQ[t][mProtocol]);
        }

        DLL_INIT(tcp_, &tcpWriteQ[t]);
        DLL_INIT(q_, &closingQ[t]);
        DLL_INIT(cmd_, &sessionCmds[t]);
        ARKIME_LOCK_INIT(sessionCmds[t].lock);


        ARKIME_LOCK_INIT(stoppedSessions[t].lock);
        stoppedSessions[t].old = g_hash_table_new_full(arkime_session_hash, (GEqualFunc)arkime_session_equal, g_free, NULL);
        stoppedSessions[t].new = g_hash_table_new(arkime_session_hash, (GEqualFunc)arkime_session_equal);
    }

    arkime_add_can_quit(arkime_session_cmd_outstanding, "session commands outstanding");
    arkime_add_can_quit(arkime_session_close_outstanding, "session close outstanding");
    arkime_add_can_quit(arkime_session_need_save_outstanding, "session save outstanding");

    g_timeout_add_seconds(10, arkime_session_save_stopped, 0);

    snprintf(stoppedFilename, sizeof(stoppedFilename), "/tmp/%s.stoppedsessions", config.nodeName);
    arkime_session_load_stopped();
    arkime_session_load_collapse();

    arkime_session_pre_save_func = arkime_parsers_get_named_func("arkime_session_pre_save");
}
/******************************************************************************/
/* Only called on main thread. Wait for all packet threads to be empty and then
 * start the save process on sessions.
 */
void arkime_session_flush()
{
    arkime_packet_flush();

    for (int thread = 0; thread < config.packetThreads; thread++) {
        arkime_session_add_cmd_thread(thread, GINT_TO_POINTER(thread), NULL, arkime_session_flush_close);
    }
}
/******************************************************************************/
void arkime_session_exit()
{
    uint32_t counts[SESSION_MAX] = {0, 0, 0, 0, 0, 0};

    for (int t = 0; t < config.packetThreads; t++) {
        for (int mProtocol = ARKIME_MPROTOCOL_MIN; mProtocol < mProtocolCnt; mProtocol++) {
            counts[mProtocols[mProtocol].ses] += sessionsQ[t][mProtocol].q_count;
        }
    }

    if (!config.pcapReadOffline || config.debug)
        LOG("sessions: %u tcp: %u udp: %u icmp: %u sctp: %u esp: %u other: %u",
            arkime_session_monitoring(),
            counts[SESSION_TCP],
            counts[SESSION_UDP],
            counts[SESSION_ICMP],
            counts[SESSION_SCTP],
            counts[SESSION_ESP],
            counts[SESSION_OTHER]
           );

    arkime_session_flush();
}
