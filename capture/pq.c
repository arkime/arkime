/* pq.c  -- Priority Q
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

#include "moloch.h"

/******************************************************************************/
extern MolochConfig_t        config;
extern time_t                lastPacketSecs[MOLOCH_MAX_PACKET_THREADS];

LOCAL int         numPQs;
LOCAL MolochPQ_t *pqs[10];
LOCAL uint32_t    pqEntries;


/******************************************************************************/
typedef struct molochpqitem {
    struct molochpqitem *pql_next, *pql_prev;
    struct molochpqitem *pqh_next, *pqh_prev;

    MolochSession_t     *session;
    void                *uw;
    time_t               expire;
    uint32_t             pqh_hash;
    uint32_t             pqh_bucket;
} MolochPQItem_t;

typedef struct {
    struct molochpqitem *pql_next, *pql_prev;
    struct molochpqitem *pqh_next, *pqh_prev;
    int                  pql_count;
    int                  pqh_count;
} MolochPQHead_t;

typedef HASH_VAR(s_, MolochPQHash_t, MolochPQHead_t, 51);

struct MolochPQ_t {
    MolochPQHead_t     *buckets[MOLOCH_MAX_PACKET_THREADS];
    MolochPQHash_t      keys[MOLOCH_MAX_PACKET_THREADS];
    MolochPQ_cb         cb;
    time_t              bucket0[MOLOCH_MAX_PACKET_THREADS];
    int                 maxSeconds;
};

/******************************************************************************/
LOCAL int moloch_pq_cmp(const void *keyv, const MolochPQItem_t *item)
{
    return memcmp(keyv, item->session->sessionId, MIN(((uint8_t *)keyv)[0], item->session->sessionId[0])) == 0;
}
/******************************************************************************/
MolochPQ_t *moloch_pq_alloc(int maxSeconds, MolochPQ_cb cb)
{
    MolochPQ_t *pq = MOLOCH_TYPE_ALLOC0(MolochPQ_t);

    pq->maxSeconds = maxSeconds;
    int t, i;
    for (t = 0; t < config.packetThreads; t++) {
        pq->bucket0[t] = 0;
        pq->buckets[t] = malloc(sizeof(MolochPQHead_t) * (maxSeconds + 1));
        HASH_INIT(pqh_, pq->keys[t], moloch_string_hash, (HASH_CMP_FUNC)moloch_pq_cmp);
        for (i = 0; i <= maxSeconds; i++)
            DLL_INIT(pql_, &pq->buckets[t][i]);
    }
    pq->cb = cb;
    pqs[numPQs] = pq;
    numPQs++;

    return pq;
}
/******************************************************************************/
LOCAL void moloch_pq_shift(MolochPQ_t *pq, int thread)
{
    int shift = lastPacketSecs[thread] - pq->bucket0[thread];

    if (shift > pq->maxSeconds)
        shift = pq->maxSeconds;

    for (int i = 1; i <= pq->maxSeconds; i++) {
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
void moloch_pq_upsert(MolochPQ_t *pq, MolochSession_t *session, int timeout, void *uw)
{
    // timeout is relative to lastPacketSecs, figure out time
    time_t expire = lastPacketSecs[session->thread] + timeout;

    // Now recalculate bucket0 if we need to
    if (lastPacketSecs[session->thread] > pq->bucket0[session->thread])
        moloch_pq_shift(pq, session->thread);

    // Now make timeout relative to bucket0
    timeout = expire - pq->bucket0[session->thread];

    // In the past, just run now
    if (timeout < 0)
        timeout = 0;

    // To far in the future for this PQ
    if (timeout > pq->maxSeconds)
        timeout = pq->maxSeconds;

    MolochPQItem_t *item;
    HASH_FIND(pqh_, (pq->keys[session->thread]), session->sessionId, item);
    if (item) {
        int bucket = item->expire - pq->bucket0[session->thread];
        if (bucket < 0) bucket = 0;

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
    item = MOLOCH_TYPE_ALLOC(MolochPQItem_t);
    DLL_PUSH_TAIL(pql_, &pq->buckets[session->thread][timeout], item);
    HASH_ADD(pqh_, pq->keys[session->thread], session->sessionId, item);
    item->expire = expire;
    item->session = session;
    item->uw = uw;
    session->pq = 1;
    pqEntries++;
}
/******************************************************************************/
void moloch_pq_remove(MolochPQ_t *pq, MolochSession_t *session)
{
    MolochPQItem_t *item;
    HASH_FIND(pqh_, pq->keys[session->thread], session->sessionId, item);
    if (!item)
        return;

    int bucket = item->expire - pq->bucket0[session->thread];
    if (bucket < 0) bucket = 0;
    DLL_REMOVE(pql_, &pq->buckets[session->thread][bucket], item);
    HASH_REMOVE(pqh_, pq->keys[session->thread], item);
    MOLOCH_TYPE_FREE(MolochPQItem_t, item);
    pqEntries--;
}
/******************************************************************************/
void moloch_pq_run(int thread, int max)
{
    if (pqEntries == 0)
        return;

    int i;
    for (i = 0; i < numPQs; i++) {
        if (lastPacketSecs[thread] > pqs[i]->bucket0[thread])
            moloch_pq_shift(pqs[i], thread);

        int cnt = max;
        MolochPQItem_t *item = 0;
        while (cnt > 0 && DLL_POP_HEAD(pql_, &pqs[i]->buckets[thread][0], item)) {
            HASH_REMOVE(pqh_, pqs[i]->keys[thread], item);
            pqs[i]->cb(item->session, item->uw);
            MOLOCH_TYPE_FREE(MolochPQItem_t, item);
            cnt--;
        }
    }
}
/******************************************************************************/
/* Remove this session from all PQs */
void moloch_pq_free(MolochSession_t *session)
{
    int i;
    for (i = 0; i < numPQs; i++) {
        moloch_pq_remove(pqs[i], session);
    }
}
/******************************************************************************/
/* Reset the bucket0 time */
void moloch_pq_flush()
{
    int i, t, b;
    for (i = 0; i < numPQs; i++) {
        for (t = 0; t < config.packetThreads; t++) {
            for (b = 0; b < pqs[i]->maxSeconds; b++) {
                MolochPQItem_t *item = 0;
                while (DLL_POP_HEAD(pql_, &pqs[i]->buckets[t][b], item)) {
                    HASH_REMOVE(pqh_, pqs[i]->keys[t], item);
                    MOLOCH_TYPE_FREE(MolochPQItem_t, item);
                }
            }
            pqs[i]->bucket0[t] = 0;
        }
    }
}
/******************************************************************************/

#ifdef TESTPQ
#include <assert.h>

SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
uint32_t moloch_string_hash(const void *key)
{
    unsigned char *p = (unsigned char *)key;
    uint32_t n = 0;
    while (*p) {
        n = (n << 5) - n + *p;
        p++;
    }
    return n;
}
int callbacks;
LOCAL void pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    callbacks++;
}
MOLOCH_LOCK_DEFINE(LOG);
MolochConfig_t         config;
time_t                 lastPacketSecs[MOLOCH_MAX_PACKET_THREADS];
int main()
{
    MolochPQ_t pq;
    MolochSession_t session1;
    session1.sessionId[0] = 2;
    session1.sessionId[1] = 'A';
    session1.thread = 0;
    config.packetThreads = 1;

    // Big jump
    LOG("**TEST1");
    moloch_pq_init(&pq, 1, pq_cb);
    lastPacketSecs[0] = 0; moloch_pq_run(0, 10);
    lastPacketSecs[0] = 1259261324; moloch_pq_run(0, 10);
    lastPacketSecs[0] = 1259261324; moloch_pq_upsert(&pq, &session1, 5, NULL);
    lastPacketSecs[0] = 1259261384; moloch_pq_upsert(&pq, &session1, 5, NULL);
    lastPacketSecs[0] = 1259261384; moloch_pq_upsert(&pq, &session1, 5, NULL);
    lastPacketSecs[0] = 1259261384; moloch_pq_run(0, 10);
    lastPacketSecs[0] = 1259261385; moloch_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);

    // Move from 1 to 0 and run
    LOG("**TEST2");
    callbacks = 0;
    pq.bucket0[0] = lastPacketSecs[0] = 100;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 1);
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 1);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 0);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    moloch_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);

    // Move from 0 to 1 and run
    LOG("**TEST3");
    callbacks = 0;
    pq.bucket0[0] = lastPacketSecs[0] = 100;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 1);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 0);
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    moloch_pq_run(0, 10);
    assert(callbacks == 0);
    lastPacketSecs[0] = 101;
    moloch_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);



    // Keep moving 0 ahead
    LOG("**TEST4");
    callbacks = 0;
    pq.bucket0[0] = lastPacketSecs[0] = 100;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 0, NULL);
    assert(callbacks == 0);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    moloch_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);

    // Keep re adding
    LOG("**TEST5");
    callbacks = 0;
    lastPacketSecs[0] = 100;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    assert(callbacks == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][0]) == 0);
    assert(DLL_COUNT(pql_, &pq.buckets[0][1]) == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    moloch_pq_run(0, 10);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    moloch_pq_run(0, 10);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    moloch_pq_run(0, 10);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    moloch_pq_run(0, 10);
    lastPacketSecs[0]++;
    moloch_pq_upsert(&pq, &session1, 1, NULL);
    moloch_pq_run(0, 10);
    assert(callbacks == 0);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 1);
    lastPacketSecs[0]++;
    moloch_pq_run(0, 10);
    assert(callbacks == 1);
    assert(HASH_COUNT(pqh_, pq.keys[0]) == 0);
}
#endif
