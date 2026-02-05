/* pq.c  -- Priority Q
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

/******************************************************************************/
extern ArkimeConfig_t        config;

LOCAL GPtrArray  *pqs;
typedef struct {
    uint32_t    pqEntries;
    time_t      lastRun;
} ARKIME_CACHE_ALIGN pqThreadData_t;

LOCAL pqThreadData_t pqThreadData[ARKIME_MAX_PACKET_THREADS];

/******************************************************************************/
typedef struct arkimepqitem {
    struct arkimepqitem *pql_next, *pql_prev;
    struct arkimepqitem *pqh_next, *pqh_prev;

    ArkimeSession_t     *session;
    void                *uw;
    uint32_t             expire;
    uint32_t             pqh_hash;
    uint32_t             pqh_bucket;
} ArkimePQItem_t;

typedef struct {
    struct arkimepqitem *pql_next, *pql_prev;
    struct arkimepqitem *pqh_next, *pqh_prev;
    int                  pql_count;
    int                  pqh_count;
} ARKIME_CACHE_ALIGN ArkimePQHead_t;

typedef HASH_VAR(s_, ARKIME_CACHE_ALIGN ArkimePQHash_t, ArkimePQHead_t, 51);

struct ArkimePQ_t {
    ArkimePQHead_t      lists[ARKIME_MAX_PACKET_THREADS];
    ArkimePQHash_t      keys[ARKIME_MAX_PACKET_THREADS];
    ArkimePQ_cb         cb;
    uint32_t            timeout;
};

/******************************************************************************/
LOCAL int arkime_pq_cmp(const void *keyv, const ArkimePQItem_t *item)
{
    return memcmp(keyv, item->session->sessionId, item->session->sessionId[0]) == 0;
}
/******************************************************************************/
ArkimePQ_t *arkime_pq_alloc(int timeout, ArkimePQ_cb cb)
{
    if (!pqs) {
        pqs = g_ptr_array_new();
    }

    ArkimePQ_t *pq = ARKIME_TYPE_ALLOC0_ALIGNED(ArkimePQ_t);

    pq->timeout = timeout;
    for (int t = 0; t < config.packetThreads; t++) {
        HASH_INIT(pqh_, pq->keys[t], arkime_string_hash, (HASH_CMP_FUNC)arkime_pq_cmp);
        DLL_INIT(pql_, &pq->lists[t]);
    }
    pq->cb = cb;
    g_ptr_array_add(pqs, pq);

    return pq;
}
/******************************************************************************/
void arkime_pq_upsert(ArkimePQ_t *pq, ArkimeSession_t *session, void *uw)
{
    // timeout is relative to lastPacketSecs, figure out time
    uint32_t expire = arkimeThreadData[session->thread].lastPacketSecs + pq->timeout;

    ArkimePQItem_t *item;
    HASH_FIND(pqh_, (pq->keys[session->thread]), session->sessionId, item);
    if (item) {
        DLL_MOVE_TAIL(pql_, &pq->lists[session->thread], item);
        item->expire = expire;
        return;
    }

    // This is a new item
    item = ARKIME_TYPE_ALLOC(ArkimePQItem_t);
    DLL_PUSH_TAIL(pql_, &pq->lists[session->thread], item);
    HASH_ADD(pqh_, pq->keys[session->thread], session->sessionId, item);
    item->expire = expire;
    item->session = session;
    item->uw = uw;
    session->pq = 1;
    pqThreadData[session->thread].pqEntries++;
}
/******************************************************************************/
void arkime_pq_remove(ArkimePQ_t *pq, ArkimeSession_t *session)
{
    ArkimePQItem_t *item;
    HASH_FIND(pqh_, pq->keys[session->thread], session->sessionId, item);
    if (!item)
        return;

    DLL_REMOVE(pql_, &pq->lists[session->thread], item);
    HASH_REMOVE(pqh_, pq->keys[session->thread], item);
    ARKIME_TYPE_FREE(ArkimePQItem_t, item);
    pqThreadData[session->thread].pqEntries--;
}
/******************************************************************************/
void arkime_pq_run(int thread, int max)
{
    if (!pqs)
        return;

    if (pqThreadData[thread].pqEntries == 0 || arkimeThreadData[thread].lastPacketSecs == pqThreadData[thread].lastRun)
        return;
    pqThreadData[thread].lastRun = arkimeThreadData[thread].lastPacketSecs;

    for (guint i = 0; i < pqs->len; i++) {
        int cnt = max;
        ArkimePQ_t *pq = g_ptr_array_index(pqs, i);
        ArkimePQItem_t *item = 0;
        while (cnt > 0 && (item = DLL_PEEK_HEAD(pql_, &pq->lists[thread]))) {
            if (item->expire >= (uint64_t)arkimeThreadData[thread].lastPacketSecs)
                break;

            DLL_REMOVE(pql_, &pq->lists[thread], item);
            HASH_REMOVE(pqh_, pq->keys[thread], item);
            pq->cb(item->session, item->uw);
            ARKIME_TYPE_FREE(ArkimePQItem_t, item);
            pqThreadData[thread].pqEntries--;
            cnt--;
        }

        if (DLL_PEEK_HEAD(pql_, &pq->lists[thread]))
            pqThreadData[thread].lastRun = 0;
    }
}
/******************************************************************************/
/* Remove this session from all PQs */
void arkime_pq_free(ArkimeSession_t *session)
{
    if (!pqs)
        return;

    for (guint i = 0; i < pqs->len; i++) {
        ArkimePQ_t *pq = g_ptr_array_index(pqs, i);
        arkime_pq_remove(pq, session);
    }
}
/******************************************************************************/
void arkime_pq_flush(int thread)
{
    if (!pqs)
        return;

    for (guint i = 0; i < pqs->len; i++) {
        ArkimePQ_t *pq = g_ptr_array_index(pqs, i);
        ArkimePQItem_t *item = 0;
        while (DLL_POP_HEAD(pql_, &pq->lists[thread], item)) {
            HASH_REMOVE(pqh_, pq->keys[thread], item);
            ARKIME_TYPE_FREE(ArkimePQItem_t, item);
            pqThreadData[thread].pqEntries--;
        }
    }
}
/******************************************************************************/
