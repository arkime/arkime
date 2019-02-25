/* Copyright 2012-2017 AOL Inc. All rights reserved.
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

extern MolochConfig_t        config;

typedef struct {
    int ircState;
} IRCInfo_t;

LOCAL  int channelsField;
LOCAL  int nickField;

/******************************************************************************/
LOCAL int irc_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    IRCInfo_t *irc = uw;

    if (which == 1)
        return 0;

    BSB bsb;

    BSB_INIT(bsb, data, remaining);

    while (BSB_REMAINING(bsb)) {
        if (irc->ircState & 0x1) {
            int pos;
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
            unsigned char *start = BSB_WORK_PTR(bsb);
            while (BSB_REMAINING(bsb) > 0 && BSB_PEEK(bsb) != ' ' && BSB_PEEK(bsb) != '\r' && BSB_PEEK(bsb) != '\n') {
                BSB_IMPORT_skip(bsb, 1);
            }
            unsigned char *end = BSB_WORK_PTR(bsb);

            if (!BSB_IS_ERROR(bsb) && start != end) {
                moloch_field_string_add(channelsField, session, (char*)start, end - start, TRUE);
            }
        }

        if (BSB_REMAINING(bsb) > 6 && BSB_memcmp("NICK ", bsb, 5) == 0) {
            BSB_IMPORT_skip(bsb, 5);
            unsigned char *start = BSB_WORK_PTR(bsb);
            while (BSB_REMAINING(bsb) > 0 && BSB_PEEK(bsb) != ' ' && BSB_PEEK(bsb) != '\r' && BSB_PEEK(bsb) != '\n') {
                BSB_IMPORT_skip(bsb, 1);
            }
            unsigned char *end = BSB_WORK_PTR(bsb);

            if (!BSB_IS_ERROR(bsb) && start != end) {
                moloch_field_string_add(nickField, session, (char*)start, end - start, TRUE);
            }
        }

        if (BSB_REMAINING(bsb) > 0) {
            irc->ircState |=  0x1;
        }
    }

    return 0;
}
/******************************************************************************/
LOCAL void irc_free(MolochSession_t UNUSED(*session), void *uw)
{
    IRCInfo_t            *irc          = uw;

    MOLOCH_TYPE_FREE(IRCInfo_t, irc);
}
/******************************************************************************/
LOCAL void irc_classify(MolochSession_t *session, const unsigned char *data, int len, int which, void *UNUSED(uw))
{
    if (len < 8)
        return;

    if (data[0] == ':' && !moloch_memstr((char *)data, len, " NOTICE ", 8))
        return;

    //If a USER packet must have NICK or +iw with it so we don't pickup FTP
    if (data[0] == 'U' && !moloch_memstr((char *)data, len, "\nNICK ", 6) && !moloch_memstr((char *)data, len, " +iw ", 5)) {
        return;
    }

    if (moloch_session_has_protocol(session, "irc"))
        return;

    moloch_session_add_protocol(session, "irc");

    IRCInfo_t            *irc          = MOLOCH_TYPE_ALLOC0(IRCInfo_t);

    moloch_parsers_register(session, irc_parser, irc, irc_free);
    irc_parser(session, irc, data, len, which);
}
/******************************************************************************/
void moloch_parser_init()
{
    nickField = moloch_field_define("irc", "termfield",
        "irc.nick", "Nickname", "irc.nick",
        "Nicknames set",
        MOLOCH_FIELD_TYPE_STR_GHASH, MOLOCH_FIELD_FLAG_CNT,
        "category", "user",
        (char *)NULL);

    channelsField = moloch_field_define("irc", "termfield",
        "irc.channel", "Channel", "irc.channel",
        "Channels joined",
        MOLOCH_FIELD_TYPE_STR_GHASH, MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    moloch_parsers_classifier_register_tcp("irc", NULL, 0, (unsigned char*)":", 1, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", NULL, 0, (unsigned char*)"NOTICE AUTH", 11, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", NULL, 0, (unsigned char*)"NICK ", 5, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", NULL, 0, (unsigned char*)"USER ", 5, irc_classify);
    moloch_parsers_classifier_register_tcp("irc", NULL, 0, (unsigned char*)"CAP REQ ", 8, irc_classify);
}

