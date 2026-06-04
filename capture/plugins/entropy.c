/******************************************************************************/
/* entropy.c  -- Calculate the Shannon entropy of the TCP/UDP data bytes
 *
 * The plugin keeps a 256 element histogram (one uint32_t counter per byte
 * value) of the TCP and UDP data bytes seen in a session, separately for each
 * direction (src/dst).  Every entropyChunkSize bytes (per direction) the
 * Shannon entropy is calculated, stored (multiplied by 100) as a unique
 * integer in the field, and the histogram reset.  Any remaining bytes are
 * flushed on save.  Up to entropyMaxUniqueValues unique values are recorded
 * per direction.
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

typedef struct {
    uint32_t counts[2][256];
    uint32_t total[2];
    uint8_t  stop[2];
} EntropyData_t;

#define ENTROPY_NUM_VALUES(session, which) \
    ((session)->fields[entropy_field[which]] ? (session)->fields[entropy_field[which]]->iarray->len : 0)

/******************************************************************************/
LOCAL void entropy_flush(ArkimeSession_t *session, EntropyData_t *ed, int which)
{
    if (ed->total[which] == 0)
        return;

    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (ed->counts[which][i] == 0)
            continue;
        double p = (double)ed->counts[which][i] / (double)ed->total[which];
        entropy -= p * log2(p);
    }

    if (arkime_field_int_add(entropy_field[which], session, (int)(entropy * 100.0 + 0.5))) {
        if (ENTROPY_NUM_VALUES(session, which) >= entropyMaxUniqueValues)
            ed->stop[which] = 1;
    }

    memset(ed->counts[which], 0, sizeof(ed->counts[which]));
    ed->total[which] = 0;
}
/******************************************************************************/
LOCAL void entropy_add_bytes(ArkimeSession_t *session, const uint8_t *data, int len, int which)
{
    if (len <= 0)
        return;

    EntropyData_t *ed = session->pluginData[entropy_plugin_num];
    if (!ed) {
        ed = ARKIME_TYPE_ALLOC0(EntropyData_t);
        session->pluginData[entropy_plugin_num] = ed;
    }

    if (ed->stop[which])
        return;

    for (int i = 0; i < len; i++) {
        ed->counts[which][data[i]]++;
        ed->total[which]++;
        if (ed->total[which] >= entropyChunkSize) {
            entropy_flush(session, ed, which);
            if (ed->stop[which])
                return;
        }
    }
}
/******************************************************************************/
LOCAL void entropy_tcp_cb(ArkimeSession_t *session, const uint8_t *data, int len, int which)
{
    entropy_add_bytes(session, data, len, which);
}
/******************************************************************************/
LOCAL void entropy_udp_cb(ArkimeSession_t *session, const uint8_t *data, int len, int which)
{
    entropy_add_bytes(session, data, len, which);
}
/******************************************************************************/
LOCAL void entropy_save(ArkimeSession_t *session, int final)
{
    EntropyData_t *ed = session->pluginData[entropy_plugin_num];
    if (!ed)
        return;

    for (int which = 0; which < 2; which++) {
        if (!ed->stop[which])
            entropy_flush(session, ed, which);
    }

    if (final) {
        ARKIME_TYPE_FREE(EntropyData_t, ed);
        session->pluginData[entropy_plugin_num] = 0;
    } else {
        // mid save - record the values above and start fresh
        memset(ed, 0, sizeof(*ed));
    }
}
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

    arkime_plugins_set_cb("entropy",
                          NULL,
                          entropy_udp_cb,
                          entropy_tcp_cb,
                          NULL,
                          entropy_save,
                          NULL,
                          NULL,
                          NULL);
}
