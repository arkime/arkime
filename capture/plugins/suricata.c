/* suricata.c  -- Simple plugin that monitors local suricata alert.json
 *
 * Copyright 2018 AOL Inc. All rights reserved.
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


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "moloch.h"
#include "bsb.h"

/******************************************************************************/

extern MolochConfig_t        config;

LOCAL GRegex     *slashslashRegex;

typedef struct suricataitem_t SuricataItem_t;
struct suricataitem_t {
    SuricataItem_t *items_next;
    char            sessionId[MOLOCH_SESSIONID_LEN];
    time_t          timestamp;
    char           *flow_id;
    char           *action;
    uint32_t        gid;
    uint32_t        signature_id;
    uint32_t        rev;
    char           *signature;
    char           *category;
    uint32_t        severity;
    uint32_t        hash;
    uint16_t        flow_id_len;
    uint16_t        action_len;
    uint16_t        signature_len;
    uint16_t        category_len;
    char            ses;
};

#define SURICATA_HASH_SIZE 7919

typedef struct suricatahead_t SuricataHead_t;
struct suricatahead_t {
    SuricataItem_t  *items[SURICATA_HASH_SIZE];
    MOLOCH_LOCK_EXTERN(lock);
    uint32_t         cnt;
    uint16_t         num;
};


LOCAL char                  *suricataAlertFile;
LOCAL FILE                  *file;
LOCAL int                    lineSize = 0xffff;
LOCAL char                  *line;
LOCAL int                    lineLen;
LOCAL ino_t                  fileInode;
LOCAL off_t                  fileSize;

LOCAL int                    flowIdField;
LOCAL int                    actionField;
LOCAL int                    signatureField;
LOCAL int                    categoryField;
LOCAL int                    gidField;
LOCAL int                    signatureIdField;
LOCAL int                    severityField;

LOCAL int                    suricataExpireSeconds;

SuricataHead_t alerts;

/******************************************************************************/
LOCAL void suricata_item_free(SuricataItem_t *item);
/******************************************************************************/
LOCAL void suricata_alerts_init()
{
    alerts.num = SURICATA_HASH_SIZE;
    MOLOCH_LOCK_INIT(alerts.lock);
}
/******************************************************************************/
LOCAL int suricata_alerts_add(SuricataItem_t *item)
{
    SuricataItem_t *check;

    item->hash = moloch_session_hash(item->sessionId);
    int h = item->hash % alerts.num;
    MOLOCH_LOCK(alerts.lock);

    // Dup is same hash, signature_id, timestamp, ses, and sessionId
    for (check = alerts.items[h]; check; check = check->items_next) {
        if (check->hash == item->hash &&
            check->timestamp == item->timestamp &&
            check->ses == item->ses &&
            check->signature_id == item->signature_id &&
            memcmp(check->sessionId, item->sessionId, item->sessionId[0]) == 0) {

            // Dup
            MOLOCH_UNLOCK(alerts.lock);
            return 0;
        }
    }
    item->items_next = alerts.items[h];
    alerts.items[h] = item;
    alerts.cnt++;
    MOLOCH_UNLOCK(alerts.lock);
    return 1;
}
/******************************************************************************/
LOCAL void suricata_alerts_del(SuricataItem_t *item)
{
    SuricataItem_t *check, *parent = NULL;

    int h = item->hash % alerts.num;

    MOLOCH_LOCK(alerts.lock);

    for (check = alerts.items[h]; check; parent = check, check = check->items_next) {
        if (check != item) {
            continue;
        }
        if (parent) {
            parent->items_next = check->items_next;
        } else {
            alerts.items[h] = check->items_next;
        }

        moloch_free_later(check, (GDestroyNotify)suricata_item_free);
        alerts.cnt--;
        break;
    }
    MOLOCH_UNLOCK(alerts.lock);
}
/******************************************************************************/
/*
 * Called by moloch when a session is about to be saved
 */
LOCAL void suricata_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    SuricataItem_t *item;
    int h = session->h_hash % alerts.num;

    for (item = alerts.items[h]; item; item = item->items_next) {
        if (item->timestamp < session->firstPacket.tv_sec - suricataExpireSeconds) {
            suricata_alerts_del(item);
            continue;
        }

        if (item->hash != session->h_hash ||
            item->ses != session->ses ||
            session->firstPacket.tv_sec - 30 > item->timestamp ||
            session->lastPacket.tv_sec + 30 < item->timestamp ||
            memcmp(session->sessionId, item->sessionId, item->sessionId[0]) != 0) {

            // This isn't the item we are looking for
            continue;
        }

        if (item->signature)
            moloch_field_string_add(signatureField, session, item->signature, item->signature_len, TRUE);
        if (item->category)
            moloch_field_string_add(categoryField, session, item->category, item->category_len, TRUE);
        if (item->flow_id)
            moloch_field_string_add(flowIdField, session, item->flow_id, item->flow_id_len, TRUE);
        if (item->action)
            moloch_field_string_add(actionField, session, item->action, item->action_len, TRUE);
        moloch_field_int_add(gidField, session, item->gid);
        moloch_field_int_add(signatureIdField, session, item->signature_id);
        moloch_field_int_add(severityField, session, item->severity);
    }
}

/******************************************************************************/
/*
 * Called by moloch when moloch is quiting
 */
LOCAL void suricata_plugin_exit()
{
}
/******************************************************************************/
LOCAL void suricata_item_free(SuricataItem_t *item)
{
    if (item->action)
        g_free(item->action);
    if (item->signature)
        g_free(item->signature);
    if (item->category)
        g_free(item->category);
    if (item->flow_id)
        g_free(item->flow_id);
    MOLOCH_TYPE_FREE(SuricataItem_t, item);
}
/******************************************************************************/
LOCAL gboolean suricata_parse_ip(char *str, int len, struct in6_addr *v)
{
    char ch = str[len];
    str[len] = 0;

    if (memchr(str, '.', 4)) {
        struct in_addr addr;
        if (inet_aton(str, &addr) == 0) {
            str[len] = ch;
            return 0;
        }

        memset(v->s6_addr, 0, 8);
        ((uint32_t *)v->s6_addr)[2] = htonl(0xffff);
        ((uint32_t *)v->s6_addr)[3] = addr.s_addr;
    } else {
        if (inet_pton(AF_INET6, str, v) == 0) {
            str[len] = ch;
            return 0;
        }
    }
    str[len] = ch;
    return 1;
}
/******************************************************************************/
#define MATCH(_d, _str) out[i+1] == sizeof(_str) - 1 && memcmp(_str, _d + out[i], sizeof(_str) - 1) == 0
/******************************************************************************/
LOCAL void suricata_process_alert(char *data, int len, SuricataItem_t *item)
{
    uint32_t out[4*100];
    memset(out, 0, sizeof(out));
    int rc;
    if ((rc = js0n((unsigned char *)data, len, out)) != 0) {
        LOG("ERROR: Parse error %d >%.*s<\n", rc, len, data);
        fflush(stdout);
        return;
    }

    int i;
    for (i = 0; out[i]; i+= 4) {
        if (config.debug > 2)
            LOG("  KEY %.*s DATA %.*s", out[i+1], data + out[i], out[i+3], data + out[i+2]);

        if (MATCH(data, "action")) {
            item->action = g_strndup(data + out[i+2], out[i+3]);
            item->action_len = out[i+3];
        } else if (MATCH(data, "gid")) {
            item->gid = atoi(data + out[i+2]);
        } else if (MATCH(data, "signature_id")) {
            item->signature_id = atoi(data + out[i+2]);
        } else if (MATCH(data, "rev")) {
            item->rev = atoi(data + out[i+2]);
        } else if (MATCH(data, "signature")) {
            item->signature = g_regex_replace_literal(slashslashRegex, data + out[i+2], out[i+3], 0, "/", 0, NULL);
            item->signature_len = strlen(item->signature);
        } else if (MATCH(data, "severity")) {
            item->severity = atoi(data + out[i+2]);
        } else if (MATCH(data, "category")) {
            item->category = g_strndup(data + out[i+2], out[i+3]);
            item->category_len = out[i+3];
        }
    }

    if (config.debug && !item->signature) {
        LOG("Missing signature >%.*s<", len, data);
    }
}
/******************************************************************************/
LOCAL void suricata_process()
{
    if (lineLen < 50)
        return;

    uint32_t out[4*100]; // Can have up to 100 elements at any level
    memset(out, 0, sizeof(out));
    int rc;
    if ((rc = js0n((unsigned char *)line, lineLen, out)) != 0) {
        LOG("ERROR: Parse error %d >%.*s<\n", rc, lineLen, line);
        fflush(stdout);
        return;
    }

    struct timespec currentTime;
    clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);

    SuricataItem_t *item = MOLOCH_TYPE_ALLOC0(SuricataItem_t);

    struct in6_addr srcIp;
    struct in6_addr dstIp;
    uint16_t        srcPort = 0;
    uint16_t        dstPort = 0;

    int i;
    for (i = 0; out[i]; i+= 4) {
        if (config.debug > 2)
            LOG("KEY %.*s DATA %.*s", out[i+1], line + out[i], out[i+3], line + out[i+2]);

        if (MATCH(line, "timestamp")) {
            struct tm tm;
            strptime(line + out[i+2], "%Y-%m-%dT%H:%M:%S.%%06u%z", &tm);
            item->timestamp = timegm(&tm);

            if (config.debug > 2) {
                char buf[100];
                ctime_r(&item->timestamp, buf);
                LOG("Parsed date  = %24.24s from %lu which %s >= %lu", buf, item->timestamp,
                        item->timestamp >= currentTime.tv_sec - suricataExpireSeconds?"is":"is not",
                        currentTime.tv_sec - suricataExpireSeconds);
            }

            if (item->timestamp < currentTime.tv_sec - suricataExpireSeconds) {
                suricata_item_free(item);
                return;
            }
        } else if (MATCH(line, "event_type")) {
            if (strncmp("alert", line + out[i+2], 5) != 0) {
                suricata_item_free(item);
                return;
            }
        } else if (MATCH(line, "src_ip")) {
            suricata_parse_ip(line + out[i+2], out[i+3], &srcIp);
        } else if (MATCH(line, "src_port")) {
            srcPort = atoi(line + out[i+2]);
        } else if (MATCH(line, "dest_ip")) {
            suricata_parse_ip(line + out[i+2], out[i+3], &dstIp);
        } else if (MATCH(line, "dest_port")) {
            dstPort = atoi(line + out[i+2]);
        } else if (MATCH(line, "flow_id")) {
            item->flow_id = g_strndup(line + out[i+2], out[i+3]);
            item->flow_id_len = out[i+3];
        } else if (MATCH(line, "proto")) {
            // Match on prototol by name or by
            // IANA number: https://www.iana.org/assignments/protocol-numbers/protocol-numbers.xhtml
            if (strncmp("TCP", line + out[i+2], 3) == 0 || strncmp("006", line + out[i+2], 3) == 0)
                item->ses = SESSION_TCP;
            else if (strncmp("UDP", line + out[i+2], 3) == 0 || strncmp("017", line + out[i+2], 3) == 0)
                item->ses = SESSION_UDP;
            else if (strncmp("ICMP", line + out[i+2], 4) == 0 || strncmp("001", line + out[i+2], 3) == 0)
                item->ses = SESSION_ICMP;
            else {
                suricata_item_free(item);
                return;
            }
        } else if (MATCH(line, "alert")) {
            suricata_process_alert(line + out[i+2], out[i+3], item);
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
    if (IN6_IS_ADDR_V4MAPPED(&srcIp)) {
        moloch_session_id(item->sessionId, MOLOCH_V6_TO_V4(srcIp), htons(srcPort), MOLOCH_V6_TO_V4(dstIp), htons(dstPort));
    } else {
        moloch_session_id6(item->sessionId, srcIp.s6_addr, htons(srcPort), dstIp.s6_addr, htons(dstPort));
    }
#pragma GCC diagnostic pop

    if (!suricata_alerts_add(item)) {
        suricata_item_free(item);
    }
}
/******************************************************************************/
LOCAL void suricata_read()
{
    while (fgets(line + lineLen, lineSize - lineLen, file)) {
        lineLen = strlen(line);
        if (line[lineLen-1] == '\n') {
            suricata_process();
            lineLen = 0;
        } else if (lineLen == lineSize - 1) {
            lineSize *= 1.5;
            line = realloc(line, lineSize);
            if (!line)
                LOGEXIT("ERROR - OOM %d", lineSize);
        }
    }
    clearerr(file);

}
/******************************************************************************/
LOCAL void suricata_open(struct stat *sb)
{
    static int printedError;

    file = fopen(suricataAlertFile, "r");
    if (!file) {
        if (!printedError) {
            LOG("ERROR - Permissions problem, can't open suricataAlertFile '%s'", suricataAlertFile);
            printedError = 1;
        }
        return;
    }

    // Change to non blocking
    int fd = fileno(file);
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);

    printedError = 0;
    fileInode = sb->st_ino;
    suricata_read();
    lineLen = 0;
}
/******************************************************************************/
LOCAL void suricata_close()
{
    fclose(file);
    file = NULL;
    fileInode = 0;
    lineLen = 0;
}
/******************************************************************************/
LOCAL gboolean suricata_timer(gpointer UNUSED(user_data))
{
    static int printedError;
    struct stat sb;

    int rc = stat(suricataAlertFile, &sb);

    if (rc < 0) {
        if (!printedError) {
            LOG("ERROR - Can't access suricataAlertFile '%s' : %s\n", suricataAlertFile, strerror(errno));
            printedError = 1;
        }
        if (file) {
            // File disappeared
            suricata_read();
            suricata_close();
        }
        return TRUE;
    }

    if (file && fileInode != sb.st_ino) {
        // File has a new inode, old was moved/deleted
        suricata_read();
        suricata_close();
    }

    if (!file) {
        // New file
        suricata_open(&sb);
        fileSize = sb.st_size;
    } else if (fileSize < sb.st_size) {
        // File got bigger
        suricata_read();
        fileSize = sb.st_size;
    } else if (fileSize > sb.st_size) {
        // File got smaller
        suricata_read();
        suricata_close();
        suricata_open(&sb);
        fileSize = sb.st_size;
    }

    printedError = 0;
    return TRUE;
}
/******************************************************************************/
/*
 * Called by moloch when the plugin is loaded
 */
void moloch_plugin_init()
{
    line = malloc(lineSize);

    suricataAlertFile     = moloch_config_str(NULL, "suricataAlertFile", NULL);
    suricataExpireSeconds = moloch_config_int(NULL, "suricataExpireMinutes", 60, 10, 0xffffff) * 60;

    suricata_alerts_init();

    if (!suricataAlertFile)
        LOGEXIT("No suricataAlertFile set");

    moloch_plugins_register("suricata", FALSE);

    moloch_plugins_set_cb("suricata",
      NULL,
      NULL,
      NULL,
      NULL,
      suricata_plugin_save,
      NULL,
      suricata_plugin_exit,
      NULL
    );

    flowIdField = moloch_field_define("suricata", "termfield",
        "suricata.flowId", "Flow Id", "suricata.flowId",
        "Suricata Flow Id",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    actionField = moloch_field_define("suricata", "termfield",
        "suricata.action", "Action", "suricata.action",
        "Suricata Action",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    signatureField = moloch_field_define("suricata", "termfield",
        "suricata.signature", "Signature", "suricata.signature",
        "Suricata Signature",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    categoryField = moloch_field_define("suricata", "termfield",
        "suricata.category", "Category", "suricata.category",
        "Suricata Category",
        MOLOCH_FIELD_TYPE_STR_HASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    gidField = moloch_field_define("suricata", "integer",
        "suricata.gid", "Gid", "suricata.gid",
        "Suricata Gid",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    signatureIdField = moloch_field_define("suricata", "integer",
        "suricata.signatureId", "Signature Id", "suricata.signatureId",
        "Suricata Signature Id",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    severityField = moloch_field_define("suricata", "integer",
        "suricata.severity", "Severity", "suricata.severity",
        "Suricata Severity",
        MOLOCH_FIELD_TYPE_INT_GHASH,  MOLOCH_FIELD_FLAG_CNT,
        (char *)NULL);

    slashslashRegex = g_regex_new("\\\\/", 0, 0, 0);

    g_timeout_add_seconds(1, suricata_timer, 0);
    suricata_timer(NULL);
}
