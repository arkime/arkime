/* Copyright 2021 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/******************************************************************************/
LOCAL void rpc_classify_udp(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 16)
        return;

    const char *name = 0;

    switch (data[15]) {
    case 0xa0:
        name = "portmap";
        break;
    case 0xa1:
        name = "rstat";
        break;
    case 0xa2:
        name = "rusers";
        break;
    case 0xa4:
        name = "ypprog";
        break;
    case 0xa5:
        name = "mount";
        break;
    case 0xa7:
        name = "ypbind";
        break;
    case 0xa8:
        name = "wall";
        break;
    case 0xa9:
        name = "yppasswd";
        break;
    case 0xab:
        name = "rquota";
        break;
    case 0xb1:
        name = "rexec";
        break;
    case 0xc5:
        name = "tfs";
        break;
    default:
        return;
    }

    if (arkime_session_has_protocol(session, name))
        return;
    arkime_session_add_protocol(session, name);
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_udp("rpc", NULL, 4, (uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x86", 11, rpc_classify_udp);
}

