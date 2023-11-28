/******************************************************************************/
/* dedup.c  -- dedup packets
 *
 * Copyright 2020 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Circular array of chained hashtable where space is preallocated and
 * there is a count array of elements per hashtable slot. We stop
 * searching the oldest hashtable and wipe its count array.
 *
 * We size the hashtable assuming DEDUP_SLOT_FACTOR elements per
 * slot, but actually allow DEDUP_SIZE_FACTOR elements.
 *
 * Currently use md5 on ip + tcp/udp hdr
 */

#include "arkime.h"
#include <openssl/md5.h>

extern ArkimeConfig_t       config;

// How many items in each hashtable we expect to be used
#define DEDUP_SLOT_FACTOR   15
// How many items in each hashtable we actually allow, must be less then 256
#define DEDUP_SIZE_FACTOR   20

LOCAL uint32_t              dedupSeconds;
LOCAL uint32_t              dedupSlots;
LOCAL uint32_t              dedupPackets;
LOCAL uint32_t              dedupSize;

typedef struct dedupsecond DedupSeconds_t;
struct dedupsecond {
    uint8_t        *md5s;
    uint8_t        *counts;
    uint32_t        tv_sec;
    uint32_t        count;
    char            error;
    ARKIME_LOCK_EXTERN(lock);
};

LOCAL DedupSeconds_t *seconds;

/******************************************************************************/
int arkime_dedup_should_drop (const ArkimePacket_t *packet, int headerLen)
{
    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);

    uint32_t secondSlot = currentTime.tv_sec % dedupSeconds;

    // Create hash, headerLen should be length of ip & tcp/udp header
    uint8_t md[16];
    MD5_CTX ctx;
    MD5_Init(&ctx);
    uint8_t *const ptr = packet->pkt + packet->ipOffset;
    if ((ptr[0] & 0xf0) == 0x40) {
        MD5_Update(&ctx, ptr, 8);
        // Skip TTL (1 byte)
        MD5_Update(&ctx, ptr + 9, 1);
        // Skip Header checksum (2 byte)
        MD5_Update(&ctx, ptr + 12, headerLen - 12);
    } else {
        MD5_Update(&ctx, ptr, 7);
        // Skip HOP
        MD5_Update(&ctx, ptr + 8, headerLen - 8);
    }
    MD5_Final(md, &ctx);
    int h = ((uint32_t *)md)[0] % dedupSlots;

    // First see if we need to clean up old slot, and block all new folks while we do
    // In theory no one should be using this slot because hadn't been searching it for a second.
    if (seconds[secondSlot].tv_sec != currentTime.tv_sec) {
        ARKIME_LOCK(seconds[secondSlot].lock);
        if (seconds[secondSlot].tv_sec != currentTime.tv_sec) { // Check critical section again
            if (seconds[secondSlot].error) {
                LOG ("WARNING - Ran out of room, increase dedupPackets to %u or above. pcount: %u", dedupSlots * DEDUP_SLOT_FACTOR + 1, seconds[secondSlot].count);
                seconds[secondSlot].error = 0;
            }
            memset(seconds[secondSlot].counts, 0, dedupSlots);
            seconds[secondSlot].count = 0;
            seconds[secondSlot].tv_sec = currentTime.tv_sec;
        }
        ARKIME_UNLOCK(seconds[secondSlot].lock);
    }

    // Search all valid slots
    for (uint32_t s = 0; s < dedupSeconds; s++) {

        // If slot is too old just skip it
        if (currentTime.tv_sec - dedupSeconds + 1 >= seconds[s].tv_sec ) {
            continue;
        }

        int count = seconds[s].counts[h];
        for (int c = 0; c < count; c++) {
            if (memcmp(md, seconds[s].md5s + 16 * (h * DEDUP_SIZE_FACTOR + c), 16) == 0) {
                return 1;
            }
        }
    }

    // Is there space to add
    if (seconds[secondSlot].counts[h] == DEDUP_SIZE_FACTOR) {
        seconds[secondSlot].error = 1;
        return 0;
    }

    // For now ignore the race condition of search between incr and copy
    int c = ARKIME_THREAD_INCROLD(seconds[secondSlot].counts[h]);
    memcpy(seconds[secondSlot].md5s + 16 * (h * DEDUP_SIZE_FACTOR + c), md, 16);
    ARKIME_THREAD_INCR(seconds[secondSlot].count);

    return 0;
}
/******************************************************************************/
void arkime_dedup_init()
{
    if (!config.enablePacketDedup)
        return;

    dedupSeconds   = arkime_config_int(NULL, "dedupSeconds", 2, 0, 30) + 1; // + 1 because a slot isn't active before being replaced
    dedupPackets   = arkime_config_int(NULL, "dedupPackets", 0xfffff, 0xffff, 0xffffff);
    dedupSlots     = arkime_get_next_prime(dedupPackets / DEDUP_SLOT_FACTOR);
    dedupSize      = dedupSlots * DEDUP_SIZE_FACTOR;

    if (config.debug)
        LOG("seconds = %u packets = %u slots = %u size = %u mem=%u", dedupSeconds, dedupPackets, dedupSlots, dedupSize, dedupSeconds * (dedupSlots + dedupSize * 16));

    seconds = ARKIME_SIZE_ALLOC0("dedup seconds", sizeof(DedupSeconds_t) * dedupSeconds);
    for (uint32_t i = 0; i < dedupSeconds; i++) {
        ARKIME_LOCK_INIT(seconds[i].lock);
        seconds[i].counts = ARKIME_SIZE_ALLOC("dedup counts", dedupSlots);
        seconds[i].md5s = ARKIME_SIZE_ALLOC("dedup counts", dedupSize * 16);
    }
}
/******************************************************************************/
void arkime_dedup_exit()
{
}
