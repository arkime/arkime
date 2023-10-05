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
#include "arkime.h"

#define MAX_SSH_BUFFER 8196

typedef struct {
    char       buf[2][MAX_SSH_BUFFER];
    int32_t    len[2];
    uint16_t   packets[2];
    uint16_t   counts[2][2];
    uint16_t   done;
} SSHInfo_t;

extern ArkimeConfig_t        config;
LOCAL  int verField;
LOCAL  int keyField;
LOCAL  int hasshField;
LOCAL  int hasshServerField;

/******************************************************************************/
LOCAL void ssh_parse_keyinit(ArkimeSession_t *session, const uint8_t *data, int remaining, int isDst)
{
    BSB   bsb;
    char  hbuf[30000];
    BSB   hbsb;

    uint32_t       len = 0;
    uint8_t *value = 0;

    BSB_INIT(bsb, data, remaining);
    BSB_INIT(hbsb, hbuf, sizeof(hbuf));

    BSB_IMPORT_skip(bsb, 16);

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);
    BSB_EXPORT_ptr(hbsb, value, len);
    BSB_EXPORT_u08(hbsb, ';');


    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_skip(bsb, len);

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);

    if (BSB_IS_ERROR(bsb)) return;
    if (!isDst) {
        BSB_EXPORT_ptr(hbsb, value, len);
        BSB_EXPORT_u08(hbsb, ';');
    }

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);

    if (BSB_IS_ERROR(bsb)) return;
    if (isDst) {
        BSB_EXPORT_ptr(hbsb, value, len);
        BSB_EXPORT_u08(hbsb, ';');
    }

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);

    if (BSB_IS_ERROR(bsb)) return;
    if (!isDst) {
        BSB_EXPORT_ptr(hbsb, value, len);
        BSB_EXPORT_u08(hbsb, ';');
    }

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);

    if (BSB_IS_ERROR(bsb)) return;
    if (isDst) {
        BSB_EXPORT_ptr(hbsb, value, len);
        BSB_EXPORT_u08(hbsb, ';');
    }

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);

    if (BSB_IS_ERROR(bsb)) return;
    if (!isDst) {
        BSB_EXPORT_ptr(hbsb, value, len);
    }

    BSB_IMPORT_u32(bsb, len);
    BSB_IMPORT_ptr(bsb, value, len);

    if (BSB_IS_ERROR(bsb)) return;
    if (isDst) {
        BSB_EXPORT_ptr(hbsb, value, len);
    }

    if (!BSB_IS_ERROR(bsb) && !BSB_IS_ERROR(hbsb)) {
        gchar *md5 = g_compute_checksum_for_data(G_CHECKSUM_MD5, (guchar *)hbuf, BSB_LENGTH(hbsb));
        if (!arkime_field_string_add(isDst?hasshServerField:hasshField, session, md5, 32, FALSE)) {
            g_free(md5);
        }

    }
}

/******************************************************************************/
LOCAL int ssh_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    SSHInfo_t *ssh = uw;

    /* From packets 6-15 count number number of packets between 0-49 and 50-99 bytes.
     * If more of the bigger packets in both directions this probably is a reverse shell
     */
    ssh->packets[which]++;
    if (ssh->packets[which] > 5) {
        if (remaining < 50)
            ssh->counts[which][0]++;
        else if (remaining < 100)
            ssh->counts[which][1]++;

        if (ssh->packets[which] > 15) {
            if (ssh->counts[0][1] > ssh->counts[0][0] && ssh->counts[1][1] > ssh->counts[1][0]) {
                arkime_session_add_tag(session, "ssh-reverse-shell");
            }

            arkime_parsers_unregister(session, uw);
            return 0;
        }
    }

    // ssh->done is set when are finished decoding
    if (ssh->done)
        return 0;

    // Version handshake
    if (remaining > 3 && memcmp("SSH", data, 3) == 0) {
        uint8_t *n = memchr(data, 0x0a, remaining);
        if (n && *(n - 1) == 0x0d)
            n--;

        if (n) {
            int len = (n - data);

            arkime_field_string_add_lower(verField, session, (char *)data, len);
        }
        return 0;
    }

    // Actual messages
    memcpy(ssh->buf[which] + ssh->len[which], data, MIN(remaining, (int)sizeof(ssh->buf[which]) - ssh->len[which]));
    ssh->len[which] += MIN(remaining, (int)sizeof(ssh->buf[which]) - ssh->len[which]);

    while (ssh->len[which] > 6) {
        BSB bsb;
        BSB_INIT(bsb, ssh->buf[which], ssh->len[which]);

        uint32_t sshLen = 0;
        BSB_IMPORT_u32(bsb, sshLen);

        if (sshLen < 2 || sshLen > MAX_SSH_BUFFER) {
            ssh->done = 1;
            return 0;
        }

        if (sshLen > BSB_REMAINING(bsb))
            return 0;

        uint8_t    sshCode = 0;
        BSB_IMPORT_skip(bsb, 1); // padding length
        BSB_IMPORT_u08(bsb, sshCode);

        if (sshCode == 20) {
            ssh_parse_keyinit(session, BSB_WORK_PTR(bsb), BSB_REMAINING(bsb), which);
        } else if (sshCode == 33) {
            ssh->done = 1;

            uint32_t keyLen = 0;
            BSB_IMPORT_u32(bsb, keyLen);

            if (!BSB_IS_ERROR(bsb) && BSB_REMAINING(bsb) >= keyLen) {
                char *str = g_base64_encode(BSB_WORK_PTR(bsb), keyLen);
                if (!arkime_field_string_add(keyField, session, str, (keyLen / 3 + 1) * 4, FALSE)) {
                    g_free(str);
                }
            }
            break;
        }
        ssh->len[which] -= 4 + sshLen;
        memmove(ssh->buf[which], ssh->buf[which] + sshLen + 4, ssh->len[which]);
    }
    return 0;
}
/******************************************************************************/
LOCAL void ssh_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    SSHInfo_t            *ssh          = uw;

    ARKIME_TYPE_FREE(SSHInfo_t, ssh);
}
/******************************************************************************/
LOCAL void ssh_classify(ArkimeSession_t *session, const uint8_t *UNUSED(data), int UNUSED(len), int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "ssh"))
        return;

    arkime_session_add_protocol(session, "ssh");

    SSHInfo_t            *ssh          = ARKIME_TYPE_ALLOC0(SSHInfo_t);

    arkime_parsers_register(session, ssh_parser, ssh, ssh_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    verField = arkime_field_define("ssh", "lotermfield",
        "ssh.ver", "Version", "ssh.version",
        "SSH Software Version",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    keyField = arkime_field_define("ssh", "termfield",
        "ssh.key", "Key", "ssh.key",
        "SSH Key",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    hasshField = arkime_field_define("ssh", "lotermfield",
        "ssh.hassh", "HASSH", "ssh.hassh",
        "SSH HASSH field",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    hasshServerField = arkime_field_define("ssh", "lotermfield",
        "ssh.hasshServer", "HASSH Server", "ssh.hasshServer",
        "SSH HASSH Server field",
        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
        (char *)NULL);

    arkime_parsers_classifier_register_tcp("ssh", NULL, 0, (uint8_t *)"SSH", 3, ssh_classify);
}

