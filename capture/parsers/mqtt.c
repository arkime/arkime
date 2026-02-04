/* mqtt.c
 *
 * Copyright 2026 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * MQTT Protocol Parser
 * Supports MQTT 3.1, 3.1.1, and 5.0
 */

#include "arkime.h"

extern ArkimeConfig_t        config;

LOCAL int clientIdField;
LOCAL int topicField;
LOCAL int versionField;
LOCAL int typeField;
LOCAL int userField;
LOCAL int willTopicField;
LOCAL int qosField;
LOCAL int flagsField;

// MQTT packet types
LOCAL const char *mqttTypes[] = {
    "Reserved",      // 0
    "CONNECT",       // 1
    "CONNACK",       // 2
    "PUBLISH",       // 3
    "PUBACK",        // 4
    "PUBREC",        // 5
    "PUBREL",        // 6
    "PUBCOMP",       // 7
    "SUBSCRIBE",     // 8
    "SUBACK",        // 9
    "UNSUBSCRIBE",   // 10
    "UNSUBACK",      // 11
    "PINGREQ",       // 12
    "PINGRESP",      // 13
    "DISCONNECT",    // 14
    "AUTH"           // 15 (MQTT 5.0 only)
};

/******************************************************************************/
// Decode MQTT variable length integer
LOCAL int mqtt_decode_varint(BSB *bsb, uint32_t *value)
{
    *value = 0;
    int shift = 0;
    uint8_t byte = 0;

    for (int i = 0; i < 4; i++) {
        BSB_IMPORT_u08(*bsb, byte);
        if (BSB_IS_ERROR(*bsb))
            return -1;

        *value |= (byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            return 0;
        shift += 7;
    }
    return -1; // Malformed varint
}
/******************************************************************************/
LOCAL void mqtt_parse_connect(ArkimeSession_t *session, BSB *bsb)
{
    // Protocol name length + name
    int protoNameLen = 0;
    BSB_IMPORT_u16(*bsb, protoNameLen);
    if (BSB_IS_ERROR(*bsb) || protoNameLen > BSB_REMAINING(*bsb))
        return;

    const uint8_t *protoName = 0;
    BSB_IMPORT_ptr(*bsb, protoName, protoNameLen);

    // Verify it's MQTT or MQIsdp (MQTT 3.1)
    if (protoNameLen != 4 && protoNameLen != 6)
        return;
    if (protoNameLen == 4 && memcmp(protoName, "MQTT", 4) != 0)
        return;
    if (protoNameLen == 6 && memcmp(protoName, "MQIsdp", 6) != 0)
        return;

    // Protocol version
    int version = 0;
    BSB_IMPORT_u08(*bsb, version);
    if (BSB_IS_ERROR(*bsb))
        return;

    // Map version numbers to display values
    const char *versionStr;
    switch (version) {
    case 3:
        versionStr = "3.1";
        break;
    case 4:
        versionStr = "3.1.1";
        break;
    case 5:
        versionStr = "5.0";
        break;
    default:
        versionStr = "unknown";
        break;
    }
    arkime_field_string_add(versionField, session, versionStr, -1, TRUE);

    // Connect flags
    int flags = 0;
    BSB_IMPORT_u08(*bsb, flags);
    if (BSB_IS_ERROR(*bsb))
        return;

    int hasUsername = (flags & 0x80) != 0;
    int hasPassword = (flags & 0x40) != 0;
    int willRetain = (flags & 0x20) != 0;
    int willQoS = (flags >> 3) & 0x03;
    int hasWill = (flags & 0x04) != 0;
    int cleanSession = (flags & 0x02) != 0;

    (void)willRetain;

    if (hasWill) {
        arkime_field_string_add(flagsField, session, "hasWill", -1, TRUE);
        if (willQoS <= 2) {
            arkime_field_int_add(qosField, session, willQoS);
        }
        if (willRetain) {
            arkime_field_string_add(flagsField, session, "willRetain", -1, TRUE);
        }
    }

    if (cleanSession) {
        arkime_field_string_add(flagsField, session, "cleanSession", -1, TRUE);
    }

    if (hasPassword) {
        arkime_field_string_add(flagsField, session, "hasPassword", -1, TRUE);
    }

    // Keep alive (skip)
    BSB_IMPORT_skip(*bsb, 2);

    // MQTT 5.0 properties (skip for now)
    if (version == 5) {
        uint32_t propsLen = 0;
        if (mqtt_decode_varint(bsb, &propsLen) < 0)
            return;
        BSB_IMPORT_skip(*bsb, propsLen);
    }

    // Client ID
    int clientIdLen = 0;
    uint8_t *clientId = 0;
    BSB_IMPORT_u16(*bsb, clientIdLen);
    BSB_IMPORT_ptr(*bsb, clientId, clientIdLen);
    if (BSB_NOT_ERROR(*bsb) && clientIdLen > 0) {
        arkime_field_string_add(clientIdField, session, (char *)clientId, clientIdLen, TRUE);
    }

    // Will properties (MQTT 5.0)
    if (hasWill && version == 5) {
        uint32_t willPropsLen = 0;
        if (mqtt_decode_varint(bsb, &willPropsLen) < 0)
            return;
        BSB_IMPORT_skip(*bsb, willPropsLen);
    }

    // Will topic
    if (hasWill) {
        int willTopicLen = 0;
        uint8_t *willTopic = 0;
        BSB_IMPORT_u16(*bsb, willTopicLen);
        BSB_IMPORT_ptr(*bsb, willTopic, willTopicLen);
        if (BSB_NOT_ERROR(*bsb) && willTopicLen > 0) {
            arkime_field_string_add(willTopicField, session, (char *)willTopic, willTopicLen, TRUE);
        }

        // Will message (skip)
        int willMsgLen = 0;
        BSB_IMPORT_u16(*bsb, willMsgLen);
        BSB_IMPORT_skip(*bsb, willMsgLen);
    }

    // Username
    if (hasUsername) {
        int userLen = 0;
        uint8_t *user = 0;
        BSB_IMPORT_u16(*bsb, userLen);
        BSB_IMPORT_ptr(*bsb, user, userLen);
        if (BSB_NOT_ERROR(*bsb) && userLen > 0) {
            arkime_field_string_add_lower(userField, session, (char *)user, userLen);
        }
    }
}
/******************************************************************************/
// Returns total bytes to skip (header + payload) on success, or -1 if need more data
LOCAL int mqtt_parse_publish(ArkimeSession_t *session, ArkimeParserBuf_t *mqtt, int which, BSB *bsb, int flags, uint32_t remainingLen)
{
    int qos = (flags >> 1) & 0x03;

    // Need at least 2 bytes for topic length
    if (BSB_REMAINING(*bsb) < 2)
        return -1;

    // Peek at topic length without consuming
    int topicLen = (mqtt->buf[which][BSB_WORK_PTR(*bsb) - mqtt->buf[which]] << 8) |
                   mqtt->buf[which][BSB_WORK_PTR(*bsb) - mqtt->buf[which] + 1];

    // Calculate how much of the header we need: 2 (topic len) + topicLen + (qos > 0 ? 2 : 0)
    int headerNeeded = 2 + topicLen + (qos > 0 ? 2 : 0);
    if (headerNeeded > (int)remainingLen)
        return -1; // Malformed

    if (BSB_REMAINING(*bsb) < headerNeeded)
        return -1; // Need more data

    // Now consume
    BSB_IMPORT_skip(*bsb, 2); // topic length already peeked
    uint8_t *topic = NULL;
    BSB_IMPORT_ptr(*bsb, topic, topicLen);
    if (BSB_NOT_ERROR(*bsb) && topicLen > 0) {
        arkime_field_string_add(topicField, session, (char *)topic, topicLen, TRUE);
    }

    if (qos <= 2) {
        arkime_field_int_add(qosField, session, qos);
    }
    if (qos > 0) {
        BSB_IMPORT_skip(*bsb, 2); // packet ID only for QoS > 0
    }

    // Return total bytes consumed from buffer (fixed header already consumed before call)
    // plus remaining payload to skip
    int consumed = BSB_WORK_PTR(*bsb) - mqtt->buf[which];
    return consumed + (remainingLen - headerNeeded);
}
/******************************************************************************/
LOCAL void mqtt_parse_subscribe(ArkimeSession_t *session, BSB *bsb, int version)
{
    // Packet identifier (skip)
    BSB_IMPORT_skip(*bsb, 2);

    // MQTT 5.0 properties
    if (version == 5) {
        uint32_t propsLen = 0;
        if (mqtt_decode_varint(bsb, &propsLen) < 0)
            return;
        BSB_IMPORT_skip(*bsb, propsLen);
    }

    // Topic filters
    while (BSB_REMAINING(*bsb) > 2) {
        int topicLen = 0;
        uint8_t *topic = 0;
        BSB_IMPORT_u16(*bsb, topicLen);
        BSB_IMPORT_ptr(*bsb, topic, topicLen);
        if (BSB_IS_ERROR(*bsb))
            break;

        if (topicLen > 0) {
            arkime_field_string_add(topicField, session, (char *)topic, topicLen, TRUE);
        }

        // QoS (skip)
        BSB_IMPORT_skip(*bsb, 1);
    }
}
/******************************************************************************/
LOCAL int mqtt_parser(ArkimeSession_t *session, void *uw, const uint8_t *data, int len, int which)
{
    ArkimeParserBuf_t *mqtt = uw;

    arkime_parser_buf_add(mqtt, which, data, len);

    BSB bsb;
    BSB_INIT(bsb, mqtt->buf[which], mqtt->len[which]);

    while (BSB_REMAINING(bsb) >= 2) {
        // Fixed header
        int packetType = 0;
        BSB_IMPORT_u08(bsb, packetType);

        int flags = packetType & 0x0F;
        packetType = (packetType >> 4) & 0x0F;

        // Remaining length
        uint32_t remainingLen = 0;
        if (mqtt_decode_varint(&bsb, &remainingLen) < 0)
            break;

        if (remainingLen > BSB_REMAINING(bsb)) {
            break;
        }

        // Add packet type
        if (packetType < (int)ARRAY_LEN(mqttTypes)) {
            arkime_field_string_add(typeField, session, mqttTypes[packetType], -1, TRUE);
        }

        // Handle PUBLISH specially - can skip large payloads
        if (packetType == 3) {
            int skipLen = mqtt_parse_publish(session, mqtt, which, &bsb, flags, remainingLen);
            if (skipLen < 0)
                break; // Need more data

            arkime_parser_buf_skip(mqtt, which, skipLen);
            BSB_INIT(bsb, mqtt->buf[which], mqtt->len[which]);
            continue;
        }

        if (remainingLen > BSB_REMAINING(bsb)) {
            break;
        }

        // Parse based on packet type
        BSB packetBsb;
        BSB_INIT(packetBsb, BSB_WORK_PTR(bsb), remainingLen);

        switch (packetType) {
        case 1: // CONNECT
            mqtt_parse_connect(session, &packetBsb);
            break;
        case 8: // SUBSCRIBE
            mqtt_parse_subscribe(session, &packetBsb, mqtt->version);
            break;
        case 10: // UNSUBSCRIBE
            mqtt_parse_subscribe(session, &packetBsb, mqtt->version);
            break;
        }

        BSB_IMPORT_skip(bsb, remainingLen);

        if (BSB_IS_ERROR(bsb))
            break;

        // Delete processed data and reinit BSB
        int processed = BSB_WORK_PTR(bsb) - mqtt->buf[which];
        arkime_parser_buf_del(mqtt, which, processed);
        BSB_INIT(bsb, mqtt->buf[which], mqtt->len[which]);
    }

    return 0;
}
/******************************************************************************/
LOCAL void mqtt_classify(ArkimeSession_t *session, const uint8_t *data, int len, int UNUSED(which), void *UNUSED(uw))
{
    if (arkime_session_has_protocol(session, "mqtt"))
        return;

    if (len < 10)
        return;

    // Check for CONNECT packet (type 1)
    if ((data[0] & 0xF0) != 0x10)
        return;

    // Decode remaining length
    BSB bsb;
    BSB_INIT(bsb, data + 1, len - 1);
    uint32_t remainingLen = 0;
    if (mqtt_decode_varint(&bsb, &remainingLen) < 0)
        return;

    // Check protocol name
    int protoNameLen = 0;
    BSB_IMPORT_u16(bsb, protoNameLen);
    if (BSB_IS_ERROR(bsb))
        return;

    if (protoNameLen != 4 && protoNameLen != 6)
        return;

    const uint8_t *protoName = 0;
    BSB_IMPORT_ptr(bsb, protoName, protoNameLen);
    if (BSB_IS_ERROR(bsb))
        return;

    if (protoNameLen == 4 && memcmp(protoName, "MQTT", 4) != 0)
        return;
    if (protoNameLen == 6 && memcmp(protoName, "MQIsdp", 6) != 0)
        return;

    arkime_session_add_protocol(session, "mqtt");

    ArkimeParserBuf_t *mqtt = arkime_parser_buf_create();
    arkime_parsers_register(session, mqtt_parser, mqtt, arkime_parser_buf_session_free);
}
/******************************************************************************/
void arkime_parser_init()
{
    clientIdField = arkime_field_define("mqtt", "termfield",
                                        "mqtt.clientId", "Client ID", "mqtt.clientId",
                                        "MQTT client identifier",
                                        ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                        (char *)NULL);

    topicField = arkime_field_define("mqtt", "termfield",
                                     "mqtt.topic", "Topic", "mqtt.topic",
                                     "MQTT topic",
                                     ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    versionField = arkime_field_define("mqtt", "termfield",
                                       "mqtt.version", "Version", "mqtt.version",
                                       "MQTT protocol version",
                                       ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                       (char *)NULL);

    typeField = arkime_field_define("mqtt", "termfield",
                                    "mqtt.type", "Type", "mqtt.type",
                                    "MQTT packet type",
                                    ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                    (char *)NULL);

    willTopicField = arkime_field_define("mqtt", "termfield",
                                         "mqtt.willTopic", "Will Topic", "mqtt.willTopic",
                                         "MQTT last will topic",
                                         ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                         (char *)NULL);

    qosField = arkime_field_define("mqtt", "integer",
                                   "mqtt.qos", "QoS", "mqtt.qos",
                                   "MQTT Quality of Service level",
                                   ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                   (char *)NULL);

    flagsField = arkime_field_define("mqtt", "termfield",
                                     "mqtt.flags", "Flags", "mqtt.flags",
                                     "MQTT connection flags",
                                     ARKIME_FIELD_TYPE_STR_GHASH, ARKIME_FIELD_FLAG_CNT,
                                     (char *)NULL);

    userField = arkime_field_by_db("user");

    arkime_parsers_classifier_register_tcp("mqtt", NULL, 0, (uint8_t *)"\x10", 1, mqtt_classify);
}
