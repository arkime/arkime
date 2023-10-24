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
