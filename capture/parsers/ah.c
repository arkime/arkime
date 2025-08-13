/* ah.c
 *
 * Copyright 2019 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include "patricia.h"
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>


/******************************************************************************/
extern ArkimeConfig_t        config;

/******************************************************************************/
SUPPRESS_ALIGNMENT
LOCAL ArkimePacketRC ah_packet_enqueue(ArkimePacketBatch_t *batch, ArkimePacket_t *const packet, const uint8_t *data, int len)
{
    int hlen = (data[1] + 2) * 4;
    if (hlen > len)
        return ARKIME_PACKET_CORRUPT;

    return arkime_packet_run_ip_cb(batch, packet, data + hlen, len - hlen, data[0], "ah");
}
/******************************************************************************/
void arkime_parser_init()
{
    arkime_packet_set_ip_cb(IPPROTO_AH, ah_packet_enqueue);
}
