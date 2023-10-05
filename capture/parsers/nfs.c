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

