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
 * Hash ip + tcp/udp hdr using either MD5 or XXH3_128bits.
 *
 * When dedup is enabled, arkime_dedup_should_drop is one of the
 * top CPU consumers at high packet rates. The ctrl array acts as
 * a cheap filter to avoid touching the larger hashes array on
 * non-matches. MD5 is used over XXH3 to keep the function small
 * and reduce instruction cache pressure.
 */

#include "arkime.h"

// MD5 is ~4% faster than XXH3 in production due to smaller icache footprint
#define DEDUP_USE_MD5

#ifdef DEDUP_USE_MD5
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/md5.h>
#else
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#if defined(__clang__)
#pragma clang attribute push(__attribute__((no_sanitize("integer"))), apply_to = function)
#include "xxhash.h"
#pragma clang attribute pop
#else
#include "xxhash.h"
#endif
#endif

extern ArkimeConfig_t       config;

// How many items in each hashtable we expect to be used, must be less than DEDUP_SIZE_FACTOR
#define DEDUP_SLOT_FACTOR   15
// How many items in each hashtable we actually allow, must be less than 256 and more than DEDUP_SLOT_FACTOR
#define DEDUP_SIZE_FACTOR   20

LOCAL uint32_t              dedupSeconds;
LOCAL uint32_t              dedupSlots;
LOCAL uint32_t              dedupSlotsMask;
LOCAL uint32_t              dedupPackets;
LOCAL uint32_t              dedupSize;

// VLAN collapse support for deduplication
LOCAL GHashTable           *dedupVlanCollapseTable;

// Function pointer for VLAN retrieval (set once at init, zero runtime overhead)
LOCAL uint16_t (*dedup_get_vlan)(const ArkimePacket_t *packet);

typedef struct dedupsecond {
    uint8_t        *ctrl;
    uint8_t        *hashes;
    uint8_t        *counts;
    uint32_t        tv_sec;
    char            error;
    ARKIME_LOCK_EXTERN(lock);
} DedupSeconds_t;

LOCAL DedupSeconds_t *seconds;

/******************************************************************************/
// VLAN getter functions - set once at init for zero runtime overhead
/******************************************************************************/
static inline uint16_t dedup_vlan_none(const ArkimePacket_t *UNUSED(packet))
{
    return 0;
}

/******************************************************************************/
static inline uint16_t dedup_vlan_original(const ArkimePacket_t *packet)
{
    return packet->vlan;
}

/******************************************************************************/
static inline uint16_t dedup_vlan_collapsed(const ArkimePacket_t *packet)
{
    if (!packet->vlan)
        return 0;

    if (dedupVlanCollapseTable) {
        uint16_t collapsed = GPOINTER_TO_UINT(g_hash_table_lookup(dedupVlanCollapseTable, GINT_TO_POINTER(packet->vlan)));
        if (collapsed) {
            return collapsed - 1; // Stored as value+1
        }
    }
    return packet->vlan;
}

/******************************************************************************/
int arkime_dedup_should_drop (const ArkimePacket_t *packet, int headerLen)
{
    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &currentTime);

    uint32_t secondSlot = currentTime.tv_sec % dedupSeconds;

    // Create hash, headerLen should be length of ip & tcp/udp header
    uint8_t md[16];
    const uint8_t *const ptr = packet->pkt + packet->ipOffset;

    const uint16_t vlan = dedup_get_vlan(packet);

#ifdef DEDUP_USE_MD5
    MD5_CTX ctx;
    MD5_Init(&ctx);
    if (vlan) {
        MD5_Update(&ctx, &vlan, sizeof(vlan));
    }
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
#else
    // Build contiguous buffer skipping volatile fields, then one-shot hash
    // +2 for optional VLAN prefix
    const int vlan_bytes = vlan ? 2 : 0;
    uint8_t buf[headerLen + 2];
    XXH128_hash_t hash;
    if (vlan) {
        memcpy(buf, &vlan, 2);
    }
    if ((ptr[0] & 0xf0) == 0x40) {
        memcpy(buf + vlan_bytes, ptr, 8);
        buf[vlan_bytes + 8] = ptr[9];  // protocol, skip TTL (1 byte)
        memcpy(buf + vlan_bytes + 9, ptr + 12, headerLen - 12); // skip checksum (2 bytes)
        hash = XXH3_128bits(buf, vlan_bytes + 9 + (headerLen - 12));
    } else {
        memcpy(buf + vlan_bytes, ptr, 7);
        memcpy(buf + vlan_bytes + 7, ptr + 8, headerLen - 8); // skip HOP (1 byte)
        hash = XXH3_128bits(buf, vlan_bytes + 7 + (headerLen - 8));
    }
    memcpy(md, &hash, 16);
#endif

    const int h = ((uint32_t *)md)[0] & dedupSlotsMask;
    const uint8_t tag = md[3];

    // First see if we need to clean up old slot, and block all new folks while we do
    // In theory no one should be using this slot because hadn't been searching it for a second.
    if (seconds[secondSlot].tv_sec != currentTime.tv_sec) {
        ARKIME_LOCK(seconds[secondSlot].lock);
        if (seconds[secondSlot].tv_sec != currentTime.tv_sec) { // Check critical section again inside the lock
            if (seconds[secondSlot].error) {
                uint32_t pcount = 0;
                for (uint32_t i = 0; i < dedupSlots; i++)
                    pcount += seconds[secondSlot].counts[i];
                LOG ("WARNING - Ran out of room, increase dedupPackets to %u or above. pcount: %u", (dedupSlots + 1) * DEDUP_SLOT_FACTOR, pcount);
                seconds[secondSlot].error = 0;
            }
            memset(seconds[secondSlot].counts, 0, dedupSlots);
            seconds[secondSlot].tv_sec = currentTime.tv_sec;
        }
        ARKIME_UNLOCK(seconds[secondSlot].lock);
    }

    // Search all valid slots
    for (uint32_t s = 0; s < dedupSeconds; s++) {

        // If slot is too old just skip it
        if (currentTime.tv_sec - dedupSeconds + 1 >= seconds[s].tv_sec) {
            continue;
        }

        int count = seconds[s].counts[h];
        uint8_t *ctrl_base = seconds[s].ctrl + h * DEDUP_SIZE_FACTOR;
        for (int c = 0; c < count; c++) {
            if (tag == ctrl_base[c] && memcmp(md, seconds[s].hashes + 16 * (h * DEDUP_SIZE_FACTOR + c), 16) == 0) {
                return 1;
            }
        }
    }

    // Is there space to add
    if (seconds[secondSlot].counts[h] >= DEDUP_SIZE_FACTOR) {
        seconds[secondSlot].error = 1;
        return 0;
    }

    // Race condition: a reader may see incremented count before stores complete.
    // This is acceptable - fail-open means a duplicate packet may slip through briefly.
    int c = ARKIME_THREAD_INCROLD(seconds[secondSlot].counts[h]);
    if (c >= DEDUP_SIZE_FACTOR) {
        seconds[secondSlot].error = 1;
        return 0;
    }
    seconds[secondSlot].ctrl[h * DEDUP_SIZE_FACTOR + c] = tag;
    memcpy(seconds[secondSlot].hashes + 16 * (h * DEDUP_SIZE_FACTOR + c), md, 16);

    return 0;
}
/******************************************************************************/
LOCAL void arkime_dedup_load_vlan_collapse()
{
    gsize keys_len;
    gchar **keys = arkime_config_section_keys(NULL, "vlan-vni-collapse", &keys_len);
    if (keys_len > 0) {
        dedupVlanCollapseTable = g_hash_table_new(g_direct_hash, g_direct_equal);
    }

    for (int i = 0; i < (int)keys_len; i++) {
        char *value = arkime_config_section_str(NULL, "vlan-vni-collapse", keys[i], NULL);
        char **values = g_strsplit(value, ",", 0);

        uint64_t key = atoi(keys[i]) + 1; // Store as key+1 so we can detect 0 as "not found"
        for (int j = 0; values[j]; j++) {
            uint64_t ivalue = atoi(values[j]);
            g_hash_table_insert(dedupVlanCollapseTable, GINT_TO_POINTER(ivalue), GINT_TO_POINTER(key));
        }
        g_strfreev(values);
        g_free(value);
    }
    g_strfreev(keys);
}
/******************************************************************************/
void arkime_dedup_init()
{
    if (!config.enablePacketDedup)
        return;

    dedupSeconds   = arkime_config_int(NULL, "dedupSeconds", 2, 0, 30) + 1; // + 1 because a slot isn't active before being replaced
    dedupPackets   = arkime_config_int(NULL, "dedupPackets", 0xfffff, 0xffff, 0xffffff);
    dedupSlots     = arkime_get_next_powerof2(dedupPackets / DEDUP_SLOT_FACTOR);
    dedupSlotsMask = dedupSlots - 1;
    dedupSize      = dedupSlots * DEDUP_SIZE_FACTOR;

    if (config.debug)
        LOG("seconds = %u packets = %u slots = %u size = %u mem=%u", dedupSeconds, dedupPackets, dedupSlots, dedupSize, dedupSeconds * (dedupSlots + dedupSize + dedupSize * 16));

    seconds = ARKIME_SIZE_ALLOC0("dedup seconds", sizeof(DedupSeconds_t) * dedupSeconds);
    for (uint32_t i = 0; i < dedupSeconds; i++) {
        ARKIME_LOCK_INIT(seconds[i].lock);
        seconds[i].counts = ARKIME_SIZE_ALLOC("dedup counts", dedupSlots);
        seconds[i].ctrl = ARKIME_SIZE_ALLOC("dedup ctrl", dedupSize);
        seconds[i].hashes = ARKIME_SIZE_ALLOC("dedup hashes", dedupSize * 16);
    }

    // Set up VLAN mode for deduplication (done once at init for zero runtime overhead)
    char *vlanMode = arkime_config_str(NULL, "dedupVlanMode", "none");
    if (strcmp(vlanMode, "none") == 0) {
        dedup_get_vlan = dedup_vlan_none;
        if (config.debug)
            LOG("Dedup VLAN mode: none (VLAN not included in dedup hash)");
    } else if (strcmp(vlanMode, "originalVlan") == 0) {
        dedup_get_vlan = dedup_vlan_original;
        if (config.debug)
            LOG("Dedup VLAN mode: originalVlan (packet VLAN included in dedup hash)");
    } else if (strcmp(vlanMode, "collapsedVlan") == 0) {
        arkime_dedup_load_vlan_collapse();
        dedup_get_vlan = dedup_vlan_collapsed;
        if (config.debug)
            LOG("Dedup VLAN mode: collapsedVlan (collapsed VLAN included in dedup hash)");
    } else {
        CONFIGEXIT("dedupVlanMode must be 'none', 'originalVlan', or 'collapsedVlan', not '%s'", vlanMode);
    }
    g_free(vlanMode);
}
/******************************************************************************/
void arkime_dedup_exit()
{
}
