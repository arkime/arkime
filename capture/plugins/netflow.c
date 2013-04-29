/* netflow.c  -- Simple plugin that sends netflow data
 *     
 * Copyright 2012-2013 AOL Inc. All rights reserved.
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
#include "moloch.h"
#include "bsb.h"
#include "nids.h"

extern struct timeval initialPacket;


typedef struct {
    int                 fd;
    struct sockaddr_in  addr;
    uint32_t            seq;
} NetflowDest_t;

static int           netflowVersion;
static int           netflowSNMPInput = 0;
static int           netflowSNMPOutput = 0;
static int           numDests = 0;
static NetflowDest_t dests[100];
static int           headerSize;

/******************************************************************************/

extern MolochConfig_t        config;

struct timeval        bufTime;
static char           buf[1500];
static BSB            bsb;
static int            bufCount = -1;
static uint32_t       totalFlows = 0;

/******************************************************************************/
void netflow_send()
{
    BSB hbsb;

    BSB_INIT(hbsb, buf, headerSize);

    uint32_t sys_uptime = (bufTime.tv_sec - initialPacket.tv_sec)*1000; /*+
                          (bufTIme.tv_usec - initialPacket.tv_usec)/1000;*/
                 

    /* Header */
    BSB_EXPORT_u16(hbsb, netflowVersion);
    BSB_EXPORT_u16(hbsb, bufCount); // count
    BSB_EXPORT_u32(hbsb, sys_uptime); // sys_uptime
    BSB_EXPORT_u32(hbsb, bufTime.tv_sec);
    BSB_EXPORT_u32(hbsb, bufTime.tv_usec);

    switch (netflowVersion) {
    case 5:
        BSB_EXPORT_u32(hbsb, totalFlows); // flow_sequence
        BSB_EXPORT_u08(hbsb, 0); // engine_type
        BSB_EXPORT_u08(hbsb, 0); // engine_id
        BSB_EXPORT_u16(hbsb, 0); // mode/interval
        break;
    case 7:
        BSB_EXPORT_u32(hbsb, totalFlows); // flow_sequence
        BSB_EXPORT_u32(hbsb, 0); // reserved
        break;
    }

    int i;
    for (i = 0; i < numDests; i++) {
        int rc;
        
        if ((rc = send(dests[i].fd, buf, BSB_LENGTH(bsb)+headerSize, 0)) < BSB_LENGTH(bsb)+headerSize) {
            LOG("Failed to send rc=%d size=%ld", rc, BSB_LENGTH(bsb)+headerSize);
        }
    }

    totalFlows += bufCount;
    BSB_INIT(bsb, buf + headerSize, sizeof(buf) - headerSize);
    bufCount = 0;
}
/******************************************************************************/
/* 
 * Called by moloch when a session is about to be saved
 */
void netflow_plugin_save(MolochSession_t *session, int UNUSED(final))
{
    static char zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    if (bufCount == -1) {
        bufCount = 0;
        BSB_INIT(bsb, buf + headerSize, sizeof(buf) - headerSize);
    } else if (bufCount > 20) {
        netflow_send();
    }

    if (bufCount == 0)
        bufTime = session->lastPacket;


    /* Body */
    BSB_EXPORT_ptr(bsb, &session->addr1, 4);
    BSB_EXPORT_ptr(bsb, &session->addr2, 4);
    BSB_EXPORT_u32(bsb, 0); // nexthop
    BSB_EXPORT_u16(bsb, netflowSNMPInput); // snmp input
    BSB_EXPORT_u16(bsb, netflowSNMPOutput); // snmp output
    BSB_EXPORT_u32(bsb, session->packets); 
    BSB_EXPORT_u32(bsb, session->databytes);
    uint32_t first = (session->firstPacket.tv_sec - initialPacket.tv_sec)*1000;
    uint32_t last = (session->lastPacket.tv_sec - initialPacket.tv_sec)*1000;
    BSB_EXPORT_u32(bsb, first);
    BSB_EXPORT_u32(bsb, last);
    BSB_EXPORT_u16(bsb, session->port1);
    BSB_EXPORT_u16(bsb, session->port2);

    switch (netflowVersion) {
    case 1:
        BSB_EXPORT_u08(bsb, 0); // pad
        BSB_EXPORT_u08(bsb, session->protocol);
        BSB_EXPORT_u08(bsb, session->ip_tos); // tos
        BSB_EXPORT_u08(bsb, session->tcp_flags); // tcp_flags
        BSB_EXPORT_ptr(bsb, zero, 8); //pad
        break;
    case 5:
        BSB_EXPORT_u08(bsb, 0); // pad
        BSB_EXPORT_u08(bsb, session->tcp_flags); // tcp_flags
        BSB_EXPORT_u08(bsb, session->protocol);
        BSB_EXPORT_u08(bsb, session->ip_tos); // tos
        BSB_EXPORT_u16(bsb, 0); // src as
        BSB_EXPORT_u16(bsb, 0); // dst as
        BSB_EXPORT_u08(bsb, 0); // src_mask
        BSB_EXPORT_u08(bsb, 0); // dst_mask
        BSB_EXPORT_ptr(bsb, zero, 2); //pad
        break;
    case 7:
        BSB_EXPORT_u08(bsb, 0); // pad
        BSB_EXPORT_u08(bsb, session->tcp_flags);
        BSB_EXPORT_u08(bsb, session->protocol);
        BSB_EXPORT_u08(bsb, session->ip_tos);
        BSB_EXPORT_u16(bsb, 0); // src as
        BSB_EXPORT_u16(bsb, 0); // dst as
        BSB_EXPORT_u08(bsb, 0); // src_mask
        BSB_EXPORT_u08(bsb, 0); // dst_mask
        BSB_EXPORT_u16(bsb, 0); // flags
        BSB_EXPORT_u32(bsb, 0); // router_sc
        break;
    }
    bufCount++;
}

/******************************************************************************/
/* 
 * Called by moloch when moloch is quiting
 */
void netflow_plugin_exit()
{
    netflow_send();
}

/******************************************************************************/
/*
 * Called by moloch when the plugin is loaded
 */
void moloch_plugin_init()
{
    moloch_plugins_register("netflow", FALSE);

    moloch_plugins_set_cb("netflow",
      NULL,
      NULL,
      NULL,
      netflow_plugin_save,
      NULL,
      netflow_plugin_exit,
      NULL
    );

    netflowSNMPInput = moloch_config_int(NULL, "netflowSNMPInput", 0, 0, 0xffffffff);
    netflowSNMPOutput = moloch_config_int(NULL, "netflowSNMPOutput", 0, 0, 0xffffffff);
    netflowVersion = moloch_config_int(NULL, "netflowVersion", 5, 1, 7);
    LOG("version = %d", netflowVersion);
    if (netflowVersion != 1 && netflowVersion != 5 && netflowVersion != 7) {
        LOG("Unsupported netflowVersion: %d", netflowVersion);
        exit(0);
    }

    if (netflowVersion == 1)
        headerSize = 16;
    else 
        headerSize = 24;

    char **dsts = moloch_config_str_list(NULL, "netflowDestinations", NULL);
    if (dsts == NULL || dsts[0] == NULL || dsts[0][0] == 0){
        LOG("Unsupport netflowDestinations must be set");
        exit(0);
    }

    int i;
    for (i = 0; i < 100 && dsts[i]; i++) {
        char *colon = strchr(dsts[i], ':');
        if (!colon) {
            LOG("netflowDestination (%s) needs a destination port", dsts[i]);
            exit(0);
        }
        *colon = 0;

        struct addrinfo hints, *res;
	memset(&hints, 0 , sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

        if (getaddrinfo(dsts[i], colon+1, &hints, &res)) {
            LOG("Failed looking up %s:%s", dsts[i], colon+1);
            exit(0);
        }

        dests[numDests].addr = *((struct sockaddr_in *) res->ai_addr);
        dests[numDests].seq = 0;

        if ((dests[numDests].fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
            LOG("Socket failed: %s", strerror(errno));
            exit(1);
        }

        if (connect(dests[numDests].fd, (struct sockaddr *) &dests[numDests].addr, sizeof(struct sockaddr_in))) {
            LOG("Connect failed: %s", strerror(errno));
            exit(1);
        }
        numDests++;
        freeaddrinfo(res);
    }
    g_strfreev(dsts);
}
