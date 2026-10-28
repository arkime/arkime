/******************************************************************************/
/* siphash.h  -- Keyed SipHash-1-3
 *
 * https://github.com/veorq/SipHash with one round per 8 byte word and three
 * finalization rounds. Used for hashing data an attacker controls, like
 * session ids and packet headers. With a secret random key, traffic can't be
 * crafted to all land in the same hash bucket.
 *
 * Reads 8 byte words in host order, which matches the reference on little
 * endian. Include after arkime.h.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SIPHASH_HEADER
#define _SIPHASH_HEADER

#include <stdint.h>
#include <string.h>

#define ARKIME_SIP_ROTL(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define ARKIME_SIPROUND do { \
    v0 += v1; v1 = ARKIME_SIP_ROTL(v1, 13); v1 ^= v0; v0 = ARKIME_SIP_ROTL(v0, 32); \
    v2 += v3; v3 = ARKIME_SIP_ROTL(v3, 16); v3 ^= v2; \
    v0 += v3; v3 = ARKIME_SIP_ROTL(v3, 21); v3 ^= v0; \
    v2 += v1; v1 = ARKIME_SIP_ROTL(v1, 17); v1 ^= v2; v2 = ARKIME_SIP_ROTL(v2, 32); \
} while (0)

/******************************************************************************/
// Always inlined so the out128 checks compile away
SUPPRESS_UNSIGNED_INTEGER_OVERFLOW
static inline __attribute__((always_inline)) uint64_t arkime_siphash_internal(const uint64_t key[2], const uint8_t *in, int len, uint8_t *out128)
{
    uint64_t v0 = 0x736f6d6570736575ULL ^ key[0];
    uint64_t v1 = 0x646f72616e646f6dULL ^ key[1];
    uint64_t v2 = 0x6c7967656e657261ULL ^ key[0];
    uint64_t v3 = 0x7465646279746573ULL ^ key[1];
    if (out128)
        v1 ^= 0xee;

    const uint8_t *end = in + (len & ~7);
    for (; in < end; in += 8) {
        uint64_t m;
        memcpy(&m, in, 8);
        v3 ^= m;
        ARKIME_SIPROUND;
        v0 ^= m;
    }

    // Last 0-7 bytes, a loop beats a switch when lengths vary
    uint64_t b = ((uint64_t)len) << 56;
    for (int i = 0; i < (len & 7); i++)
        b |= ((uint64_t)in[i]) << (8 * i);
    v3 ^= b;
    ARKIME_SIPROUND;
    v0 ^= b;

    v2 ^= out128 ? 0xee : 0xff;
    ARKIME_SIPROUND;
    ARKIME_SIPROUND;
    ARKIME_SIPROUND;
    const uint64_t h0 = v0 ^ v1 ^ v2 ^ v3;
    if (!out128)
        return h0;

    v1 ^= 0xdd;
    ARKIME_SIPROUND;
    ARKIME_SIPROUND;
    ARKIME_SIPROUND;
    const uint64_t h1 = v0 ^ v1 ^ v2 ^ v3;
    memcpy(out128, &h0, 8);
    memcpy(out128 + 8, &h1, 8);
    return h0;
}
/******************************************************************************/
// 64 bit output
static inline uint64_t arkime_siphash(const uint64_t key[2], const void *in, int len)
{
    return arkime_siphash_internal(key, (const uint8_t *)in, len, NULL);
}
/******************************************************************************/
// 128 bit output
static inline void arkime_siphash128(const uint64_t key[2], const void *in, int len, uint8_t out[16])
{
    arkime_siphash_internal(key, (const uint8_t *)in, len, out);
}
#endif
