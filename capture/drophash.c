/* drophash.c - simple hash that locks on writes but not on reads
 *              used for dropping packets by ip:port before the packet copy
 *
 * Copyright 2018 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "dll.h"

/******************************************************************************/
extern ArkimeConfig_t        config;

/******************************************************************************/
struct arkimedrophashitem_t {
    ArkimeDropHashItem_t *dhg_next, *dhg_prev;
    ArkimeDropHashItem_t *hnext;
    uint8_t               key[ARKIME_SESSIONID_LEN - 1];
    uint32_t              last;
    uint32_t              goodFor;
    uint16_t              port;
    uint16_t              flags;
};

struct arkimedrophash_t {
    ArkimeDropHashItem_t **heads;
    uint32_t               cnt;
    uint16_t               num;
};

/******************************************************************************/
LOCAL inline uint32_t arkime_drophash_hash (const void *key, int len)
{
    uint32_t  h = 0;
    uint32_t *p = (uint32_t *)key;
    uint32_t *end = p + len / 4;
    while (p < end) {
        h = (h + *p) * 0xc6a4a793;
        h ^= h >> 16;
        p += 1;
    }
    return h;
}

/******************************************************************************/
LOCAL void arkime_drophash_make(ArkimeDropHashGroup_t *group, int port)
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

    ARKIME_LOCK(group->lock);
    if (group->drops[port])
        goto done;

    ArkimeDropHash_t *hash;

    hash          = ARKIME_TYPE_ALLOC(ArkimeDropHash_t);
    hash->num     = size;
    hash->heads   = ARKIME_SIZE_ALLOC0("heads", size * sizeof(ArkimeDropHashItem_t *));
    hash->cnt     = 0;
    group->drops[port] = hash;
done:
    ARKIME_UNLOCK(group->lock);
}
/******************************************************************************/
int arkime_drophash_add (ArkimeDropHashGroup_t *group, int port, const void *key, uint32_t current, uint32_t goodFor)
{
    if (!group->drops[port]) {
        arkime_drophash_make(group, port);
    }

    ArkimeDropHash_t *hash = group->drops[port];

    ArkimeDropHashItem_t *item;
    uint32_t              h;
    if (group->keyLen == 4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = arkime_drophash_hash(key, group->keyLen) % hash->num;

    ARKIME_LOCK(group->lock);
    if (hash->heads[h]) {
        for (item = hash->heads[h]; item; item = item->hnext) {
            if (memcmp(key, item->key, group->keyLen) == 0) {
                ARKIME_UNLOCK(group->lock);
                return 0;
            }
        }
    }

    item           = ARKIME_TYPE_ALLOC(ArkimeDropHashItem_t);
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
    ARKIME_UNLOCK(group->lock);
    return 1;
}

/******************************************************************************/
int arkime_drophash_should_drop (ArkimeDropHashGroup_t *group, int port, void *key, uint32_t current)
{
    ArkimeDropHash_t *hash = group->drops[port];

    uint32_t              h;
    if (group->keyLen == 4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = arkime_drophash_hash(key, group->keyLen) % hash->num;

    if (!hash->heads[h])
        return 0;

    ArkimeDropHashItem_t *item;
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
            arkime_drophash_delete(group, port, key);
            return 0;
        }
    }
    return 0;
}
/******************************************************************************/
void arkime_drophash_free(void *ptr)
{
    ARKIME_TYPE_FREE(ArkimeDropHashItem_t, ptr);
}
/******************************************************************************/
void arkime_drophash_delete (ArkimeDropHashGroup_t *group, int port, void *key)
{
    ArkimeDropHash_t *hash = group->drops[port];

    ArkimeDropHashItem_t *item, *parent = NULL;
    uint32_t              h;
    if (group->keyLen == 4)
        h = (*(uint32_t *)key) % hash->num;
    else
        h = arkime_drophash_hash(key, group->keyLen) % hash->num;

    if (!hash->heads[h])
        return;

    ARKIME_LOCK(group->lock);
    for (item = hash->heads[h]; item; parent = item, item = item->hnext) {
        if (memcmp(key, item->key, group->keyLen) == 0) {
            hash->cnt--;
            if (parent) {
                parent->hnext = item->hnext;
            } else {
                hash->heads[h] = item->hnext;
            }
            DLL_REMOVE(dhg_, group, item);
            arkime_free_later(item, arkime_drophash_free);
            group->changed++;
            break;
        }
    }
    ARKIME_UNLOCK(group->lock);
}

/******************************************************************************/
void arkime_drophash_init(ArkimeDropHashGroup_t *group, char *file, int keyLen)
{
    ARKIME_LOCK_INIT(group->lock);
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
            arkime_drophash_add(group, port, key, last, goodFor);
    }
    group->changed = 0; // Reset changes so we don't save right away
    fclose(fp);
}

/******************************************************************************/
void arkime_drophash_save(ArkimeDropHashGroup_t *group)
{
    FILE *fp;
    ArkimeDropHashItem_t *item;

    if (!group->file)
        return;

    if (!(fp = fopen(group->file, "w"))) {
        LOG("ERROR - Couldn't open `%s` to save drophash", group->file);
        return;
    }
    ARKIME_LOCK(group->lock);
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
    ARKIME_UNLOCK(group->lock);
    fclose(fp);
}
