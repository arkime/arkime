#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../capture/moloch.h"

extern MolochConfig_t        config;

static int test_number;
static int test_ip;
static int test_string;
/******************************************************************************/
void test_plugin_pre_save(MolochSession_t *session, int UNUSED(final))
{
    if (MOLOCH_V6_TO_V4(session->addr1) == 0x0100000a) {
        char tmp[1000];
        moloch_field_ip4_add(test_ip, session, ((uint32_t *)session->addr1.s6_addr)[3]);
        moloch_field_int_add(test_number, session, ((uint32_t *)session->addr2.s6_addr)[3]);
        sprintf(tmp, "%d:%d,%d:%d", ((uint32_t *)session->addr1.s6_addr)[3], session->port1, ((uint32_t *)session->addr2.s6_addr)[3], session->port2);
        moloch_field_string_add(test_string, session, tmp, -1, TRUE);
    }
}

/******************************************************************************/
void moloch_plugin_init()
{
    test_number = moloch_field_define("test", "integer",
        "test.number", "Test Number", "test.number",
        "Test Number",
        MOLOCH_FIELD_TYPE_INT_HASH,      0, 
        NULL);

    test_ip = moloch_field_define("test", "ip",
        "test.ip", "Test Ip", "test.ip",
        "Test IP",
        MOLOCH_FIELD_TYPE_IP_GHASH,       0, 
        NULL);

    test_string = moloch_field_define("test", "textfield",
        "test.string", "Test String", "test.string.snow",
        "Test String",
        MOLOCH_FIELD_TYPE_STR_HASH,       0, 
        NULL);

    moloch_plugins_register("string", FALSE);
    moloch_plugins_set_cb("string",
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
