/* Copyright 2012-2014 AOL Inc. All rights reserved.
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
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "moloch.h"
 
typedef struct {
    uint16_t   sshLen;
    uint8_t    sshCode;
} SSHInfo_t;

static int verField;
static int keyField;

/******************************************************************************/
int ssh_parser(MolochSession_t *session, void *uw, const unsigned char *data, int remaining, int which)
{
    SSHInfo_t *ssh = uw;

    if (memcmp("SSH", data, 3) == 0) {
        unsigned char *n = memchr(data, 0x0a, remaining);
        if (n && *(n-1) == 0x0d)
            n--;

        if (n) {
            int len = (n - data);

            char *str = g_ascii_strdown((char *)data, len);

            if (!moloch_field_string_add(verField, session, str, len, FALSE)) {
                free(str);
            }
        }
        return 0;
    }

    if (which != 1)
        return 0;

    while (remaining >= 6) {
        if (ssh->sshLen == 0) {
            ssh->sshLen = (data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3]) + 4;
            ssh->sshCode = data[5];
            if (ssh->sshLen == 0) {
                break;
            }
        }

        if (ssh->sshCode == 33 && remaining > 8) {
            uint32_t keyLen = data[6] << 24 | data[7] << 16 | data[8] << 8 | data[9];
            moloch_parsers_unregister(session, uw);
            if ((uint32_t)remaining > keyLen + 8) {
                char *str = g_base64_encode(data+10, keyLen);

                if (!moloch_field_string_add(keyField, session, str, (keyLen/3+1)*4, FALSE)) {
                    g_free(str);
                }
            }
            break;
        }

        if (remaining > ssh->sshLen) {
            remaining -= ssh->sshLen;
            ssh->sshLen = 0;
            continue;
        } else {
            ssh->sshLen -= remaining;
            remaining = 0;
            continue;
        }
    }
    return 0;
}
/******************************************************************************/
void ssh_free(MolochSession_t UNUSED(*session), void *uw)
{
    SSHInfo_t            *ssh          = uw;

    MOLOCH_TYPE_FREE(SSHInfo_t, ssh);
}
/******************************************************************************/
void ssh_classify(MolochSession_t *session, const unsigned char *UNUSED(data), int UNUSED(len), int which)
{
    if (moloch_nids_has_protocol(session, "ssh"))
        return;

    moloch_nids_add_protocol(session, "ssh");

    SSHInfo_t            *ssh          = MOLOCH_TYPE_ALLOC0(SSHInfo_t);

    moloch_parsers_register(session, ssh_parser, ssh, ssh_free);
    ssh_parser(session, ssh, data, len, which);
}
/******************************************************************************/
void moloch_parser_init()
{
    verField = moloch_field_define("ssh", "lotermfield",
        "ssh.ver", "Version", "sshver",
        "SSH Software Version",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        NULL);

    keyField = moloch_field_define("ssh", "termfield",
        "ssh.key", "Key", "sshkey", 
        "SSH Key",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT, 
        NULL);

    moloch_parsers_classifier_register_tcp("ssh", 0, (unsigned char*)"SSH", 3, ssh_classify);
}

