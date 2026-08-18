/******************************************************************************/
/* mprotocol.c  -- Handle mProtocol registration and unknown/corrupt packets
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"

/******************************************************************************/
extern ArkimeConfig_t        config;

int                          mProtocolCnt = ARKIME_MPROTOCOL_MIN;
ArkimeProtocol_t             mProtocols[ARKIME_MPROTOCOL_MAX];
LOCAL GHashTable            *mProtocolHash;

LOCAL uint32_t defaultMaxPackets;
LOCAL uint32_t maxStreams;

LOCAL void arkime_mprotocol_config_one(int mProtocol);

LOCAL int mProtocolCorruptEther;
LOCAL int mProtocolCorruptIp;
LOCAL int mProtocolUnknownEther;
LOCAL int mProtocolUnknownIp;

/******************************************************************************/
int arkime_mprotocol_register_internal(const char                      *name,
                                       uint32_t                         flags,
                                       ArkimeProtocolCreateSessionId_cb createSessionId,
                                       ArkimeProtocolPreProcess_cb      preProcess,
                                       ArkimeProtocolProcess_cb         process,
                                       ArkimeProtocolSessionFree_cb     sFree,
                                       ArkimeProtocolSessionMidSave_cb  midSave,
                                       int                              sessionTimeout,
                                       size_t                           sessionsize,
                                       int                              apiversion)
{
    static ARKIME_LOCK_DEFINE(lock);

    if (sizeof(ArkimeSession_t) != sessionsize) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %u != %u", name, (unsigned int)sizeof(ArkimeSession_t), (unsigned int)sessionsize);
    }

    if (ARKIME_API_VERSION != apiversion) {
        CONFIGEXIT("Parser '%s' built with different version of arkime.h\n %d %d", name, ARKIME_API_VERSION, apiversion);
    }

    ARKIME_LOCK(lock);

    int n = GPOINTER_TO_INT(g_hash_table_lookup(mProtocolHash, name));
    if (n > 0) {
        ARKIME_UNLOCK(lock);
        return n;
    }

    if (mProtocolCnt >= ARKIME_MPROTOCOL_MAX) {
        CONFIGEXIT("Too many protocols registered (max %d)", ARKIME_MPROTOCOL_MAX);
    }
    int num = mProtocolCnt++;
    mProtocols[num].name = name;
    mProtocols[num].flags = flags;
    mProtocols[num].createSessionId = createSessionId;
    mProtocols[num].preProcess = preProcess;
    mProtocols[num].process = process;
    mProtocols[num].sFree = sFree;
    mProtocols[num].midSave = midSave;
    mProtocols[num].sessionTimeout = sessionTimeout;
    mProtocols[num].saveTimeout = ARKIME_DEFAULT_SAVE_TIMEOUT;
    mProtocols[num].closingTimeout = ARKIME_DEFAULT_CLOSING_TIMEOUT;
    mProtocols[num].maxPackets = defaultMaxPackets;

    if (flags & ARKIME_MPROTOCOL_FLAG_STREAMS_HIGH)
        mProtocols[num].maxStreams = MAX(64, maxStreams / config.packetThreads * 1.25);
    else if (flags & ARKIME_MPROTOCOL_FLAG_STREAMS_LOW)
        mProtocols[num].maxStreams = MAX(64, maxStreams / config.packetThreads / 200);
    else
        mProtocols[num].maxStreams = MAX(64, maxStreams / config.packetThreads / 20);

    g_hash_table_insert(mProtocolHash, g_strdup(name), GINT_TO_POINTER(num));

    ARKIME_UNLOCK(lock);

    arkime_mprotocol_config_one(num);
    arkime_session_mprotocol_init(num);

    return num;
}
/******************************************************************************/
int arkime_mprotocol_get(const char *name)
{
    return GPOINTER_TO_INT(g_hash_table_lookup(mProtocolHash, name));
}
/******************************************************************************/
/* Used by mProtocols that have their own legacy settings, like tcpSaveTimeout.
 * Anything negative is left alone. The [protocol-settings] section is applied
 * after this and wins.
 */
void arkime_mprotocol_set_timeouts(int mProtocol, int saveTimeout, int closingTimeout)
{
    if (mProtocol < ARKIME_MPROTOCOL_MIN || mProtocol >= mProtocolCnt)
        LOGEXIT("ERROR - Unknown mProtocol %d", mProtocol);

    if (saveTimeout >= 0)
        mProtocols[mProtocol].saveTimeout = saveTimeout;

    if (closingTimeout >= 0)
        mProtocols[mProtocol].closingTimeout = closingTimeout;

    // These are only defaults, [protocol-settings] still wins
    arkime_mprotocol_config_one(mProtocol);
}
/******************************************************************************/
LOCAL int arkime_mprotocol_config_num(const char *key, const char *name, const char *str, int min, int max)
{
    char *end;
    errno = 0;
    long num = strtol(str, &end, 10);

    while (isspace(*end))
        end++;

    if (errno != 0 || end == str || *end != 0)
        CONFIGEXIT("'%s' isn't a number for '%s:' of '%s' in section [protocol-settings]", str, name, key);

    if (num < min || num > max)
        CONFIGEXIT("%ld must be between %d and %d for '%s:' of '%s' in section [protocol-settings]", num, min, max, name, key);

    return num;
}
/******************************************************************************/
/* Apply one "name:value;name:value" string to a single mProtocol */
LOCAL void arkime_mprotocol_config_apply(int mProtocol, const char *key, const char *value)
{
    if (!value)
        CONFIGEXIT("Invalid value for '%s' in section [protocol-settings]", key);

    char **settings = g_strsplit(value, ";", 0);

    for (int i = 0; settings[i]; i++) {
        char *setting = g_strstrip(settings[i]);

        if (!*setting)
            continue;

        char *colon = strchr(setting, ':');
        if (!colon)
            CONFIGEXIT("'%s' must be name:value for '%s' in section [protocol-settings]", setting, key);
        *colon = 0;
        g_strchomp(setting);

        const char *num = g_strchug(colon + 1);

        if (strcmp(setting, "idle") == 0) {
            mProtocols[mProtocol].sessionTimeout = arkime_mprotocol_config_num(key, setting, num, 1, 0xffff);
        } else if (strcmp(setting, "save") == 0) {
            int save = arkime_mprotocol_config_num(key, setting, num, 0, 60 * 120);
            if (save != 0 && save < 10)
                CONFIGEXIT("%d must be 0 or between 10 and %d for 'save:' of '%s' in section [protocol-settings]", save, 60 * 120, key);
            mProtocols[mProtocol].saveTimeout = save;
        } else if (strcmp(setting, "closing") == 0) {
            mProtocols[mProtocol].closingTimeout = arkime_mprotocol_config_num(key, setting, num, 1, 255);
        } else if (strcmp(setting, "packets") == 0) {
            mProtocols[mProtocol].maxPackets = arkime_mprotocol_config_num(key, setting, num, 1, 0xffff);
        } else if (strcmp(setting, "streams") == 0) {
            // The setting is the total across all packet threads
            mProtocols[mProtocol].maxStreams = MAX(64, arkime_mprotocol_config_num(key, setting, num, 64, 16777215) / config.packetThreads);
        } else {
            CONFIGEXIT("Unknown setting '%s' for '%s' in section [protocol-settings], must be idle, save, closing, packets or streams", setting, key);
        }
    }
    g_strfreev(settings);
}
/******************************************************************************/
/* Apply the [protocol-settings] section to a single mProtocol. default first so
 * the mProtocol specific key wins
 */
LOCAL void arkime_mprotocol_config_one(int mProtocol)
{
    char *value = arkime_config_section_str(NULL, "protocol-settings", "default", NULL);
    if (value) {
        arkime_mprotocol_config_apply(mProtocol, "default", value);
        g_free(value);
    }

    value = arkime_config_section_str(NULL, "protocol-settings", mProtocols[mProtocol].name, NULL);
    if (value) {
        arkime_mprotocol_config_apply(mProtocol, mProtocols[mProtocol].name, value);
        g_free(value);
    }
}
/******************************************************************************/
/* Load the [protocol-settings] section, which overrides the values that each
 * mProtocol registered with (and therefore the old tcpTimeout/tcpSaveTimeout/
 * tcpClosingTimeout/maxPackets style settings).  Must be called after all
 * mProtocols have registered.
 *
 * [protocol-settings]
 * default=save:480;packets:10000
 * tcp=idle:600;closing:5
 * udp=idle:30
 */
void arkime_mprotocol_config()
{
    gsize keys_len;
    gchar **keys = arkime_config_section_keys(NULL, "protocol-settings", &keys_len);

    // Each mProtocol applied its own settings when it registered, all that is left
    // is telling the user about keys that no mProtocol ever claimed
    for (int i = 0; keys && i < (int)keys_len; i++) {
        if (strcmp(keys[i], "default") == 0)
            continue;

        if (arkime_mprotocol_get(keys[i]) == 0)
            LOG("WARNING - Ignoring '%s' in section [protocol-settings], no protocol with that name is loaded", keys[i]);
    }
    g_strfreev(keys);

    if (config.debug) {
        for (int m = ARKIME_MPROTOCOL_MIN; m < mProtocolCnt; m++) {
            LOG("%s idle:%d save:%d closing:%d packets:%u streams:%u",
                mProtocols[m].name, mProtocols[m].sessionTimeout, mProtocols[m].saveTimeout,
                mProtocols[m].closingTimeout, mProtocols[m].maxPackets, mProtocols[m].maxStreams);
        }
    }
}
/******************************************************************************/
// Corrupt Ether packet mProtocol - session ID based on src/dst MAC
LOCAL void corrupt_ether_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    sessionId[0] = 16;
    int avail = (int)packet->pktlen - (int)packet->etherOffset;
    if (avail >= 12) {
        memcpy(sessionId + 1, packet->pkt + packet->etherOffset, 12);
    } else {
        memset(sessionId + 1, 0, 12);
        if (avail > 0)
            memcpy(sessionId + 1, packet->pkt + packet->etherOffset, avail);
    }
    sessionId[13] = sessionId[14] = sessionId[15] = 0;
}
/******************************************************************************/
LOCAL int corrupt_ether_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "corrupt-ether");
    return 0;
}
/******************************************************************************/
LOCAL int corrupt_ether_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
// Corrupt IP packet mProtocol - session ID based on src/dst IP
SUPPRESS_ALIGNMENT
LOCAL void corrupt_ip_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    if (packet->v6) {
        sessionId[0] = 36;
        const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        memcpy(sessionId + 1, &ip6->ip6_src, 16);
        memcpy(sessionId + 17, &ip6->ip6_dst, 16);
        sessionId[33] = sessionId[34] = sessionId[35] = 0;
    } else {
        sessionId[0] = 12;
        const struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        memcpy(sessionId + 1, &ip4->ip_src, 4);
        memcpy(sessionId + 5, &ip4->ip_dst, 4);
        sessionId[9] = sessionId[10] = sessionId[11] = 0;
    }
}
/******************************************************************************/
LOCAL int corrupt_ip_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "corrupt-ip");
    return 0;
}
/******************************************************************************/
LOCAL int corrupt_ip_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC corrupt_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *UNUSED(data), int UNUSED(len))
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    packet->payloadOffset = 0;
    packet->payloadLen = packet->pktlen;

    // Check if we have enough data for IP addresses (IPv6: 40 bytes, IPv4: 20 bytes)
    int ipMinLen = packet->v6 ? 40 : 20;
    if (packet->ipOffset && packet->ipOffset + ipMinLen <= packet->pktlen) {
        corrupt_ip_create_sessionid(sessionId, packet);
        packet->mProtocol = mProtocolCorruptIp;
    } else {
        corrupt_ether_create_sessionid(sessionId, packet);
        packet->mProtocol = mProtocolCorruptEther;
    }

    packet->hash = arkime_session_hash(sessionId);

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
// Unknown Ethernet mProtocol - session ID based on src/dst MAC + ethertype
LOCAL void unknown_ether_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    sessionId[0] = 16;
    // Copy src/dst MACs (12 bytes) + ethertype (2 bytes)
    memcpy(sessionId + 1, packet->pkt + packet->etherOffset, 14);
    sessionId[15] = 0;
}
/******************************************************************************/
LOCAL int unknown_ether_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "unknown-ether");
    return 0;
}
/******************************************************************************/
LOCAL int unknown_ether_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC unknown_ether_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    // Need src/dst MACs (12 bytes) + ethertype (2 bytes) at etherOffset
    if ((int)packet->pktlen - (int)packet->etherOffset < 14)
        return ARKIME_PACKET_CORRUPT;

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    unknown_ether_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = mProtocolUnknownEther;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
// Unknown IP Protocol mProtocol - session ID based on src/dst IP + protocol
SUPPRESS_ALIGNMENT
LOCAL void unknown_ip_create_sessionid(uint8_t *sessionId, ArkimePacket_t *const packet)
{
    if (packet->v6) {
        sessionId[0] = 36;
        const struct ip6_hdr *ip6 = (struct ip6_hdr *)(packet->pkt + packet->ipOffset);
        memcpy(sessionId + 1, &ip6->ip6_src, 16);
        memcpy(sessionId + 17, &ip6->ip6_dst, 16);
        sessionId[33] = packet->ipProtocol;
        sessionId[34] = sessionId[35] = 0;
    } else {
        sessionId[0] = 12;
        const struct ip *ip4 = (struct ip *)(packet->pkt + packet->ipOffset);
        memcpy(sessionId + 1, &ip4->ip_src, 4);
        memcpy(sessionId + 5, &ip4->ip_dst, 4);
        sessionId[9] = packet->ipProtocol;
        sessionId[10] = sessionId[11] = 0;
    }
}
/******************************************************************************/
LOCAL int unknown_ip_pre_process(ArkimeSession_t *session, ArkimePacket_t *const UNUSED(packet), int isNewSession)
{
    if (isNewSession)
        arkime_session_add_protocol(session, "unknown-ip");
    return 0;
}
/******************************************************************************/
LOCAL int unknown_ip_process(ArkimeSession_t *UNUSED(session), ArkimePacket_t *const UNUSED(packet))
{
    return 1;
}
/******************************************************************************/
LOCAL ArkimePacketRC unknown_ip_packet_enqueue(ArkimePacketBatch_t *UNUSED(batch), ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    uint8_t sessionId[ARKIME_SESSIONID_LEN];

    packet->payloadOffset = data - packet->pkt;
    packet->payloadLen = len;

    unknown_ip_create_sessionid(sessionId, packet);

    packet->hash = arkime_session_hash(sessionId);
    packet->mProtocol = mProtocolUnknownIp;

    return ARKIME_PACKET_DO_PROCESS;
}
/******************************************************************************/
void arkime_mprotocol_init()
{
    mProtocolHash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    // Must be set before anything registers since it is the default for all mProtocols
    defaultMaxPackets = arkime_config_int(NULL, "maxPackets", 10000, 1, 0xffff);
    maxStreams = arkime_config_int(NULL, "maxStreams", 1500000, 1, 16777215);

    mProtocolCorruptEther = arkime_mprotocol_register("corrupt-ether",
                                                      0,
                                                      corrupt_ether_create_sessionid,
                                                      corrupt_ether_pre_process,
                                                      corrupt_ether_process,
                                                      NULL,
                                                      NULL,
                                                      60);

    mProtocolCorruptIp = arkime_mprotocol_register("corrupt-ip",
                                                   0,
                                                   corrupt_ip_create_sessionid,
                                                   corrupt_ip_pre_process,
                                                   corrupt_ip_process,
                                                   NULL,
                                                   NULL,
                                                   60);

    mProtocolUnknownEther = arkime_mprotocol_register("unknown-ether",
                                                      0,
                                                      unknown_ether_create_sessionid,
                                                      unknown_ether_pre_process,
                                                      unknown_ether_process,
                                                      NULL,
                                                      NULL,
                                                      60);

    mProtocolUnknownIp = arkime_mprotocol_register("unknown-ip",
                                                   0,
                                                   unknown_ip_create_sessionid,
                                                   unknown_ip_pre_process,
                                                   unknown_ip_process,
                                                   NULL,
                                                   NULL,
                                                   60);

    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_CORRUPT, corrupt_packet_enqueue);
    arkime_packet_set_ethernet_cb(ARKIME_ETHERTYPE_UNKNOWN, unknown_ether_packet_enqueue);
    arkime_packet_set_ip_cb(ARKIME_IPPROTO_UNKNOWN, unknown_ip_packet_enqueue);
}
