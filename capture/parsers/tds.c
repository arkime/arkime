/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

#define TDS_MAX_SIZE 4096
typedef struct {
    uint8_t  data[2][TDS_MAX_SIZE];
    int      pos[2];
} TDSInfo_t;

extern ArkimeConfig_t        config;
LOCAL  int userField;

/******************************************************************************/
LOCAL int tds_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    TDSInfo_t *tds = uw;

    remaining = MIN(remaining, TDS_MAX_SIZE - tds->pos[which]);
    memcpy(tds->data[which] + tds->pos[which], data, remaining);
    tds->pos[which] += remaining;

    // Lots of info from http://www.freetds.org/tds.html

    if (tds->pos[0] > 598) {
#if 0
        LOG("host:%.*s user:%.*s pass:%.*s process:%.*s app:%.*s server:%.*s lib:%.*s",
            tds->data[0][38], tds->data[0] + 8,
            tds->data[0][69], tds->data[0] + 39,
            tds->data[0][100], tds->data[0] + 70,
            tds->data[0][131], tds->data[0] + 101,
            tds->data[0][178], tds->data[0] + 148,
            tds->data[0][209], tds->data[0] + 179,
            tds->data[0][480], tds->data[0] + 470
           );
#endif
        arkime_field_string_add_lower(userField, session, (const char *)tds->data[0] + 39, tds->data[0][69]);
        arkime_parsers_unregister(session, uw);
    }

    return 0;
}
/******************************************************************************/
LOCAL void tds_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    TDSInfo_t            *tds          = uw;

    ARKIME_TYPE_FREE(TDSInfo_t, tds);
}
/******************************************************************************/
LOCAL void tds_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int len, int which, void *UNUSED(uw))
{
    if (which != 0 || len < 512 || arkime_session_has_protocol(session, "tds"))
        return;

    arkime_session_add_protocol(session, "tds");

    TDSInfo_t            *tds          = ARKIME_TYPE_ALLOC(TDSInfo_t);
    tds->pos[0] = tds->pos[1] = 0;

    arkime_parsers_register(session, tds_parser, tds, tds_free);
}
/******************************************************************************/
void arkime_parser_init()
{

    userField = arkime_field_by_db("user");
    arkime_parsers_classifier_register_tcp("tds", NULL, 0, (uint8_t *)"\x02\x00\x02\x00\x00\x00\x01\x00", 8, tds_classify);
}

