/* Copyright 2019 AOL Inc. All rights reserved.
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
#include "moloch.h"

//#define BGPDEBUG 1

extern MolochConfig_t        config;

LOCAL MolochPQ_t            *bgpPq;
LOCAL  int                   typeField;

/******************************************************************************/
LOCAL int bgp_parser(MolochSession_t *session, void *UNUSED(uw), const unsigned char *data, int len, int UNUSED(which))
{
    if (len < 19 || memcmp("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", data, 16) != 0)
        return 0;

    if (data[18] > 0 && data[18] < 5) {
        static const char *types[5] = {NULL, "OPEN", "UPDATE", "NOTIFICATION", "KEEPALIVE"};

        moloch_field_string_add(typeField, session, types[data[18]], -1, TRUE);
    }

    moloch_pq_upsert(bgpPq, session, 5, NULL);
    return 0;
}
/******************************************************************************/
LOCAL void bgp_tcp_classify(MolochSession_t *session, const unsigned char *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 19 || memcmp("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", data, 16) != 0)
        return;

    moloch_session_add_protocol(session, "bgp");
    moloch_parsers_register(session, bgp_parser, NULL, NULL);
}
/******************************************************************************/
LOCAL void bgp_pq_cb(MolochSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_port("bgp",  NULL, 179, MOLOCH_PARSERS_PORT_TCP_DST, bgp_tcp_classify);
    bgpPq = moloch_pq_alloc(10, bgp_pq_cb);

    typeField = moloch_field_define("bgp","uptermfield",
        "bgp.type", "Type", "bgp.type",
        "BGP Type field",
        MOLOCH_FIELD_TYPE_STR_GHASH, 0,
        (char *)NULL);
}
