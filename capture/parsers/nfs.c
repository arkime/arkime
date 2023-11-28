/* Copyright 2021 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/******************************************************************************/
LOCAL void nfs_classify_tcp(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "nfs"))
        return;

    arkime_session_add_protocol(session, "nfs");

}
/******************************************************************************/
LOCAL void nfs_classify_udp(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "nfs"))
        return;

    arkime_session_add_protocol(session, "nfs");

}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_parsers_classifier_register_udp("nfs", NULL, 4, (uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x86\xa3", 12, nfs_classify_udp);
    arkime_parsers_classifier_register_tcp("nfs", NULL, 8, (uint8_t *)"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x86\xa3", 12, nfs_classify_tcp);
}

