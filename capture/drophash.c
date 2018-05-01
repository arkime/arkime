/* drophash.c - simple hash that locks on writes but not on reads
 *              used for dropping packets by ip:port before the copy
 *
 * Copyright 2018 AOL Inc. All rights reserved.
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

typedef struct molochdrophashitem_t MolochDropHashItem_t;
struct molochdrophashitem_t {
    MolochDropHashItem_t *next;
    MolochDropHashItem_t *dnext;
    uint8_t               key[16];
    uint32_t              value;
};

typedef struct molochdrophash_t MolochDropHash_t;
struct molochdrophash_t {
    MolochDropHashItem_t **heads;
    MolochDropHashItem_t  *deleted;
    char                   isIp4;
    int                    num;
    struct timespec        lastDelete;
    MOLOCH_LOCK_EXTERN(lock);
};


MolochDropHash_t *moloch_drophash_init (int num, char isIp4)
{
    MolochDropHash_t *hash;
    hash           = MOLOCH_TYPE_ALLOC(MolochDropHash_t);
    hash->num     = num;
    hash->isIp4   = isIp4;
    hash->heads   = MOLOCH_SIZE_ALLOC0("heads", num * sizeof(MolochDropHashItem_t *));
    hash->deleted = NULL;
    MOLOCH_LOCK_INIT(hash->lock);
    return hash;
}

static inline uint32_t moloch_drophash_hash6 (const void *key)
{
    uint32_t  h = 0;
    uint32_t *p = (uint32_t *)key;
    uint32_t *end = p + 4;
    while (p < end) {
        h = (h + *p) * 0xc6a4a793;
        h ^= h >> 16;
        p += 1;
    }
    return h;
}

int moloch_drophash_add (MolochDropHash_t *hash, const void *key, uint32_t value)
{
    MolochDropHashItem_t *item;
    uint32_t              h;
    if (hash->isIp4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash6(key) % hash->num;

    MOLOCH_LOCK(hash->lock);
    if (hash->heads[h]) {
        for (item = hash->heads[h]; item; item = item->next) {
            if (memcmp(key, item->key, hash->isIp4?4:16) == 0) {
                MOLOCH_UNLOCK(hash->lock);
                return 0;
            }
        }
    }

    item           = MOLOCH_TYPE_ALLOC(MolochDropHashItem_t);
    item->next     = hash->heads[h];
    item->dnext    = NULL;
    memcpy(item->key, key, hash->isIp4?4:16);
    item->value    = value;
    hash->heads[h] = item;
    MOLOCH_UNLOCK(hash->lock);
    return 1;
}

uint32_t moloch_drophash_get (MolochDropHash_t *hash, void *key)
{
    uint32_t              h;
    if (hash->isIp4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash6(key) % hash->num;

    if (!hash->heads[h])
        return 0;

    MolochDropHashItem_t *item;
    for (item = hash->heads[h]; item; item = item->next) {
        if (memcmp(key, item->key, hash->isIp4?4:16) == 0) {
            return item->value;
        }
    }
    return 0;
}

void moloch_drophash_delete (MolochDropHash_t *hash, void *key)
{
    MolochDropHashItem_t *item, *parent = NULL;
    uint32_t              h;
    if (hash->isIp4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash6(key) % hash->num;

    if (!hash->heads[h])
        return;

    MOLOCH_LOCK(hash->lock);
    // Cleanup items waiting to be freed
    if (hash->deleted) {
        struct timespec currentTime;
        clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);
        if (currentTime.tv_sec > hash->lastDelete.tv_sec + 10) {
            do {
                item = hash->deleted;
                hash->deleted = item->dnext;
                MOLOCH_TYPE_FREE(MolochDropHashItem_t, item);
            } while (hash->deleted);
        }
    }

    for (item = hash->heads[h]; item; parent = item, item = item->next) {
        if (memcmp(key, item->key, hash->isIp4?4:16) == 0) {
            if (parent) {
                parent->next = item->next;
            } else {
                hash->heads[h] = item->next;
            }
            item->dnext = hash->deleted;
            hash->deleted = item;
            clock_gettime(CLOCK_REALTIME_COARSE, &hash->lastDelete);
            break;
        }
    }
    MOLOCH_UNLOCK(hash->lock);
}
