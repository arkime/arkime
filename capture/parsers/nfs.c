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
#include "moloch.h"

/******************************************************************************/
LOCAL void nfs_classify_tcp(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "nfs"))
        return;

    moloch_session_add_protocol(session, "nfs");

}
/******************************************************************************/
LOCAL void nfs_classify_udp(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (moloch_session_has_protocol(session, "nfs"))
        return;

    moloch_session_add_protocol(session, "nfs");

}
/******************************************************************************/
void moloch_parser_init()
{
    moloch_parsers_classifier_register_udp("nfs", NULL, 4, (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x86\xa3", 12, nfs_classify_udp);
    moloch_parsers_classifier_register_tcp("nfs", NULL, 8, (unsigned char *)"\x00\x00\x00\x00\x00\x00\x00\x02\x00\x01\x86\xa3", 12, nfs_classify_tcp);
}

