/* drophash.c - simple hash that locks on writes but not on reads
 *              used for dropping packets by ip:port before the packet copy
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
#include "dll.h"

/******************************************************************************/
extern MolochConfig_t        config;

/******************************************************************************/
typedef struct molochdrophashitem_t MolochDropHashItem_t;
struct molochdrophashitem_t {
    MolochDropHashItem_t *dhg_next, *dhg_prev;
    MolochDropHashItem_t *hnext, *dnext;
    uint8_t               key[16];
    uint32_t              expire;
    uint16_t              port;
    uint16_t              flags;
};

typedef struct molochdrophash_t MolochDropHash_t;
struct molochdrophash_t {
    MolochDropHashItem_t **heads;
    MolochDropHashItem_t  *deleted;
    MOLOCH_LOCK_EXTERN(lock);
    struct timespec        lastDelete;
    uint32_t               cnt;
    uint16_t               num;
    char                   isIp4;
};

/******************************************************************************/
MolochDropHash_t *moloch_drophash_init (int num, char isIp4)
{
    MolochDropHash_t *hash;
    hash           = MOLOCH_TYPE_ALLOC(MolochDropHash_t);
    hash->num     = num;
    hash->isIp4   = isIp4;
    hash->heads   = MOLOCH_SIZE_ALLOC0("heads", num * sizeof(MolochDropHashItem_t *));
    hash->deleted = NULL;
    hash->cnt     = 0;
    MOLOCH_LOCK_INIT(hash->lock);
    return hash;
}

/******************************************************************************/
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

/******************************************************************************/
LOCAL void moloch_drophash_clean_locked (MolochDropHash_t *hash)
{
    MolochDropHashItem_t *item;
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
}

/******************************************************************************/
LOCAL void moloch_drophash_make(MolochDropHashGroup_t *group, int port)
{
    int size;

    switch (port) {
    case 80:
    case 443:
    case 25:
        size = 7919;
        break;
    default:
        size = 409;
        break;
    }

    MOLOCH_LOCK(group->lock);
    if (group->drops[port])
        goto done;
    group->drops[port] = moloch_drophash_init(size, group->isIp4);
done:
    MOLOCH_UNLOCK(group->lock);
}
/******************************************************************************/
int moloch_drophash_add (MolochDropHashGroup_t *group, int port, const void *key, uint32_t expire)
{
    if (!group->drops[port]) {
        moloch_drophash_make(group, port);
    }

    MolochDropHash_t *hash = group->drops[port];

    MolochDropHashItem_t *item;
    uint32_t              h;
    if (hash->isIp4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash6(key) % hash->num;

    MOLOCH_LOCK(hash->lock);
    moloch_drophash_clean_locked(hash);
    if (hash->heads[h]) {
        for (item = hash->heads[h]; item; item = item->hnext) {
            if (memcmp(key, item->key, hash->isIp4?4:16) == 0) {
                MOLOCH_UNLOCK(hash->lock);
                return 0;
            }
        }
    }

    item           = MOLOCH_TYPE_ALLOC(MolochDropHashItem_t);
    item->hnext    = hash->heads[h];
    item->dnext    = NULL;
    item->flags    = 0;
    item->port     = port;
    memcpy(item->key, key, hash->isIp4?4:16);
    item->expire   = expire;
    hash->heads[h] = item;
    hash->cnt++;

    MOLOCH_LOCK(group->lock);
    DLL_PUSH_TAIL(dhg_, group, item);
    group->changed++;
    MOLOCH_UNLOCK(group->lock);
    MOLOCH_UNLOCK(hash->lock);
    return 1;
}

/******************************************************************************/
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
    for (item = hash->heads[h]; item; item = item->hnext) {
        if (memcmp(key, item->key, hash->isIp4?4:16) == 0) {
            return item->expire;
        }
    }
    return 0;
}

/******************************************************************************/
void moloch_drophash_delete (MolochDropHashGroup_t *group, int port, void *key)
{
    MolochDropHash_t *hash = group->drops[port];

    MolochDropHashItem_t *item, *parent = NULL;
    uint32_t              h;
    if (hash->isIp4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash6(key) % hash->num;

    if (!hash->heads[h])
        return;

    MOLOCH_LOCK(hash->lock);
    moloch_drophash_clean_locked(hash);

    for (item = hash->heads[h]; item; parent = item, item = item->hnext) {
        if (memcmp(key, item->key, hash->isIp4?4:16) == 0) {
            hash->cnt--;
            if (parent) {
                parent->hnext = item->hnext;
            } else {
                hash->heads[h] = item->hnext;
            }
            item->dnext = hash->deleted;
            hash->deleted = item;
            clock_gettime(CLOCK_REALTIME_COARSE, &hash->lastDelete);
            MOLOCH_LOCK(group->lock);
            DLL_REMOVE(dhg_, group, item);
            group->changed++;
            MOLOCH_UNLOCK(group->lock);
            break;
        }
    }
    MOLOCH_UNLOCK(hash->lock);
}

/******************************************************************************/
void moloch_drophashgroup_init(MolochDropHashGroup_t *group, char *file, int isIp4)
{
    group->isIp4 = isIp4;
    DLL_INIT(dhg_, group);

    if (!file)
        return;

    group->file = g_strdup(file);
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);

    int      i;
    int      cnt;
    int      ver;
    char     key[16];
    uint32_t expire;
    uint16_t flags;
    uint16_t port;

    FILE *fp;
    if (!(fp = fopen(group->file, "r"))) {
        LOG("ERROR - Couldn't open `%s` to load drophash", group->file);
        return;
    }

    fread(&ver, 4, 1, fp);
    if (ver != 1) {
        fclose(fp);
        LOG("ERROR - Unknown save file version %d for `%s`", ver, group->file);
        return;
    }

    fread(&cnt, 4, 1, fp);
    for (i = 0; i < cnt; i++) {
        fread(&port, 2, 1, fp);
        fread(key, isIp4?4:16, 1, fp);
        fread(&expire, 4, 1, fp);
        fread(&flags, 2, 1, fp);
        if (expire > currentTime.tv_sec)
            moloch_drophash_add(group, port, key, expire);
    }
    group->changed = 0; // Reset changes so we don't save right away
    fclose(fp);
}

/******************************************************************************/
void moloch_drophashgroup_save(MolochDropHashGroup_t *group)
{
    FILE *fp;
    MolochDropHashItem_t *item;

    if (!group->file)
        return;

    if (!(fp = fopen(group->file, "w"))) {
        LOG("ERROR - Couldn't open `%s` to save drophash", group->file);
        return;
    }
    MOLOCH_LOCK(group->lock);
    group->changed = 0;
    int ver = 1;

    fwrite(&ver, 4, 1, fp);
    fwrite(&group->dhg_count, 4, 1, fp);
    DLL_FOREACH(dhg_, group, item) {
        fwrite(&item->port, 2, 1, fp);
        fwrite(item->key, group->isIp4?4:16, 1, fp);
        fwrite(&item->expire, 4, 1, fp);
        fwrite(&item->flags, 2, 1, fp);
    }
    MOLOCH_UNLOCK(group->lock);
    fclose(fp);
}
