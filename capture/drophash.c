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
struct molochdrophashitem_t {
    MolochDropHashItem_t *dhg_next, *dhg_prev;
    MolochDropHashItem_t *hnext;
    uint8_t               key[MOLOCH_SESSIONID_LEN - 1];
    uint32_t              last;
    uint32_t              goodFor;
    uint16_t              port;
    uint16_t              flags;
};

struct molochdrophash_t {
    MolochDropHashItem_t **heads;
    uint32_t               cnt;
    uint16_t               num;
};

/******************************************************************************/
LOCAL inline uint32_t moloch_drophash_hash (const void *key, int len)
{
    uint32_t  h = 0;
    uint32_t *p = (uint32_t *)key;
    uint32_t *end = p + len/4;
    while (p < end) {
        h = (h + *p) * 0xc6a4a793;
        h ^= h >> 16;
        p += 1;
    }
    return h;
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

    MolochDropHash_t *hash;

    hash          = MOLOCH_TYPE_ALLOC(MolochDropHash_t);
    hash->num     = size;
    hash->heads   = MOLOCH_SIZE_ALLOC0("heads", size * sizeof(MolochDropHashItem_t *));
    hash->cnt     = 0;
    group->drops[port] = hash;
done:
    MOLOCH_UNLOCK(group->lock);
}
/******************************************************************************/
int moloch_drophash_add (MolochDropHashGroup_t *group, int port, const void *key, uint32_t current, uint32_t goodFor)
{
    if (!group->drops[port]) {
        moloch_drophash_make(group, port);
    }

    MolochDropHash_t *hash = group->drops[port];

    MolochDropHashItem_t *item;
    uint32_t              h;
    if (group->keyLen == 4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash(key, group->keyLen) % hash->num;

    MOLOCH_LOCK(group->lock);
    if (hash->heads[h]) {
        for (item = hash->heads[h]; item; item = item->hnext) {
            if (memcmp(key, item->key, group->keyLen) == 0) {
                MOLOCH_UNLOCK(group->lock);
                return 0;
            }
        }
    }

    item           = MOLOCH_TYPE_ALLOC(MolochDropHashItem_t);
    item->hnext    = hash->heads[h];
    item->flags    = 0;
    item->port     = port;
    memcpy(item->key, key, group->keyLen);
    item->last     = current;
    item->goodFor  = goodFor;
    hash->heads[h] = item;
    hash->cnt++;

    DLL_PUSH_TAIL(dhg_, group, item);
    group->changed++;
    MOLOCH_UNLOCK(group->lock);
    return 1;
}

/******************************************************************************/
int moloch_drophash_should_drop (MolochDropHashGroup_t *group, int port, void *key, uint32_t current)
{
    MolochDropHash_t *hash = group->drops[port];

    uint32_t              h;
    if (group->keyLen == 4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash(key, group->keyLen) % hash->num;

    if (!hash->heads[h])
        return 0;

    MolochDropHashItem_t *item;
    for (item = hash->heads[h]; item; item = item->hnext) {
        if (memcmp(key, item->key, group->keyLen) == 0) {

            // Same time as last time, drop
            if (likely(item->last == current))
                return 1;

            // Check if within the window, drop
            if (item->last + item->goodFor >= current) {
                item->last = current;
                return 1;
            }

            // Outside the window, need to remove, don't drop
            moloch_drophash_delete(group, port, key);
            return 0;
        }
    }
    return 0;
}
/******************************************************************************/
void moloch_drophash_free(void *ptr)
{
    MOLOCH_TYPE_FREE(MolochDropHashItem_t, ptr);
}
/******************************************************************************/
void moloch_drophash_delete (MolochDropHashGroup_t *group, int port, void *key)
{
    MolochDropHash_t *hash = group->drops[port];

    MolochDropHashItem_t *item, *parent = NULL;
    uint32_t              h;
    if (group->keyLen == 4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = moloch_drophash_hash(key, group->keyLen) % hash->num;

    if (!hash->heads[h])
        return;

    MOLOCH_LOCK(group->lock);
    for (item = hash->heads[h]; item; parent = item, item = item->hnext) {
        if (memcmp(key, item->key, group->keyLen) == 0) {
            hash->cnt--;
            if (parent) {
                parent->hnext = item->hnext;
            } else {
                hash->heads[h] = item->hnext;
            }
            DLL_REMOVE(dhg_, group, item);
            moloch_free_later(item, moloch_drophash_free);
            group->changed++;
            break;
        }
    }
    MOLOCH_UNLOCK(group->lock);
}

/******************************************************************************/
void moloch_drophash_init(MolochDropHashGroup_t *group, char *file, int keyLen)
{
    MOLOCH_LOCK_INIT(group->lock);
    group->keyLen = keyLen;
    DLL_INIT(dhg_, group);

    if (!file)
        return;

    group->file = g_strdup(file);

    if (!g_file_test(file, G_FILE_TEST_EXISTS))
        return;

    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);

    int      cnt;
    int      ver;
    char     fkeyLen;
    char     key[16];
    uint32_t last;
    uint32_t goodFor;
    uint16_t flags;
    uint16_t port;

    FILE *fp;
    if (!(fp = fopen(group->file, "r"))) {
        LOG("ERROR - Couldn't open `%s` to load drophash", group->file);
        return;
    }

    if (!fread(&ver, 4, 1, fp)) {
        fclose(fp);
        LOG("ERROR - `%s` corrupt", group->file);
        return;
    }

    if (ver != 2) {
        fclose(fp);
        LOG("ERROR - Unknown save file version %d for `%s`", ver, group->file);
        return;
    }

    if (!fread(&fkeyLen, 1, 1, fp)) {
        fclose(fp);
        LOG("ERROR - `%s` corrupt", group->file);
        return;
    }

    if (fkeyLen == 0)
        fkeyLen = 16;
    else if (fkeyLen == 1)
        fkeyLen = 4;

    if (fkeyLen != keyLen) {
        fclose(fp);
        LOG("ERROR - keyLen mismatch %d != %d", fkeyLen, keyLen);
        return;
    }

    if (!fread(&cnt, 4, 1, fp)) {
        fclose(fp);
        LOG("ERROR - `%s` corrupt", group->file);
        return;
    }
    for (int i = 0; i < cnt; i++) {
        int read = 0;
        read += fread(&port, 2, 1, fp);
        read += fread(key, keyLen, 1, fp);
        read += fread(&last, 4, 1, fp);
        read += fread(&goodFor, 4, 1, fp);
        read += fread(&flags, 2, 1, fp);

        if (read != 5) {
            LOG("ERROR - `%s` corrupt", group->file);
            break;
        }

        if (last + goodFor >= currentTime.tv_sec)
            moloch_drophash_add(group, port, key, last, goodFor);
    }
    group->changed = 0; // Reset changes so we don't save right away
    fclose(fp);
}

/******************************************************************************/
void moloch_drophash_save(MolochDropHashGroup_t *group)
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
    int ver = 2;

    fwrite(&ver, 4, 1, fp);
    fwrite(&group->keyLen, 1, 1, fp);
    fwrite(&group->dhg_count, 4, 1, fp);
    DLL_FOREACH(dhg_, group, item) {
        fwrite(&item->port, 2, 1, fp);
        fwrite(item->key, group->keyLen, 1, fp);
        fwrite(&item->last, 4, 1, fp);
        fwrite(&item->goodFor, 4, 1, fp);
        fwrite(&item->flags, 2, 1, fp);
    }
    MOLOCH_UNLOCK(group->lock);
    fclose(fp);
}
