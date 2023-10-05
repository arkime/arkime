/* pq.c  -- Priority Q
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

/******************************************************************************/
extern ArkimeConfig_t        config;
extern time_t                lastPacketSecs[ARKIME_MAX_PACKET_THREADS];

LOCAL int         numPQs;
LOCAL ArkimePQ_t *pqs[10];
LOCAL uint32_t    pqEntries;


/******************************************************************************/
typedef struct arkimepqitem {
    struct arkimepqitem *pql_next, *pql_prev;
    struct arkimepqitem *pqh_next, *pqh_prev;

    ArkimeSession_t     *session;
    void                *uw;
    time_t               expire;
    uint32_t             pqh_hash;
    uint32_t             pqh_bucket;
} ArkimePQItem_t;

typedef struct {
    struct arkimepqitem *pql_next, *pql_prev;
    struct arkimepqitem *pqh_next, *pqh_prev;
    int                  pql_count;
    int                  pqh_count;
} ArkimePQHead_t;

typedef HASH_VAR(s_, ArkimePQHash_t, ArkimePQHead_t, 51);

struct ArkimePQ_t {
    ArkimePQHead_t     *buckets[ARKIME_MAX_PACKET_THREADS];
    ArkimePQHash_t      keys[ARKIME_MAX_PACKET_THREADS];
    ArkimePQ_cb         cb;
    time_t              bucket0[ARKIME_MAX_PACKET_THREADS];
    uint32_t            maxSeconds;
};

/******************************************************************************/
LOCAL int arkime_pq_cmp(const void *keyv, const ArkimePQItem_t *item)
{
    return memcmp(keyv, item->session->sessionId, MIN(((uint8_t *)keyv)[0], item->session->sessionId[0])) == 0;
}
/******************************************************************************/
ArkimePQ_t *arkime_pq_alloc(int maxSeconds, ArkimePQ_cb cb)
{
    ArkimePQ_t *pq = ARKIME_TYPE_ALLOC0(ArkimePQ_t);

    pq->maxSeconds = maxSeconds;
    int t, i;
    for (t = 0; t < config.packetThreads; t++) {
        pq->bucket0[t] = 0;
        pq->buckets[t] = malloc(sizeof(ArkimePQHead_t) * (maxSeconds + 1));
        HASH_INIT(pqh_, pq->keys[t], arkime_string_hash, (HASH_CMP_FUNC)arkime_pq_cmp);
        for (i = 0; i <= maxSeconds; i++)
            DLL_INIT(pql_, &pq->buckets[t][i]);
    }
    pq->cb = cb;
    pqs[numPQs] = pq;
    numPQs++;

    return pq;
}
/******************************************************************************/
LOCAL void arkime_pq_shift(ArkimePQ_t *pq, int thread)
{
    uint32_t shift = lastPacketSecs[thread] - pq->bucket0[thread];

    if (shift > pq->maxSeconds)
        shift = pq->maxSeconds;

    for (uint32_t i = 1; i <= pq->maxSeconds; i++) {
        if (i <= shift) {
            DLL_PUSH_TAIL_DLL(pql_, &pq->buckets[thread][0], &pq->buckets[thread][i]);
        }
        if (i+shift <= pq->maxSeconds) {
            DLL_PUSH_TAIL_DLL(pql_, &pq->buckets[thread][i], &pq->buckets[thread][i+shift]);
        }
    }
    pq->bucket0[thread] = lastPacketSecs[thread];
}
/******************************************************************************/
void arkime_pq_upsert(ArkimePQ_t *pq, ArkimeSession_t *session, uint32_t timeout, void *uw)
{
    // timeout is relative to lastPacketSecs, figure out time
    time_t expire = lastPacketSecs[session->thread] + timeout;

    // Now recalculate bucket0 if we need to
    if (lastPacketSecs[session->thread] > pq->bucket0[session->thread])
        arkime_pq_shift(pq, session->thread);

    // In the past, just run now
    if (expire < pq->bucket0[session->thread])
        timeout = 0;
    else {
        timeout = expire - pq->bucket0[session->thread];

        // To far in the future for this PQ
        if (timeout > pq->maxSeconds)
            timeout = pq->maxSeconds;
    }

    ArkimePQItem_t *item;
    HASH_FIND(pqh_, (pq->keys[session->thread]), session->sessionId, item);
    if (item) {
        uint32_t bucket = item->expire - pq->bucket0[session->thread];
        if (bucket > pq->maxSeconds)
            bucket = pq->maxSeconds;

        // Same bucket
        if (bucket == timeout) {
            return;
        }

        // Move the item from 1 bucket to another
        DLL_REMOVE(pql_, &pq->buckets[session->thread][bucket], item);
        DLL_PUSH_TAIL(pql_, &pq->buckets[session->thread][timeout], item);
        item->expire = expire;
        return;
    }

    // This is a new item
    item = ARKIME_TYPE_ALLOC(ArkimePQItem_t);
    DLL_PUSH_TAIL(pql_, &pq->buckets[session->thread][timeout], item);
    HASH_ADD(pqh_, pq->keys[session->thread], session->sessionId, item);
    item->expire = expire;
    item->session = session;
    item->uw = uw;
    session->pq = 1;
    pqEntries++;
}
/******************************************************************************/
void arkime_pq_remove(ArkimePQ_t *pq, ArkimeSession_t *session)
{
    ArkimePQItem_t *item;
    HASH_FIND(pqh_, pq->keys[session->thread], session->sessionId, item);
    if (!item)
        return;

    int bucket = item->expire - pq->bucket0[session->thread];
    if (bucket < 0) bucket = 0;
    DLL_REMOVE(pql_, &pq->buckets[session->thread][bucket], item);
    HASH_REMOVE(pqh_, pq->keys[session->thread], item);
    ARKIME_TYPE_FREE(ArkimePQItem_t, item);
    pqEntries--;
}
/******************************************************************************/
void arkime_pq_run(int thread, int max)
{
    if (pqEntries == 0)
        return;

    int i;
    for (i = 0; i < numPQs; i++) {
        if (lastPacketSecs[thread] > pqs[i]->bucket0[thread])
            arkime_pq_shift(pqs[i], thread);

        int cnt = max;
        ArkimePQItem_t *item = 0;
        while (cnt > 0 && DLL_POP_HEAD(pql_, &pqs[i]->buckets[thread][0], item)) {
            HASH_REMOVE(pqh_, pqs[i]->keys[thread], item);
            pqs[i]->cb(item->session, item->uw);
            ARKIME_TYPE_FREE(ArkimePQItem_t, item);
            cnt--;
        }
    }
}
/******************************************************************************/
/* Remove this session from all PQs */
void arkime_pq_free(ArkimeSession_t *session)
{
    int i;
    for (i = 0; i < numPQs; i++) {
        arkime_pq_remove(pqs[i], session);
    }
}
/******************************************************************************/
/* Reset the bucket0 time */
void arkime_pq_flush(int thread)
{
    for (int i = 0; i < numPQs; i++) {
        for (uint32_t b = 0; b < pqs[i]->maxSeconds; b++) {
            ArkimePQItem_t *item = 0;
            while (DLL_POP_HEAD(pql_, &pqs[i]->buckets[thread][b], item)) {
                HASH_REMOVE(pqh_, pqs[i]->keys[thread], item);
                ARKIME_TYPE_FREE(ArkimePQItem_t, item);
            }
        }
        pqs[i]->bucket0[thread] = 0;
    }
}
/******************************************************************************/

#ifdef TESTPQ
#include <assert.h>

SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t arkime_string_hash(const void *key)
{
    uint8_t *p = (uint8_t *)key;
    uint32_t n = 0;
    while (*p) {
        n = (n << 5) - n + *p;
        p++;
    }
    return n;
}
int callbacks;
LOCAL void pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    callbacks++;
}
ARKIME_LOCK_DEFINE(LOG);
ArkimeConfig_t         config;
time_t                 lastPacketSecs[ARKIME_MAX_PACKET_THREADS];
int main()
{
    ArkimePQ_t pq;
    ArkimeSession_t session1;
    session1.sessionId[0] = 2;
    session1.sessionId[1] = 'A';
    session1.thread = 0;
    config.packetThreads = 1;

    // Big jump
    LOG("**TEST1");
    arkime_pq_init(&pq, 1, pq_cb);
    lastPacketSecs[0] = 0; arkime_pq_run(0, 10);
    lastPacketSecs[0] = 1259261324; arkime_pq_run(0, 10);
    lastPacketSecs[0] = 1259261324; arkime_pq_upsert(&pq, &session1, 5, NULL);
    lastPacketSecs[0] = 1259261384; arkime_pq_upsert(&pq, &session1, 5, NULL);
    lastPacketSecs[0] = 1259261384; arkime_pq_upsert(&pq, &session1, 5, NULL);
    lastPacketSecs[0] = 1259261384; arkime_pq_run(0, 10);
    lastPacketSecs[0] = 1259261385; arkime_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);

    // Move from 1 to 0 and run
    LOG("**TEST2");
    callbacks = 0;
    pq.bucket0[0] = lastPacketSecs[0] = 100;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 1);
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 1);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 0);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    arkime_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);

    // Move from 0 to 1 and run
    LOG("**TEST3");
    callbacks = 0;
    pq.bucket0[0] = lastPacketSecs[0] = 100;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 1);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 0);
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    arkime_pq_run(0, 10);
    assert(callbacks == 0);
    lastPacketSecs[0] = 101;
    arkime_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);



    // Keep moving 0 ahead
    LOG("**TEST4");
    callbacks = 0;
    pq.bucket0[0] = lastPacketSecs[0] = 100;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 0, NULL);
    assert(callbacks == 0);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    arkime_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);

    // Keep re adding
    LOG("**TEST5");
    callbacks = 0;
    lastPacketSecs[0] = 100;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    assert(callbacks == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    arkime_pq_run(0, 10);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    arkime_pq_run(0, 10);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    arkime_pq_run(0, 10);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    arkime_pq_run(0, 10);
    lastPacketSecs[0]++;
    arkime_pq_upsert(&pq, &session1, 1, NULL);
    arkime_pq_run(0, 10);
    assert(callbacks == 0);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    lastPacketSecs[0]++;
    arkime_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);
}
#endif
