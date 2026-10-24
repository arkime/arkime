/******************************************************************************/
/* test.c
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../capture/arkime.h"

extern ArkimeConfig_t        config;

static int test_number;
static int test_ip;
static int test_string;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
/******************************************************************************/
void test_plugin_pre_save(ArkimeSession_t *session, int UNUSED(final))
{
    if (ARKIME_V6_TO_V4(session->addr1) == 0x0100000a) {
        char tmp[1000];
        arkime_field_ip4_add(test_ip, session, ((uint32_t *)session->addr1.s6_addr)[3]);
        arkime_field_int_add(test_number, session, ((uint32_t *)session->addr2.s6_addr)[3]);
        snprintf(tmp, sizeof(tmp), "%d:%d,%d:%d", ((uint32_t *)session->addr1.s6_addr)[3], session->port1, ((uint32_t *)session->addr2.s6_addr)[3], session->port2);
        arkime_field_string_add(test_string, session, tmp, -1, TRUE);
    }
}
#pragma GCC diagnostic pop

/******************************************************************************/
/* Rules v2 functions for tests/rulesv2.t */
LOCAL gboolean test_rules_double(ArkimeSession_t *UNUSED(session), const ArkimeRulesV2Value_t *args, int UNUSED(nargs), ArkimeRulesV2Value_t *result, gpointer UNUSED(uw))
{
    result->i = args[0].i * 2;
    return TRUE;
}
LOCAL gboolean test_rules_half(ArkimeSession_t *UNUSED(session), const ArkimeRulesV2Value_t *args, int UNUSED(nargs), ArkimeRulesV2Value_t *result, gpointer UNUSED(uw))
{
    result->f = args[0].f / 2;
    return TRUE;
}
LOCAL gboolean test_rules_rev(ArkimeSession_t *UNUSED(session), const ArkimeRulesV2Value_t *args, int UNUSED(nargs), ArkimeRulesV2Value_t *result, gpointer UNUSED(uw))
{
    char *rev = g_strreverse(g_strdup(args[0].s));
    arkime_free_later(rev, g_free);
    result->s = rev;
    return TRUE;
}
LOCAL gboolean test_rules_port_sum(ArkimeSession_t *session, const ArkimeRulesV2Value_t *UNUSED(args), int UNUSED(nargs), ArkimeRulesV2Value_t *result, gpointer UNUSED(uw))
{
    result->i = session->port1 + session->port2;
    return TRUE;
}
LOCAL gboolean test_rules_has_char(ArkimeSession_t *UNUSED(session), const ArkimeRulesV2Value_t *args, int UNUSED(nargs), ArkimeRulesV2Value_t *result, gpointer uw)
{
    result->i = strchr(args[0].s, GPOINTER_TO_INT(uw)) != NULL;
    return TRUE;
}
LOCAL gboolean test_rules_nothing(ArkimeSession_t *UNUSED(session), const ArkimeRulesV2Value_t *UNUSED(args), int UNUSED(nargs), ArkimeRulesV2Value_t *UNUSED(result), gpointer UNUSED(uw))
{
    return FALSE;
}
LOCAL gboolean test_rules_same_ip(ArkimeSession_t *UNUSED(session), const ArkimeRulesV2Value_t *args, int UNUSED(nargs), ArkimeRulesV2Value_t *result, gpointer UNUSED(uw))
{
    result->i = memcmp(args[0].ip, args[1].ip, sizeof(struct in6_addr)) == 0;
    return TRUE;
}

/******************************************************************************/
void arkime_plugin_init()
{
    test_number = arkime_field_define("test", "integer",
                                      "test.number", "Test Number", "test.number",
                                      "Test Number",
                                      ARKIME_FIELD_TYPE_INT_HASH,      0,
                                      NULL);

    test_ip = arkime_field_define("test", "ip",
                                  "test.ip", "Test Ip", "test.ip",
                                  "Test IP",
                                  ARKIME_FIELD_TYPE_IP_GHASH,       0,
                                  NULL);

    test_string = arkime_field_define("test", "textfield",
                                      "test.string", "Test String", "test.string.snow",
                                      "Test String",
                                      ARKIME_FIELD_TYPE_STR_HASH,       0,
                                      NULL);

    arkime_rulesv2_register_function("testDouble", "i:i", ARKIME_RULESV2_FUNC_PURE, test_rules_double, NULL);
    arkime_rulesv2_register_function("testHalf", "f:f", ARKIME_RULESV2_FUNC_PURE, test_rules_half, NULL);
    arkime_rulesv2_register_function("testRev", "s:s", ARKIME_RULESV2_FUNC_PURE, test_rules_rev, NULL);
    arkime_rulesv2_register_function("testPortSum", "i:", 0, test_rules_port_sum, NULL);
    arkime_rulesv2_register_function("testHasDot", "b:s", ARKIME_RULESV2_FUNC_PURE, test_rules_has_char, GINT_TO_POINTER('.'));
    arkime_rulesv2_register_function("testNothing", "i:i", 0, test_rules_nothing, NULL);
    arkime_rulesv2_register_function("testSameIp", "b:aa", 0, test_rules_same_ip, NULL);

    arkime_plugins_register("string", FALSE);
    arkime_plugins_set_cb("string",
                          NULL,
                          NULL,
                          NULL,
                          test_plugin_pre_save,
                          NULL,
                          NULL,
                          NULL,
                          NULL
                         );
}
