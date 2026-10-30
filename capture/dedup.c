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
 * Hash ip + tcp/udp hdr using keyed SipHash-1-3, or MD5 with -DDEDUP_USE_MD5.
 *
 * When dedup is enabled, arkime_dedup_should_drop is one of the
 * top CPU consumers at high packet rates. The ctrl array acts as
 * a cheap filter to avoid touching the larger hashes array on
 * non-matches.
 */

#include <inttypes.h>
#include "arkime.h"
#include "siphash.h"

// SipHash-1-3 is ~50ns per packet cheaper than MD5 with 1 or 4 reader threads,
// and keyed so packets can't be crafted to collide with real ones.
#ifdef DEDUP_USE_MD5
#define OPENSSL_SUPPRESS_DEPRECATED
#include <openssl/md5.h>
#endif

extern ArkimeConfig_t          config;
extern GHashTable             *collapseTable;
extern ArkimeSessionIdTracking sessionIdTracking;
extern uint64_t                hashKey[2];

// How many items in each hashtable we expect to be used, must be less than DEDUP_SIZE_FACTOR
#define DEDUP_SLOT_FACTOR   15
// How many items in each hashtable we actually allow, must be less than 256 and more than DEDUP_SLOT_FACTOR
#define DEDUP_SIZE_FACTOR   20

LOCAL uint32_t              dedupSeconds;
LOCAL uint32_t              dedupSlots;
LOCAL uint32_t              dedupSlotsMask;
LOCAL uint32_t              dedupPackets;
LOCAL uint32_t              dedupSize;

typedef enum { DEDUP_PLAIN, DEDUP_VLAN, DEDUP_VNI } DedupMode;
LOCAL DedupMode             dedupMode;

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
int arkime_dedup_should_drop(const ArkimePacket_t *packet, int headerLen)
{
    // Cap headerLen so the headerLen-derived copies below can't overflow the
    // fixed-size md[]/buf[] stack buffers
    if (headerLen <= 0 || headerLen > 256) {
        return 0;
    }

    struct timespec currentTime;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &currentTime);

    uint32_t secondSlot = (uint32_t)currentTime.tv_sec % dedupSeconds;

    // Create hash, headerLen should be length of ip & tcp/udp header
    uint8_t md[16];
    const uint8_t *const ptr = packet->pkt + packet->ipOffset;

    uint8_t prefix[4];
    int prefix_len = 0;
    switch (dedupMode) {
    case DEDUP_VLAN: {
        uint16_t v = packet->vlan;
        if (v && collapseTable) {
            uint16_t c = GPOINTER_TO_UINT(g_hash_table_lookup(collapseTable, GINT_TO_POINTER(v)));
            if (c) v = c - 1;
        }
        if (v) {
            memcpy(prefix, &v, 2);
            prefix_len = 2;
        }
        break;
    }
    case DEDUP_VNI: {
        uint32_t v = packet->vni;
        if (v && collapseTable) {
            uint32_t c = GPOINTER_TO_UINT(g_hash_table_lookup(collapseTable, GINT_TO_POINTER(v)));
            if (c) v = c - 1;
        }
        if (v) {
            memcpy(prefix, &v, 4);
            prefix_len = 4;
        }
        break;
    }
    default:
        break;
    }

#ifdef DEDUP_USE_MD5
    MD5_CTX ctx;
    MD5_Init(&ctx);
    if (prefix_len) {
        MD5_Update(&ctx, prefix, prefix_len);
    }
    if ((ptr[0] & 0xf0) == 0x40) {
        MD5_Update(&ctx, ptr, 8);
        // Skip TTL (1 byte)
        MD5_Update(&ctx, ptr + 9, 1);
        // Skip Header checksum (2 bytes)
        MD5_Update(&ctx, ptr + 12, headerLen - 12);
    } else {
        MD5_Update(&ctx, ptr, 7);
        // Skip HOP
        MD5_Update(&ctx, ptr + 8, headerLen - 8);
    }
    MD5_Final(md, &ctx);
#else
    uint8_t buf[260];
    int     bufLen;
    if (prefix_len) {
        memcpy(buf, prefix, prefix_len);
    }
    if ((ptr[0] & 0xf0) == 0x40) {
        memcpy(buf + prefix_len, ptr, 8);
        buf[prefix_len + 8] = ptr[9];  // protocol, skip TTL (1 byte)
        memcpy(buf + prefix_len + 9, ptr + 12, headerLen - 12); // skip checksum (2 bytes)
        bufLen = prefix_len + 9 + (headerLen - 12);
    } else {
        memcpy(buf + prefix_len, ptr, 7);
        memcpy(buf + prefix_len + 7, ptr + 8, headerLen - 8); // skip HOP (1 byte)
        bufLen = prefix_len + 7 + (headerLen - 8);
    }
    arkime_siphash128(hashKey, buf, bufLen, md);
#endif

    const int h = ((uint32_t *)md)[0] & dedupSlotsMask;
    const uint8_t tag = md[3];

    // First see if we need to clean up old slot, and block all new folks while we do
    // In theory no one should be using this slot because we hadn't been searching it for a second.
    if (ARKIME_THREAD_ATOMIC_LOAD(seconds[secondSlot].tv_sec) != currentTime.tv_sec) {
        ARKIME_LOCK(seconds[secondSlot].lock);
        if (seconds[secondSlot].tv_sec != currentTime.tv_sec) { // Check critical section again inside the lock
            if (seconds[secondSlot].error) {
                uint32_t pcount = 0;
                for (uint32_t i = 0; i < dedupSlots; i++)
                    pcount += seconds[secondSlot].counts[i];
                LOG("WARNING - Ran out of room, increase dedupPackets to %u or above. pcount: %u", (dedupSlots + 1) * DEDUP_SLOT_FACTOR, pcount);
                seconds[secondSlot].error = 0;
            }
            // Invalidate before wiping so the lock free searchers below skip
            // this slot until the wipe is done and published
            ARKIME_THREAD_ATOMIC_STORE(seconds[secondSlot].tv_sec, 0);
            memset(seconds[secondSlot].counts, 0, dedupSlots);
            ARKIME_THREAD_ATOMIC_STORE(seconds[secondSlot].tv_sec, (uint32_t)currentTime.tv_sec);
        }
        ARKIME_UNLOCK(seconds[secondSlot].lock);
    }

    // Search all valid slots
    for (uint32_t s = 0; s < dedupSeconds; s++) {

        // If slot is too old, or mid wipe (tv_sec 0), just skip it
        if (currentTime.tv_sec - dedupSeconds + 1 >= ARKIME_THREAD_ATOMIC_LOAD(seconds[s].tv_sec)) {
            continue;
        }

        int count = ARKIME_THREAD_ATOMIC_LOAD(seconds[s].counts[h]);
        const uint8_t *ctrl_base = seconds[s].ctrl + h * DEDUP_SIZE_FACTOR;
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

    // Race condition: a searcher may see the incremented count before the ctrl/hash
    // stores below complete. Usually the unfinished entry just doesn't match, so a
    // duplicate can slip through briefly (fail open). The entry may also still hold
    // the previous occupant from dedupSeconds ago, so a packet identical to one from
    // back then can very rarely be dropped as a duplicate.
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
        seconds[i].counts = ARKIME_SIZE_ALLOC0("dedup counts", dedupSlots);
        seconds[i].ctrl = ARKIME_SIZE_ALLOC("dedup ctrl", dedupSize);
        seconds[i].hashes = ARKIME_SIZE_ALLOC("dedup hashes", dedupSize * 16);
    }

    // When sessionIdTracking uses VLAN or VNI, include that in the dedup hash
    // so packets on different VLANs/VNIs are not treated as duplicates.
    if (arkime_config_boolean(NULL, "dedupVlanVni", TRUE)) {
        if (sessionIdTracking == ARKIME_TRACKING_VLAN)
            dedupMode = DEDUP_VLAN;
        else if (sessionIdTracking == ARKIME_TRACKING_VNI)
            dedupMode = DEDUP_VNI;
    }

    if (config.debug)
        LOG("Dedup mode: %s", dedupMode == DEDUP_VLAN ? "VLAN" : dedupMode == DEDUP_VNI ? "VNI" : "PLAIN");
}
/******************************************************************************/
void arkime_dedup_perf_stats(GString *out, gboolean all)
{
    g_string_append(out, "\n== dedup ==\n");
    if (!config.enablePacketDedup) {
        g_string_append(out, "disabled\n");
        return;
    }
    if (!seconds) {
        g_string_append(out, "not initialized yet\n");
        return;
    }

    const uint64_t mem = (uint64_t)dedupSeconds * (dedupSlots + dedupSize + (uint64_t)dedupSize * 16);
    g_string_append_printf(out, "%-18s %s\n", "mode:", dedupMode == DEDUP_VLAN ? "vlan" : dedupMode == DEDUP_VNI ? "vni" : "plain");
    g_string_append_printf(out, "%-18s %u\n", "seconds:", dedupSeconds);
    g_string_append_printf(out, "%-18s %u\n", "packets:", dedupPackets);
    g_string_append_printf(out, "%-18s %u\n", "slots:", dedupSlots);
    g_string_append_printf(out, "%-18s %u (expected %u)\n", "entriesPerSlot:", DEDUP_SIZE_FACTOR, DEDUP_SLOT_FACTOR);
    g_string_append_printf(out, "%-18s %u\n", "size:", dedupSize);
    g_string_append_printf(out, "%-18s %" PRIu64 " bytes\n", "memory:", mem);
    g_string_append_printf(out, "%-18s %" PRIu64 "\n", "duplicatesDropped:", arkimeCounters.packetStats[ARKIME_PACKET_DUPLICATE_DROPPED]);

    // Scanning every second's counts can stall the main thread, so only with --all
    if (!all)
        return;

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_COARSE, &now);

    g_string_append_printf(out, "\n%-6s %-7s %5s %10s %10s %6s %10s %9s %8s\n",
                           "second", "state", "age", "entries", "usedSlots", "load%", "maxEntries", "fullSlots", "overflow");
    for (uint32_t s = 0; s < dedupSeconds; s++) {
        const uint32_t tv_sec = ARKIME_THREAD_ATOMIC_LOAD(seconds[s].tv_sec);
        uint32_t entries = 0, usedSlots = 0, fullSlots = 0, maxEntries = 0;
        for (uint32_t i = 0; i < dedupSlots; i++) {
            // counts keeps incrementing for inserts rejected by a full slot, so clamp to what is really stored
            const uint8_t raw = seconds[s].counts[i];
            if (!raw)
                continue;
            const uint8_t c = MIN(raw, DEDUP_SIZE_FACTOR);
            usedSlots++;
            entries += c;
            if (c > maxEntries)
                maxEntries = c;
            if (raw >= DEDUP_SIZE_FACTOR)
                fullSlots++;
        }

        const char *state;
        if (tv_sec == 0)
            state = "unused";
        else if ((uint32_t)now.tv_sec - dedupSeconds + 1 >= tv_sec)
            state = "expired";
        else
            state = "active";

        g_string_append_printf(out, "%-6u %-7s %5ld %10u %10u %5.1f%% %10u %9u %8s\n",
                               s, state, tv_sec ? (long)now.tv_sec - (long)tv_sec : -1L,
                               entries, usedSlots, dedupSlots ? usedSlots * 100.0 / dedupSlots : 0.0,
                               maxEntries, fullSlots, seconds[s].error ? "yes" : "no");
    }
}
/******************************************************************************/
void arkime_dedup_exit()
{
}
