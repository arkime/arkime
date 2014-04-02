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
    if (session->addr1 == 0x0100000a) {
        char tmp[1000];
        moloch_field_int_add(test_ip, session, session->addr1);
        moloch_field_int_add(test_number, session, session->addr2);
        sprintf(tmp, "%d:%d,%d:%d", session->addr1, session->port1, session->addr2, session->port2);
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
        MOLOCH_FIELD_TYPE_IP_HASH,       0, 
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
