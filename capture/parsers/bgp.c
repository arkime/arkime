/* Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

//#define BGPDEBUG 1

extern ArkimeConfig_t        config;

LOCAL ArkimePQ_t            *bgpPq;
LOCAL  int                   typeField;

/******************************************************************************/
LOCAL int bgp_parser(ArkimeSession_t *session, void *UNUSED(uw), const uint8_t *data, int len, int UNUSED(which))
{
    if (len < 19 || memcmp("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", data, 16) != 0)
        return 0;

    static const char *types[] = {NULL, "OPEN", "UPDATE", "NOTIFICATION", "KEEPALIVE"};
    if (data[18] > 0 && data[18] < ARRAY_LEN(types)) {
        arkime_field_string_add(typeField, session, types[data[18]], -1, TRUE);
    }

    arkime_pq_upsert(bgpPq, session, NULL);
    return 0;
}
/******************************************************************************/
LOCAL void bgp_tcp_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (len < 19 || memcmp("\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", data, 16) != 0)
        return;

    arkime_session_add_protocol(session, "bgp");
    arkime_parsers_register(session, bgp_parser, NULL, NULL);
}
/******************************************************************************/
LOCAL void bgp_pq_cb(ArkimeSession_t *session, void UNUSED(*uw))
{
    session->midSave = 1;
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_port("bgp",  NULL, 179, ARKIME_PARSERS_PORT_TCP_DST, bgp_tcp_classify);
    bgpPq = arkime_pq_alloc(10, bgp_pq_cb);

    typeField = arkime_field_define("bgp", "uptermfield",
                                    "bgp.type", "Type", "bgp.type",
                                    "BGP Type field",
                                    ARKIME_FIELD_TYPE_STR_GHASH, 0,
                                    (char *)NULL);
}
