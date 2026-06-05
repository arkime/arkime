/******************************************************************************/
/* entropy.c  -- Calculate the Shannon entropy of the TCP/UDP data bytes
 *
 * The plugin keeps a 256 element histogram of the TCP and UDP data bytes seen
 * in a session, separately for each direction (src/dst).  Every
 * entropyChunkSize bytes (per direction) the Shannon entropy is calculated,
 * stored (multiplied by 100) as a unique integer in the field, and the
 * histogram reset.  Any remaining bytes are flushed on save.  Up to
 * entropyMaxUniqueValues unique values are recorded per direction.
 *
 * When entropyChunkSize fits in a uint16_t a uint16_t histogram is used to
 * halve the per-session memory, otherwise a uint32_t histogram is used.  The
 * width is chosen once at startup and the matching set of callbacks is
 * registered, so the per-byte hot path has no extra branching.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int      entropy_plugin_num;
LOCAL int      entropy_field[2];
LOCAL uint32_t entropyChunkSize;
LOCAL uint32_t entropyMaxUniqueValues;

#define ENTROPY_NUM_VALUES(session, which)                                                             \
    ((session)->fields[entropy_field[which]] ? (session)->fields[entropy_field[which]]->iarray->len : 0)

/******************************************************************************/
/* Generate a complete set of routines for a given count storage width.       */
#define ENTROPY_DEFINE(BITS, CTYPE)                                                                    \
typedef struct {                                                                                       \
    CTYPE    counts[2][256];                                                                           \
    uint32_t total[2];                                                                                 \
    uint8_t  stop[2];                                                                                  \
} EntropyData##BITS##_t;                                                                               \
                                                                                                       \
LOCAL void entropy_flush_##BITS(ArkimeSession_t *session, EntropyData##BITS##_t *ed, int which)        \
{                                                                                                      \
    if (ed->total[which] == 0)                                                                         \
        return;                                                                                        \
                                                                                                       \
    double entropy = 0.0;                                                                              \
    for (int i = 0; i < 256; i++) {                                                                    \
        if (ed->counts[which][i] == 0)                                                                 \
            continue;                                                                                  \
        double p = (double)ed->counts[which][i] / (double)ed->total[which];                            \
        entropy -= p * log2(p);                                                                        \
    }                                                                                                  \
                                                                                                       \
    if (arkime_field_int_add(entropy_field[which], session, (int)(entropy * 100.0 + 0.5))) {           \
        if (ENTROPY_NUM_VALUES(session, which) >= entropyMaxUniqueValues)                              \
            ed->stop[which] = 1;                                                                       \
    }                                                                                                  \
                                                                                                       \
    memset(ed->counts[which], 0, sizeof(ed->counts[which]));                                           \
    ed->total[which] = 0;                                                                              \
}                                                                                                      \
                                                                                                       \
LOCAL void entropy_add_bytes_##BITS(ArkimeSession_t *session, const uint8_t *data, int len, int which) \
{                                                                                                      \
    if (len <= 0)                                                                                      \
        return;                                                                                        \
                                                                                                       \
    EntropyData##BITS##_t *ed = session->pluginData[entropy_plugin_num];                               \
    if (!ed) {                                                                                         \
        ed = ARKIME_TYPE_ALLOC0(EntropyData##BITS##_t);                                                \
        session->pluginData[entropy_plugin_num] = ed;                                                  \
    }                                                                                                  \
                                                                                                       \
    if (ed->stop[which])                                                                               \
        return;                                                                                        \
                                                                                                       \
    for (int i = 0; i < len; i++) {                                                                    \
        ed->counts[which][data[i]]++;                                                                  \
        ed->total[which]++;                                                                            \
        if (ed->total[which] >= entropyChunkSize) {                                                    \
            entropy_flush_##BITS(session, ed, which);                                                  \
            if (ed->stop[which])                                                                       \
                return;                                                                                \
        }                                                                                              \
    }                                                                                                  \
}                                                                                                      \
                                                                                                       \
LOCAL void entropy_tcp_cb_##BITS(ArkimeSession_t *session, const uint8_t *data, int len, int which)    \
{                                                                                                      \
    entropy_add_bytes_##BITS(session, data, len, which);                                               \
}                                                                                                      \
                                                                                                       \
LOCAL void entropy_udp_cb_##BITS(ArkimeSession_t *session, const uint8_t *data, int len, int which)    \
{                                                                                                      \
    entropy_add_bytes_##BITS(session, data, len, which);                                               \
}                                                                                                      \
                                                                                                       \
LOCAL void entropy_save_##BITS(ArkimeSession_t *session, int final)                                    \
{                                                                                                      \
    EntropyData##BITS##_t *ed = session->pluginData[entropy_plugin_num];                               \
    if (!ed)                                                                                           \
        return;                                                                                        \
                                                                                                       \
    for (int which = 0; which < 2; which++) {                                                          \
        if (!ed->stop[which])                                                                          \
            entropy_flush_##BITS(session, ed, which);                                                  \
    }                                                                                                  \
                                                                                                       \
    if (final) {                                                                                       \
        ARKIME_TYPE_FREE(EntropyData##BITS##_t, ed);                                                   \
        session->pluginData[entropy_plugin_num] = 0;                                                   \
    } else {                                                                                           \
        /* mid save - record the values above and start fresh */                                       \
        memset(ed, 0, sizeof(*ed));                                                                    \
    }                                                                                                  \
}

ENTROPY_DEFINE(32, uint32_t)
ENTROPY_DEFINE(16, uint16_t)

/******************************************************************************/
void arkime_plugin_init()
{
    entropy_plugin_num = arkime_plugins_register("entropy", TRUE);

    entropyChunkSize = arkime_config_int(NULL, "entropyChunkSize", 0xffffffff, 1, 0xffffffff);
    entropyMaxUniqueValues = arkime_config_int(NULL, "entropyMaxUniqueValues", 10, 1, 0xffffffff);

    entropy_field[0] = arkime_field_define("general", "integer",
                                           "entropy.src", "Entropy Src", "entropy.src",
                                           "Shannon entropy of the source TCP/UDP data bytes, multiplied by 100",
                                           ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE, 0,
                                           (char *)NULL);

    entropy_field[1] = arkime_field_define("general", "integer",
                                           "entropy.dst", "Entropy Dst", "entropy.dst",
                                           "Shannon entropy of the destination TCP/UDP data bytes, multiplied by 100",
                                           ARKIME_FIELD_TYPE_INT_ARRAY_UNIQUE, 0,
                                           (char *)NULL);

    // A uint16_t histogram is enough when no single count (bounded by the
    // chunk size) can exceed 0xffff, which halves the per-session memory.
    if (entropyChunkSize <= 0xffff) {
        arkime_plugins_set_cb("entropy",
                              NULL,
                              entropy_udp_cb_16,
                              entropy_tcp_cb_16,
                              NULL,
                              entropy_save_16,
                              NULL,
                              NULL,
                              NULL);
    } else {
        arkime_plugins_set_cb("entropy",
                              NULL,
                              entropy_udp_cb_32,
                              entropy_tcp_cb_32,
                              NULL,
                              entropy_save_32,
                              NULL,
                              NULL,
                              NULL);
    }
}
