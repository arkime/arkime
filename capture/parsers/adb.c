/* Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "arkime.h"

/******************************************************************************/
/* ADB command constants (little-endian) */
#define A_SYNC  0x434e5953
#define A_CNXN  0x4e584e43
#define A_OPEN  0x4e45504f
#define A_OKAY  0x59414b4f
#define A_CLSE  0x45534c43
#define A_WRTE  0x45545257
#define A_AUTH  0x48545541
#define A_STLS  0x534c5453

/* ADB authentication types */
#define ADB_AUTH_TOKEN         1
#define ADB_AUTH_SIGNATURE     2
#define ADB_AUTH_RSAPUBLICKEY  3

/* File Sync protocol IDs (little-endian) */
#define SYNC_STAT  0x54415453  /* "STAT" */
#define SYNC_LIST  0x5453494c  /* "LIST" */
#define SYNC_SEND  0x444e4553  /* "SEND" */
#define SYNC_RECV  0x56434552  /* "RECV" */
#define SYNC_DATA  0x41544144  /* "DATA" */
#define SYNC_DONE  0x454e4f44  /* "DONE" */
#define SYNC_OKAY  0x59414b4f  /* "OKAY" */
#define SYNC_FAIL  0x4c494146  /* "FAIL" */
#define SYNC_DENT  0x544e4544  /* "DENT" - directory entry */
#define SYNC_QUIT  0x54495551  /* "QUIT" */

/* Shell protocol v2 packet IDs */
#define SHELL_ID_STDIN              0
#define SHELL_ID_STDOUT             1
#define SHELL_ID_STDERR             2
#define SHELL_ID_EXIT               3
#define SHELL_ID_CLOSE_STDIN        4
#define SHELL_ID_WINDOW_SIZE_CHANGE 5

typedef struct {
    ArkimeParserBuf_t *pb;      /* Buffer for incomplete messages */
    uint8_t  connected[2];      /* Have we seen CONNECT message */
    uint8_t  tls[2];            /* Is TLS enabled */
    uint8_t  tlsHandshakeDone;  /* Have both sides done TLS */
    uint8_t  isClientServer[2]; /* Is this client-server protocol (port 5037) */
    uint32_t expectedLen[2];    /* Expected length for client-server protocol */
    uint8_t  syncMode[2];       /* Is this stream in sync mode */
    uint8_t  shellV2[2];        /* Is this stream using shell v2 protocol */
    uint32_t localStreamId[2];  /* Track stream IDs for service mapping */
} ADBInfo_t;

LOCAL int versionField;
LOCAL int serialField;
LOCAL int systemTypeField;
LOCAL int commandField;
LOCAL int serviceField;
LOCAL int maxPayloadField;
LOCAL int authTypeField;
LOCAL int streamIdField;

/* New fields for enhanced features */
LOCAL int syncOperationField;
LOCAL int syncPathField;
LOCAL int shellExitCodeField;
LOCAL int forwardLocalField;
LOCAL int forwardRemoteField;
LOCAL int forwardDirectionField;

extern ArkimeConfig_t        config;

/******************************************************************************/
LOCAL const char *adb_command_to_string(uint32_t cmd)
{
    switch (cmd) {
    case A_SYNC:
        return "sync";
    case A_CNXN:
        return "cnxn";
    case A_OPEN:
        return "open";
    case A_OKAY:
        return "okay";
    case A_CLSE:
        return "clse";
    case A_WRTE:
        return "wrte";
    case A_AUTH:
        return "auth";
    case A_STLS:
        return "stls";
    default:
        return "unknown";
    }
}

/******************************************************************************/
LOCAL const char *sync_id_to_string(uint32_t id)
{
    switch (id) {
    case SYNC_STAT:
        return "STAT";
    case SYNC_LIST:
        return "LIST";
    case SYNC_SEND:
        return "SEND";
    case SYNC_RECV:
        return "RECV";
    case SYNC_DATA:
        return "DATA";
    case SYNC_DONE:
        return "DONE";
    case SYNC_OKAY:
        return "OKAY";
    case SYNC_FAIL:
        return "FAIL";
    case SYNC_DENT:
        return "DENT";
    case SYNC_QUIT:
        return "QUIT";
    default:
        return NULL;
    }
}

/******************************************************************************/
LOCAL void adb_parse_sync_payload(ArkimeSession_t *session, const uint8_t *data, int len)
{
    /* Sync protocol messages are 8 bytes minimum: 4-byte ID + 4-byte length/mode */
    if (len < 8)
        return;

    BSB bsb;
    BSB_INIT(bsb, data, len);

    while (BSB_REMAINING(bsb) >= 8) {
        uint32_t id = 0, arg = 0;
        BSB_LIMPORT_u32(bsb, id);
        BSB_LIMPORT_u32(bsb, arg);
        if (BSB_IS_ERROR(bsb)) break;

        const char *op_str = sync_id_to_string(id);
        if (op_str) {
            arkime_field_string_add(syncOperationField, session, op_str, -1, TRUE);
        }

        /* For SEND/RECV/STAT/LIST, the arg is the path length, followed by path */
        if ((id == SYNC_SEND || id == SYNC_RECV || id == SYNC_STAT || id == SYNC_LIST) && arg > 0 && arg < 4096) {
            if (BSB_REMAINING(bsb) >= arg) {
                const uint8_t *path = BSB_WORK_PTR(bsb);
                int path_len = MIN(arg, 1023);
                char path_str[1024];
                memcpy(path_str, path, path_len);
                path_str[path_len] = '\0';

                /* For SEND, path may have ",mode" suffix - strip it */
                char *comma = strchr(path_str, ',');
                if (comma) {
                    *comma = '\0';
                }

                arkime_field_string_add(syncPathField, session, path_str, -1, TRUE);
                BSB_IMPORT_skip(bsb, arg);
            } else {
                break;
            }
        } else if (id == SYNC_DATA && arg > 0) {
            /* DATA packet - skip the data payload */
            if (BSB_REMAINING(bsb) >= arg) {
                BSB_IMPORT_skip(bsb, arg);
            } else {
                break;
            }
        } else if (id == SYNC_FAIL && arg > 0 && arg < 256) {
            /* FAIL packet has error message */
            BSB_IMPORT_skip(bsb, MIN(arg, BSB_REMAINING(bsb)));
        }
        /* DONE, OKAY, QUIT have no additional data */
    }
}

/******************************************************************************/
LOCAL void adb_parse_shell_v2_payload(ArkimeSession_t *session, const uint8_t *data, int len)
{
    /* Shell v2 protocol: 1-byte ID + 4-byte length + payload */
    BSB bsb;
    BSB_INIT(bsb, data, len);

    while (BSB_REMAINING(bsb) >= 5) {
        uint8_t id = 0;
        uint32_t pkt_len = 0;
        BSB_IMPORT_u08(bsb, id);
        BSB_LIMPORT_u32(bsb, pkt_len);

        if (pkt_len > BSB_REMAINING(bsb)) {
            break;
        }

        if (id == SHELL_ID_EXIT && pkt_len >= 1) {
            /* Exit code is in the payload */
            uint8_t exit_code = 0;
            BSB_IMPORT_u08(bsb, exit_code);
            char exit_str[16];
            snprintf(exit_str, sizeof(exit_str), "%u", exit_code);
            arkime_field_string_add(shellExitCodeField, session, exit_str, -1, TRUE);
            if (pkt_len > 1) {
                BSB_IMPORT_skip(bsb, pkt_len - 1);
            }
        } else {
            BSB_IMPORT_skip(bsb, pkt_len);
        }
    }
}

/******************************************************************************/
LOCAL void adb_parse_forward_service(ArkimeSession_t *session, const char *service)
{
    /* Parse forward:<local>;<remote> or reverse:forward:<remote>;<local> */
    const char *direction = NULL;
    const char *endpoints = NULL;

    if (strncmp(service, "reverse:forward:", 16) == 0) {
        direction = "reverse";
        endpoints = service + 16;
    } else if (strncmp(service, "forward:", 8) == 0) {
        direction = "forward";
        endpoints = service + 8;
    } else if (strncmp(service, "reverse:", 8) == 0) {
        /* reverse:list or other reverse commands */
        direction = "reverse";
        arkime_field_string_add(forwardDirectionField, session, direction, -1, TRUE);
        return;
    } else {
        return;
    }

    arkime_field_string_add(forwardDirectionField, session, direction, -1, TRUE);

    /* Parse <local>;<remote> */
    if (endpoints) {
        char buf[512];
        g_strlcpy(buf, endpoints, sizeof(buf));

        char *semicolon = strchr(buf, ';');
        if (semicolon) {
            *semicolon = '\0';
            arkime_field_string_add(forwardLocalField, session, buf, -1, TRUE);
            arkime_field_string_add(forwardRemoteField, session, semicolon + 1, -1, TRUE);
        }
    }
}

/******************************************************************************/
LOCAL void adb_parse_connect(ArkimeSession_t *session, const uint8_t *data, int remaining, int UNUSED(which))
{
    if (remaining < 24)
        return;

    BSB bsb;
    BSB_INIT(bsb, data, remaining);

    uint32_t command = 0, arg0 = 0, arg1 = 0, data_length = 0, magic = 0;
    BSB_LIMPORT_u32(bsb, command);
    BSB_LIMPORT_u32(bsb, arg0);  /* version */
    BSB_LIMPORT_u32(bsb, arg1);  /* maxdata */
    BSB_LIMPORT_u32(bsb, data_length);
    BSB_LIMPORT_skip(bsb, 4); // data_check
    BSB_LIMPORT_u32(bsb, magic);

    /* Validate magic field */
    if (magic != (command ^ 0xffffffff)) {
        return;
    }

    if (command != A_CNXN) {
        return;
    }

    /* Extract version */
    char version_str[32];
    snprintf(version_str, sizeof(version_str), "0x%08x", arg0);
    arkime_field_string_add(versionField, session, version_str, -1, TRUE);

    /* Extract max payload */
    char maxpayload_str[32];
    snprintf(maxpayload_str, sizeof(maxpayload_str), "%u", arg1);
    arkime_field_string_add(maxPayloadField, session, maxpayload_str, -1, TRUE);

    /* Parse banner payload */
    if (data_length > 0 && BSB_REMAINING(bsb) >= data_length) {
        const uint8_t *banner = BSB_WORK_PTR(bsb);
        int banner_len = MIN(data_length, 1023);
        char banner_str[1024];
        memcpy(banner_str, banner, banner_len);
        banner_str[banner_len] = '\0';

        /* Parse "<systemtype>:<serialno>:<banner>" */
        char *colon1 = strchr(banner_str, ':');
        if (colon1) {
            *colon1 = '\0';
            arkime_field_string_add_lower(systemTypeField, session, banner_str, colon1 - banner_str);

            char *colon2 = strchr(colon1 + 1, ':');
            if (colon2) {
                *colon2 = '\0';
                if (colon2 > colon1 + 1) {
                    arkime_field_string_add(serialField, session, colon1 + 1, colon2 - (colon1 + 1), TRUE);
                }
            }
        }
    }
}

/******************************************************************************/
LOCAL void adb_parse_open(ArkimeSession_t *session, const uint8_t *data, int remaining, int which, ADBInfo_t *adb)
{
    if (remaining < 24)
        return;

    BSB bsb;
    BSB_INIT(bsb, data, remaining);

    uint32_t command = 0, arg0 = 0, data_length = 0, magic = 0;
    BSB_LIMPORT_u32(bsb, command);
    BSB_LIMPORT_u32(bsb, arg0);  /* local-id */
    BSB_LIMPORT_skip(bsb, 4); // arg1
    BSB_LIMPORT_u32(bsb, data_length);
    BSB_LIMPORT_skip(bsb, 4); // data_check
    BSB_LIMPORT_u32(bsb, magic);

    if (magic != (command ^ 0xffffffff) || command != A_OPEN) {
        return;
    }

    /* Store stream ID for tracking */
    adb->localStreamId[which] = arg0;

    /* Extract stream ID */
    char stream_str[32];
    snprintf(stream_str, sizeof(stream_str), "%u", arg0);
    arkime_field_string_add(streamIdField, session, stream_str, -1, TRUE);

    /* Extract service name from payload */
    if (data_length > 0 && BSB_REMAINING(bsb) >= data_length) {
        const uint8_t *service = BSB_WORK_PTR(bsb);
        int service_len = MIN(data_length, 511);
        char service_str[512];
        memcpy(service_str, service, service_len);
        service_str[service_len] = '\0';

        /* Remove null terminator if present */
        if (service_len > 0 && service_str[service_len - 1] == '\0') {
            service_len--;
        }

        /* Check for sync mode */
        if (strncmp(service_str, "sync:", 5) == 0) {
            adb->syncMode[which] = 1;
            arkime_session_add_tag(session, "adb:sync");
        }

        /* Check for shell v2 */
        if (strncmp(service_str, "shell,v2:", 9) == 0 || strncmp(service_str, "shell,V2:", 9) == 0) {
            adb->shellV2[which] = 1;
            arkime_session_add_tag(session, "adb:shell-v2");
        }

        /* Check for port forwarding */
        if (strncmp(service_str, "forward:", 8) == 0 || strncmp(service_str, "reverse:", 8) == 0) {
            adb_parse_forward_service(session, service_str);
        }

        /* Extract base service name (before colon or comma) */
        char *delim = strpbrk(service_str, ":,");
        if (delim && delim > service_str) {
            char base_service[64];
            int base_len = MIN(delim - service_str, 63);
            memcpy(base_service, service_str, base_len);
            base_service[base_len] = '\0';
            arkime_field_string_add_lower(serviceField, session, base_service, base_len);
        } else {
            arkime_field_string_add_lower(serviceField, session, service_str, strlen(service_str));
        }
    }
}

/******************************************************************************/
LOCAL void adb_parse_client_server(ArkimeSession_t *session, const uint8_t *data, int remaining, int which, ADBInfo_t *adb)
{
    /* Client-server protocol format: <hex4><service-name> */
    /* hex4 is a 4-byte hexadecimal string (ASCII) indicating length */

    /* Add data to buffer */
    arkime_parser_buf_add(adb->pb, which, data, remaining);

    /* Need at least 4 bytes for hex length */
    if (adb->pb->len[which] < 4)
        return;

    /* Check if we have a valid hex string */
    if (adb->expectedLen[which] == 0) {
        char hex_str[5];
        memcpy(hex_str, adb->pb->buf[which], 4);
        hex_str[4] = '\0';

        /* Validate hex string */
        int valid = 1;
        for (int i = 0; i < 4; i++) {
            if (!isxdigit(hex_str[i])) {
                valid = 0;
                break;
            }
        }

        if (valid) {
            adb->expectedLen[which] = (uint32_t)strtoul(hex_str, NULL, 16);
            if (adb->expectedLen[which] > 4096) {
                /* Invalid length, reset */
                adb->pb->len[which] = 0;
                adb->expectedLen[which] = 0;
                return;
            }
        } else {
            /* Not valid hex, might not be client-server protocol */
            adb->isClientServer[which] = 0;
            return;
        }
    }

    /* Check if we have complete message (4 bytes hex + service name) */
    if ((uint32_t)adb->pb->len[which] >= 4 + adb->expectedLen[which]) {
        const uint8_t *service = adb->pb->buf[which] + 4;
        int service_len = adb->expectedLen[which];

        /* Extract service name */
        char service_str[512];
        int copy_len = MIN(service_len, 511);
        memcpy(service_str, service, copy_len);
        service_str[copy_len] = '\0';

        /* Check for port forwarding */
        if (strncmp(service_str, "forward:", 8) == 0 ||
            strncmp(service_str, "reverse:", 8) == 0 ||
            strncmp(service_str, "host:forward:", 13) == 0 ||
            strncmp(service_str, "host:reverse:", 13) == 0) {
            const char *fwd_start = strstr(service_str, "forward:");
            if (!fwd_start) fwd_start = strstr(service_str, "reverse:");
            if (fwd_start) {
                adb_parse_forward_service(session, fwd_start);
            }
        }

        /* Extract base service name */
        char *colon = strchr(service_str, ':');
        if (colon && colon > service_str) {
            char base_service[64];
            int base_len = MIN(colon - service_str, 63);
            memcpy(base_service, service_str, base_len);
            base_service[base_len] = '\0';
            arkime_field_string_add_lower(serviceField, session, base_service, base_len);
        } else {
            arkime_field_string_add_lower(serviceField, session, service_str, strlen(service_str));
        }

        /* Remove processed message */
        uint32_t total_len = 4 + adb->expectedLen[which];
        arkime_parser_buf_del(adb->pb, which, total_len);
        adb->expectedLen[which] = 0;
    }
}

/******************************************************************************/
LOCAL int adb_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int remaining, int which)
{
    ADBInfo_t *adb = uw;

    /* If TLS handshake is complete on both sides, hand off to TLS classifier */
    if (adb->tlsHandshakeDone) {
        arkime_parsers_classify_tcp(session, data, remaining, which);
        return ARKIME_PARSER_UNREGISTER;
    }

    /* Check if this is client-server protocol (port 5037) */
    /* Client-server protocol starts with 4 hex digits (ASCII) */
    if (adb->isClientServer[which] == 0 && adb->pb->len[which] == 0 && remaining >= 4) {
        /* Check if first 4 bytes are hex digits */
        int is_hex = 1;
        for (int i = 0; i < 4 && i < remaining; i++) {
            if (!isxdigit(data[i])) {
                is_hex = 0;
                break;
            }
        }
        if (is_hex) {
            adb->isClientServer[which] = 1;
        }
    }

    /* Handle client-server protocol differently */
    if (adb->isClientServer[which]) {
        adb_parse_client_server(session, data, remaining, which, adb);
        return 0;
    }

    /* Add data to buffer for transport protocol */
    arkime_parser_buf_add(adb->pb, which, data, remaining);

    /* Process complete messages */
    while (adb->pb->len[which] >= 24) {
        BSB bsb;
        BSB_INIT(bsb, adb->pb->buf[which], adb->pb->len[which]);

        uint32_t command = 0, arg0 = 0, arg1 = 0, data_length = 0, magic = 0;
        BSB_LIMPORT_u32(bsb, command);
        BSB_LIMPORT_u32(bsb, arg0);
        BSB_LIMPORT_u32(bsb, arg1);
        BSB_LIMPORT_u32(bsb, data_length);
        BSB_LIMPORT_skip(bsb, 4); // data_check
        BSB_LIMPORT_u32(bsb, magic);

        /* Validate magic field */
        if (magic != (command ^ 0xffffffff)) {
            /* Invalid message, clear buffer */
            adb->pb->len[which] = 0;
            return 0;
        }

        /* Sanity check data_length to prevent integer overflow */
        if (data_length > (uint32_t)adb->pb->bufMax - 24) {
            adb->pb->len[which] = 0;
            return 0;
        }

        /* Check if we have complete message (header + payload) */
        uint32_t total_len = 24 + data_length;
        if ((uint32_t)adb->pb->len[which] < total_len) {
            /* Need more data */
            return 0;
        }

        /* Record command type */
        const char *cmd_str = adb_command_to_string(command);
        arkime_field_string_add_lower(commandField, session, cmd_str, -1);

        /* Process specific commands */
        switch (command) {
        case A_CNXN:
            adb_parse_connect(session, adb->pb->buf[which], total_len, which);
            adb->connected[which] = 1;
            break;

        case A_OPEN:
            /* Parse OPEN regardless of connected state - service info is valuable */
            adb_parse_open(session, adb->pb->buf[which], total_len, which, adb);
            break;

        case A_AUTH:
            /* Parse AUTH regardless of connected state (fixes Priority 1.1) */
        {
            const char *auth_type_str = "unknown";
            switch (arg0) {
            case ADB_AUTH_TOKEN:
                auth_type_str = "token";
                break;
            case ADB_AUTH_SIGNATURE:
                auth_type_str = "signature";
                break;
            case ADB_AUTH_RSAPUBLICKEY:
                auth_type_str = "rsapublickey";
                break;
            }
            arkime_field_string_add_lower(authTypeField, session, auth_type_str, -1);
        }
        break;

        case A_STLS:
            adb->tls[which] = 1;
            arkime_session_add_tag(session, "adb:tls");
            /* Check if both sides have done TLS */
            if (adb->tls[0] && adb->tls[1]) {
                adb->tlsHandshakeDone = 1;
                arkime_session_add_tag(session, "adb:tls-encrypted");
            }
            break;

        case A_OKAY:
            /* Extract stream IDs */
        {
            char stream_str[64];
            snprintf(stream_str, sizeof(stream_str), "%u-%u", arg0, arg1);
            arkime_field_string_add(streamIdField, session, stream_str, -1, TRUE);
        }
        break;

        case A_WRTE:
            /* Parse WRTE payload for sync and shell v2 protocols */
            if (data_length > 0) {
                const uint8_t *payload = adb->pb->buf[which] + 24;

                /* Check if this stream is in sync mode */
                if (adb->syncMode[0] || adb->syncMode[1]) {
                    adb_parse_sync_payload(session, payload, data_length);
                }

                /* Check if this stream is using shell v2 */
                if (adb->shellV2[0] || adb->shellV2[1]) {
                    adb_parse_shell_v2_payload(session, payload, data_length);
                }
            }
            break;

        case A_CLSE:
        case A_SYNC:
            /* These are less interesting for parsing, but we've already logged the command */
            break;
        }

        /* Remove processed message from buffer */
        arkime_parser_buf_del(adb->pb, which, total_len);
    }

    return 0;
}

/******************************************************************************/
LOCAL void adb_free(ArkimeSession_t UNUSED(*session), void *uw)
{
    ADBInfo_t *adb = uw;
    arkime_parser_buf_free(adb->pb);
    ARKIME_TYPE_FREE(ADBInfo_t, adb);
}

/******************************************************************************/
LOCAL void adb_classify_client_server(ArkimeSession_t *session, const uint8_t *data, int len, int which, void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "adb"))
        return;

    /* Client-server protocol starts with 4 hex digits (ASCII) */
    if (len < 4)
        return;

    /* Check if first 4 bytes are hex digits */
    int is_hex = 1;
    for (int i = 0; i < 4; i++) {
        if (!isxdigit(data[i])) {
            is_hex = 0;
            break;
        }
    }

    if (!is_hex)
        return;

    /* Parse hex length */
    char hex_str[5];
    memcpy(hex_str, data, 4);
    hex_str[4] = '\0';
    uint32_t expected_len = (uint32_t)strtoul(hex_str, NULL, 16);

    /* Validate reasonable length */
    if (expected_len == 0 || expected_len > 4096)
        return;

    /* Looks like ADB client-server protocol */
    arkime_session_add_protocol(session, "adb");

    ADBInfo_t *adb = ARKIME_TYPE_ALLOC0(ADBInfo_t);
    adb->pb = arkime_parser_buf_create();
    adb->isClientServer[which] = 1;
    arkime_parsers_register(session, adb_parser, adb, adb_free);
}

/******************************************************************************/
LOCAL void adb_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "adb"))
        return;

    /* Need at least 24 bytes for ADB message header */
    if (len < 24)
        return;

    /* Check for valid ADB message header (little-endian) */
    uint32_t command, magic;
    command = ((uint32_t)data[0]) | ((uint32_t)data[1] << 8) |
              ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    magic = ((uint32_t)data[20]) | ((uint32_t)data[21] << 8) |
            ((uint32_t)data[22] << 16) | ((uint32_t)data[23] << 24);

    /* Validate magic field (command ^ 0xffffffff) */
    if (magic != (command ^ 0xffffffff)) {
        return;
    }

    /* Check if command is a known ADB command */
    switch (command) {
    case A_SYNC:
    case A_CNXN:
    case A_OPEN:
    case A_OKAY:
    case A_CLSE:
    case A_WRTE:
    case A_AUTH:
    case A_STLS:
        break;
    default:
        return;
    }

    /* Looks like ADB protocol */
    arkime_session_add_protocol(session, "adb");

    ADBInfo_t *adb = ARKIME_TYPE_ALLOC0(ADBInfo_t);
    adb->pb = arkime_parser_buf_create();
    arkime_parsers_register(session, adb_parser, adb, adb_free);
}

/******************************************************************************/
void arkime_parser_init()
{
    /* Register for common ADB ports */
    /* Port 5037: ADB server (client-server protocol) */
    /* Port 5555: ADB device connections (default) */
    arkime_parsers_classifier_register_port("adb", NULL, 5037, ARKIME_PARSERS_PORT_TCP, adb_classify_client_server);
    arkime_parsers_classifier_register_port("adb", NULL, 5555, ARKIME_PARSERS_PORT_TCP, adb_classify);

    /* Also try to classify by magic bytes (ADB message header) */
    /* Look for A_CNXN (CONNECT) which is typically the first message */
    arkime_parsers_classifier_register_tcp("adb", NULL, 0, (uint8_t *)"\x43\x4e\x58\x4e", 4, adb_classify); /* A_CNXN */

    /* Also match A_AUTH for cases where AUTH comes before CNXN */
    arkime_parsers_classifier_register_tcp("adb", NULL, 0, (uint8_t *)"\x41\x55\x54\x48", 4, adb_classify); /* A_AUTH */

    /* Define fields */
    versionField = arkime_field_define("adb", "termfield",
                                       "adb.version", "Version", "adb.version",
                                       "ADB Protocol Version",
                                       ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    serialField = arkime_field_define("adb", "lotermfield",
                                      "adb.serial", "Serial", "adb.serial",
                                      "ADB Device Serial Number",
                                      ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                      (char *)NULL);

    systemTypeField = arkime_field_define("adb", "lotermfield",
                                          "adb.systemtype", "System Type", "adb.systemtype",
                                          "ADB System Type (device, host, bootloader, etc.)",
                                          ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    commandField = arkime_field_define("adb", "lotermfield",
                                       "adb.command", "Command", "adb.command",
                                       "ADB Command Type",
                                       ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    serviceField = arkime_field_define("adb", "lotermfield",
                                       "adb.service", "Service", "adb.service",
                                       "ADB Service Name",
                                       ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    maxPayloadField = arkime_field_define("adb", "termfield",
                                          "adb.maxpayload", "Max Payload", "adb.maxpayload",
                                          "ADB Maximum Payload Size",
                                          ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                          (char *)NULL);

    authTypeField = arkime_field_define("adb", "lotermfield",
                                        "adb.authtype", "Auth Type", "adb.authtype",
                                        "ADB Authentication Type",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    streamIdField = arkime_field_define("adb", "termfield",
                                        "adb.streamid", "Stream ID", "adb.streamid",
                                        "ADB Stream Identifier",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    /* New fields for enhanced features */
    syncOperationField = arkime_field_define("adb", "lotermfield",
                                             "adb.sync.operation", "Sync Operation", "adb.sync.operation",
                                             "ADB File Sync Operation (SEND, RECV, LIST, STAT)",
                                             ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                             (char *)NULL);

    syncPathField = arkime_field_define("adb", "lotermfield",
                                        "adb.sync.path", "Sync Path", "adb.sync.path",
                                        "ADB File Sync Path",
                                        ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    shellExitCodeField = arkime_field_define("adb", "termfield",
                                             "adb.shell.exitcode", "Shell Exit Code", "adb.shell.exitcode",
                                             "ADB Shell Command Exit Code",
                                             ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                             (char *)NULL);

    forwardLocalField = arkime_field_define("adb", "lotermfield",
                                            "adb.forward.local", "Forward Local", "adb.forward.local",
                                            "ADB Port Forward Local Endpoint",
                                            ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                            (char *)NULL);

    forwardRemoteField = arkime_field_define("adb", "lotermfield",
                                             "adb.forward.remote", "Forward Remote", "adb.forward.remote",
                                             "ADB Port Forward Remote Endpoint",
                                             ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                             (char *)NULL);

    forwardDirectionField = arkime_field_define("adb", "lotermfield",
                                                "adb.forward.direction", "Forward Direction", "adb.forward.direction",
                                                "ADB Port Forward Direction (forward or reverse)",
                                                ARKIME_FIELD_TYPE_STR_HASH,  ARKIME_FIELD_FLAG_CNT,
                                                (char *)NULL);
}
