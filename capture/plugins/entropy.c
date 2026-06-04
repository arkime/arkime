/******************************************************************************/
/* entropy.c  -- Calculate the Shannon entropy of the TCP/UDP data bytes
 *
 * The plugin keeps a 256 element histogram (one uint32_t counter per byte
 * value) of all the TCP and UDP data bytes seen in a session.  When the
 * session is saved the Shannon entropy is calculated from the histogram and
 * stored, multiplied by 100, as an integer field.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int entropy_plugin_num;
LOCAL int entropy_field;

typedef struct {
    uint32_t counts[256];
    uint32_t total;
} EntropyData_t;

/******************************************************************************/
LOCAL EntropyData_t *entropy_get_data(ArkimeSession_t *session)
{
    EntropyData_t *ed = session->pluginData[entropy_plugin_num];
    if (!ed) {
        ed = ARKIME_TYPE_ALLOC0(EntropyData_t);
        session->pluginData[entropy_plugin_num] = ed;
    }
    return ed;
}
/******************************************************************************/
LOCAL void entropy_add_bytes(ArkimeSession_t *session, const uint8_t *data, int len)
{
    if (len <= 0)
        return;

    EntropyData_t *ed = entropy_get_data(session);
    for (int i = 0; i < len; i++) {
        ed->counts[data[i]]++;
    }
    ed->total += len;
}
/******************************************************************************/
LOCAL void entropy_tcp_cb(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which))
{
    entropy_add_bytes(session, data, len);
}
/******************************************************************************/
LOCAL void entropy_udp_cb(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which))
{
    entropy_add_bytes(session, data, len);
}
/******************************************************************************/
LOCAL void entropy_save(ArkimeSession_t *session, int final)
{
    EntropyData_t *ed = session->pluginData[entropy_plugin_num];
    if (!ed)
        return;

    if (ed->total > 0) {
        double entropy = 0.0;
        for (int i = 0; i < 256; i++) {
            if (ed->counts[i] == 0)
                continue;
            double p = (double)ed->counts[i] / (double)ed->total;
            entropy -= p * log2(p);
        }
        arkime_field_int_add(entropy_field, session, (int)(entropy * 100.0 + 0.5));
    }

    if (final) {
        ARKIME_TYPE_FREE(EntropyData_t, ed);
        session->pluginData[entropy_plugin_num] = 0;
    } else {
        // mid save - record the value above and start a fresh histogram
        memset(ed->counts, 0, sizeof(ed->counts));
        ed->total = 0;
    }
}
/******************************************************************************/
void arkime_plugin_init()
{
    entropy_plugin_num = arkime_plugins_register("entropy", TRUE);

    entropy_field = arkime_field_define("general", "integer",
                                        "entropy", "Entropy", "entropy",
                                        "Shannon entropy of the TCP/UDP data bytes, multiplied by 100",
                                        ARKIME_FIELD_TYPE_INT, 0,
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
