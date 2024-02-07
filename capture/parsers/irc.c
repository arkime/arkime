/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

extern ArkimeConfig_t        config;

typedef struct {
    int ircState;
} IRCInfo_t;

LOCAL  int channelsField;
LOCAL  int nickField;

/******************************************************************************/
LOCAL int irc_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    IRCInfo_t *irc = uw;

    if (which == 1)
        return 0;

    BSB bsb;

    BSB_INIT(bsb, data, remaining);

    while (BSB_REMAINING(bsb)) {
        if (irc->ircState & 0x1) {
            int pos = 0;
            BSB_memchr(bsb, '\n', pos);

            if (pos) {
                irc->ircState &= ~ 0x1;
                BSB_IMPORT_skip(bsb, pos + 1);
                while (BSB_REMAINING(bsb) > 0 && BSB_PEEK(bsb) == 0) { // Some irc clients have 0's after new lines
                    BSB_IMPORT_skip(bsb, 1);
                }
            } else {
                return 0;
            }
        }

        if (BSB_REMAINING(bsb) > 6 && BSB_memcmp("JOIN ", bsb, 5) == 0) {
            BSB_IMPORT_skip(bsb, 5);
            uint8_t *start = BSB_WORK_PTR(bsb);
            while (BSB_REMAINING(bsb) > 0 && BSB_PEEK(bsb) != ' ' && BSB_PEEK(bsb) != '\r' && BSB_PEEK(bsb) != '\n') {
                BSB_IMPORT_skip(bsb, 1);
            }
            const uint8_t *end = BSB_WORK_PTR(bsb);

            if (!BSB_IS_ERROR(bsb) && start != end) {
                arkime_field_string_add(channelsField, session, (char *)start, end - start, TRUE);
            }
        }

        if (BSB_REMAINING(bsb) > 6 && BSB_memcmp("NICK ", bsb, 5) == 0) {
            BSB_IMPORT_skip(bsb, 5);
            uint8_t *start = BSB_WORK_PTR(bsb);
            while (BSB_REMAINING(bsb) > 0 && BSB_PEEK(bsb) != ' ' && BSB_PEEK(bsb) != '\r' && BSB_PEEK(bsb) != '\n') {
                BSB_IMPORT_skip(bsb, 1);
            }
            const uint8_t *end = BSB_WORK_PTR(bsb);

            if (!BSB_IS_ERROR(bsb) && start != end) {
                arkime_field_string_add(nickField, session, (char *)start, end - start, TRUE);
            }
        }

        if (BSB_REMAINING(bsb) > 0) {
            irc->ircState |=  0x1;
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void irc_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    IRCInfo_t            *irc          = uw;

    ARKIME_TYPE_FREE(IRCInfo_t, irc);
}
/******************************************************************************/
LOCAL void irc_classify(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (len < 8)
        return;

    if (data[0] == ':' && !arkime_memstr((char *)data, len, " NOTICE ", 8))
        return;

    //If a USER packet must have NICK or +iw with it so we don't pickup FTP
    if (data[0] == 'U' && !arkime_memstr((char *)data, len, "\nNICK ", 6) && !arkime_memstr((char *)data, len, " +iw ", 5)) {
        return;
    }

    if (arkime_session_has_protocol(session, "irc"))
        return;

    arkime_session_add_protocol(session, "irc");

    IRCInfo_t            *irc          = ARKIME_TYPE_ALLOC0(IRCInfo_t);

    arkime_parsers_register(session, irc_parser, irc, irc_free);
    irc_parser(session, irc, data, len, which);
}
/******************************************************************************/
void arkime_parser_init()
{
    nickField = arkime_field_define("irc", "termfield",
                                    "irc.nick", "Nickname", "irc.nick",
                                    "Nicknames set",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    "category", "user",
                                    (char *)NULL);

    channelsField = arkime_field_define("irc", "termfield",
                                        "irc.channel", "Channel", "irc.channel",
                                        "Channels joined",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    arkime_parsers_classifier_register_tcp("irc", NULL, 0, (uint8_t *)":", 1, irc_classify);
    arkime_parsers_classifier_register_tcp("irc", NULL, 0, (uint8_t *)"NOTICE AUTH", 11, irc_classify);
    arkime_parsers_classifier_register_tcp("irc", NULL, 0, (uint8_t *)"NICK ", 5, irc_classify);
    arkime_parsers_classifier_register_tcp("irc", NULL, 0, (uint8_t *)"USER ", 5, irc_classify);
    arkime_parsers_classifier_register_tcp("irc", NULL, 0, (uint8_t *)"CAP REQ ", 8, irc_classify);
}

