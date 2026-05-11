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
LOCAL int connackCodeField;

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
LOCAL void mqtt_parse_connect(ArkimeSession_t *session, ArkimeParserBuf_t *mqtt, BSB *bsb)
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
    mqtt->version = version;

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
// Returns total bytes to skip (header + payload) on success,
// -1 if need more data, -2 if malformed/oversized
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
        return -2; // Malformed

    // If header itself can never fit in the parser buffer, it's not parseable
    if (headerNeeded > (int)mqtt->bufMax)
        return -2;

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

    int truncated = (arkime_parser_buf_add(mqtt, which, data, len) < 0);

    BSB bsb;
    BSB_INIT(bsb, mqtt->buf[which], mqtt->len[which]);

    while (BSB_REMAINING(bsb) >= 2) {
        // Save start of header so we can rewind on need-more-data
        BSB headerStart = bsb;

        // Fixed header
        int packetType = 0;
        BSB_IMPORT_u08(bsb, packetType);

        int flags = packetType & 0x0F;
        packetType = (packetType >> 4) & 0x0F;

        // Remaining length
        uint32_t remainingLen = 0;
        if (mqtt_decode_varint(&bsb, &remainingLen) < 0) {
            // Either malformed or need-more bytes for the varint. If there are
            // already 4 length bytes available, it's malformed; otherwise it
            // is incomplete and we should wait for more data.
            int varintBytes = BSB_WORK_PTR(bsb) - (BSB_WORK_PTR(headerStart) + 1);
            if (varintBytes >= 4) {
                arkime_session_add_tag(session, "mqtt:bad-varint");
                arkime_parsers_unregister(session, mqtt);
                return 0;
            }
            bsb = headerStart;
            break;
        }

        // Add packet type
        if (packetType < (int)ARRAY_LEN(mqttTypes)) {
            arkime_field_string_add(typeField, session, mqttTypes[packetType], -1, TRUE);
        }

        // Handle PUBLISH specially first - can skip large payloads even when
        // they exceed the parser buffer capacity.
        if (packetType == 3) {
            int skipLen = mqtt_parse_publish(session, mqtt, which, &bsb, flags, remainingLen);
            if (skipLen == -1) {
                // Need more data; rewind so we re-parse the fixed header next time.
                bsb = headerStart;
                break;
            }
            if (skipLen < 0) {
                arkime_session_add_tag(session, "mqtt:bad-publish");
                arkime_parsers_unregister(session, mqtt);
                return 0;
            }

            arkime_parser_buf_skip(mqtt, which, skipLen);
            BSB_INIT(bsb, mqtt->buf[which], mqtt->len[which]);
            continue;
        }

        // For non-PUBLISH packets we must buffer the entire packet. If the
        // declared length cannot fit in the parser buffer we would wedge
        // forever, so tag and unregister.
        if (remainingLen > (uint32_t)mqtt->bufMax) {
            arkime_session_add_tag(session, "mqtt:message-too-long");
            arkime_parsers_unregister(session, mqtt);
            return 0;
        }

        if (remainingLen > BSB_REMAINING(bsb)) {
            // Not enough buffered yet. If add() truncated this read it can
            // never complete, so unregister; otherwise wait for more data.
            if (truncated) {
                arkime_session_add_tag(session, "mqtt:message-too-long");
                arkime_parsers_unregister(session, mqtt);
                return 0;
            }
            bsb = headerStart;
            break;
        }

        // Parse based on packet type
        BSB packetBsb;
        BSB_IMPORT_bsb(bsb, packetBsb, remainingLen);

        switch (packetType) {
        case 1: // CONNECT
            mqtt_parse_connect(session, mqtt, &packetBsb);
            break;
        case 2: // CONNACK
            if (BSB_REMAINING(packetBsb) >= 2) {
                BSB_IMPORT_skip(packetBsb, 1); // ack flags
                uint8_t code = 0;
                BSB_IMPORT_u08(packetBsb, code);
                if (BSB_NOT_ERROR(packetBsb)) {
                    arkime_field_int_add(connackCodeField, session, code);
                }
            }
            break;
        case 8: // SUBSCRIBE
            mqtt_parse_subscribe(session, &packetBsb, mqtt->version);
            break;
        case 10: // UNSUBSCRIBE
            mqtt_parse_subscribe(session, &packetBsb, mqtt->version);
            break;
        }

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

    connackCodeField = arkime_field_define("mqtt", "integer",
                                           "mqtt.connackCode", "CONNACK Code", "mqtt.connackCode",
                                           "MQTT CONNACK return/reason code (0=Accepted; non-zero=rejected/error)",
                                           ARKIME_FIELD_TYPE_INT_GHASH, ARKIME_FIELD_FLAG_CNT,
                                           (char *)NULL);

    userField = arkime_field_by_db("user");

    arkime_parsers_classifier_register_tcp("mqtt", NULL, 0, (uint8_t *)"\x10", 1, mqtt_classify);
}
