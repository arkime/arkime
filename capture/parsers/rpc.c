/* Copyright 2021 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "arkime.h"

/******************************************************************************/
LOCAL void rpc_classify_udp(ArkimeSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 16)
        return;

    char *name = 0;

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
    arkime_parsers_classifier_register_udp("rpc", NULL, 4, (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x86", 11, rpc_classify_udp);
}

