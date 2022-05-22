/******************************************************************************/
/* agent.c  -- Agent stuff
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
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
#include <stdarg.h>
#include <arpa/inet.h>
#include <errno.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include "moloch.h"
#include "patricia.h"

extern MolochConfig_t        config;

LOCAL char *agentEndpoint;
LOCAL void *agentServer;

LOCAL char *awsMetaEndpoint;
LOCAL void *awsMetaServer;

LOCAL  int s3UseTokenForMetadata = 0;

char       constantItems[10000];
int        constantItemsLen;

/******************************************************************************/
// Packet forwarding
LOCAL  char  vxlanHeader[8];
LOCAL  int   packetServerFd;
LOCAL  struct sockaddr_in packetServerAddr;
/******************************************************************************/
// My ips
LOCAL patricia_tree_t *myIp4;
LOCAL patricia_tree_t *myIp6;

LOCAL char sharedInfo[10000];
LOCAL int  sharedInfoLen;
/******************************************************************************/
// Traffic match rules
#define AGENT_RULE_MAX       100
#define AGENT_RULE_MAX_PORTS 12
typedef struct {
    char            *name;
    patricia_tree_t *ipTree4;
    patricia_tree_t *ipTree6;
    int32_t          maxPackets;
    uint16_t         ports[AGENT_RULE_MAX_PORTS]; // Could be a hash
    uint8_t          portsCnt;
    uint8_t          action;
} AgentSendRule_t;

AgentSendRule_t agentRules[AGENT_RULE_MAX];
int             agentRulesCnt;

/******************************************************************************/
void arkime_print_tree(prefix_t *prefix, void *UNUSED(data)){
    char buf[100];
    prefix_toa2x(prefix, buf, 1);
    printf("%s\n", buf);
}
/******************************************************************************/
unsigned char *arkime_agent_get_aws_metadata(char *key, int key_len, size_t *rlen)
{
    char *requestHeaders[2];
        
    if (s3UseTokenForMetadata) {
        unsigned char *token;
        char tokenHeader[200];
        char *tokenRequestHeaders[2];
        tokenRequestHeaders[0] = "X-aws-ec2-metadata-token-ttl-seconds: 30";
        tokenRequestHeaders[1] = NULL;
        if (config.debug)
            LOG("Requesting IMDSv2 metadata token");
        token = moloch_http_send_sync(awsMetaServer, "PUT", "/latest/api/token", -1, NULL, 0, tokenRequestHeaders, rlen);
        if (config.debug)
            LOG("IMDSv2 metadata token received");
        snprintf(tokenHeader, sizeof(tokenHeader), "X-aws-ec2-metadata-token: %s", token);
        requestHeaders[0] = tokenHeader;
        requestHeaders[1] = NULL;
    } else {
        if (config.debug)
            LOG("Using IMDSv1");
        requestHeaders[0] = NULL;
    }
    return moloch_http_send_sync(awsMetaServer, "GET", key, key_len, NULL, 0, requestHeaders, rlen);
}
/******************************************************************************/
void arkime_agent_print_rules()
{
    int i, p;
    for (i = 0; i < agentRulesCnt; i++) {
        AgentSendRule_t *asr = &agentRules[i];
        printf("Rule %s %x\n", asr->name, asr->action);
        if (asr->portsCnt > 0) {
            printf("  Ports:");
            for (p = 0; p < asr->portsCnt; p++) {
                printf(" %d", asr->ports[p]);
            }
            printf("\n");
        }

        if (asr->ipTree4) {
            printf ("  4:\n");
            patricia_process(asr->ipTree4, arkime_print_tree);
        }

        if (asr->ipTree6) {
            printf ("  6:\n");
            patricia_process(asr->ipTree6, arkime_print_tree);
        }
    }
}
/******************************************************************************/
void arkime_agent_config_update()
{
    static char *headers[2] = {"Content-Type: application/json", NULL};
    char    json[10000];
    BSB     bsb;

    BSB_INIT(bsb, json, sizeof(json));
    BSB_EXPORT_u08(bsb, '{');
    BSB_EXPORT_ptr(bsb, sharedInfo, sharedInfoLen);
    BSB_EXPORT_u08(bsb, '}');

    size_t rlen;
    unsigned char *data = moloch_http_send_sync(agentServer, "POST", "/config", -1, json, BSB_LENGTH(bsb), headers, &rlen);
    if (config.debug)
        LOG("config = %s", data);

    uint32_t           rules_len;
    unsigned char     *rules = 0;
    rules = moloch_js0n_get(data, rlen, "rules", &rules_len);
    if (!rules)
        LOGEXIT("rules section missing");

    uint32_t out[2*800];
    memset(out, 0, sizeof(out));
    js0n(rules, rules_len, out, sizeof(out));
    int i;
    for (i = 0; out[i]; i+= 2) {
        AgentSendRule_t *asr = &agentRules[agentRulesCnt++];

        // NAME
        asr->name = moloch_js0n_get_str(rules + out[i], out[i+1], "name"); // Don't free
        if (!asr->name)
            LOGEXIT("Must be name set: %.*s", out[i+1], rules + out[i]);
        
        // ACTION
        char *action = moloch_js0n_get_str(rules + out[i], out[i+1], "action");
        if (!action)
            LOGEXIT("Must be action set: %.*s", out[i+1], rules + out[i]);

        if (strcmp(action, "telemetry") == 0)
            asr->action = AGENT_SEND_TELEMETRY;
        else if (strcmp(action, "packets") == 0)
            asr->action = AGENT_SEND_PACKETS;
        else if (strcmp(action, "both") == 0)
            asr->action = AGENT_SEND_BOTH;
        else if (strcmp(action, "none") == 0)
            asr->action = 0;
        else
            LOGEXIT("Unknown action: %s", action);
        free(action);

        // PORTS
        char *ports = moloch_js0n_get_str(rules + out[i], out[i+1], "ports");
        if (ports) {
            ports[strlen(ports)-1] = 0; // Remove ]
            gchar **portsArray = g_strsplit(ports+1, ",", -1);
            for (int p = 0; p < 10 && portsArray[p]; p++) {
                asr->ports[asr->portsCnt++] = atoi(portsArray[p]);
            }
            g_strfreev(portsArray);
        }
        if (ports)
            free(ports);

        // CIDRS
        char *cidrs = moloch_js0n_get_str(rules + out[i], out[i+1], "cidrs");
        if (cidrs) {
            cidrs[strlen(cidrs)-1] = 0; // Remove ]
            asr->ipTree4 = New_Patricia(32);
            asr->ipTree6 = New_Patricia(128);
            gchar **cidrsArray = g_strsplit(cidrs+1, ",", -1);
            for (int c = 0; c < 10 && cidrsArray[c]; c++) {
                cidrsArray[c][strlen(cidrsArray[c]) - 1] = 0; // Remove trailing "
                if (strchr(cidrsArray[c] + 1, '.') != 0)
                    make_and_lookup(asr->ipTree4, cidrsArray[c] + 1);
                else
                    make_and_lookup(asr->ipTree6, cidrsArray[c] + 1);
            }
            g_strfreev(cidrsArray);
            free(cidrs);
        }

        // MAX PACKETS
        char *maxPackets = moloch_js0n_get_str(rules + out[i], out[i+1], "maxPackets");
        if (maxPackets) {
            asr->maxPackets = atoi(maxPackets);
            free(maxPackets);
        } else {
            asr->maxPackets = -1;
        }
    }

    // VXLANID
    char *vxlanIdStr = moloch_js0n_get_str(data, rlen, "vxlanId");
    if (!vxlanIdStr)
        LOGEXIT("No vxlanId section");
    int vxlanId = atoi(vxlanIdStr);
    free(vxlanIdStr);
    vxlanHeader[0] = 0x8; // VXLAN Id Is Valid
    vxlanHeader[4] = (vxlanId >> 16) & 0xff;
    vxlanHeader[5] = (vxlanId >> 8) & 0xff;
    vxlanHeader[6] = vxlanId & 0xff;

    // PACKETENDPOINT
    char *packetEndpointStr = moloch_js0n_get_str(data, rlen, "packetEndpoint"); // Don't free
    if (!packetEndpointStr)
        LOGEXIT("No packetEndpoint section");

    if ( (packetServerFd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        LOGEXIT("Failed udp creation: %d", errno);
    }

    // SOURCE
    char *source = moloch_js0n_get_str(data, rlen, "source");
    if (source) {
        if (strchr(source, '.') != 0)
            make_and_lookup(myIp4, source);
        else
            make_and_lookup(myIp6, source);
        free(source);
    }

    memset(&packetServerAddr, 0, sizeof(packetServerAddr));
    packetServerAddr.sin_family = AF_INET;
    packetServerAddr.sin_port = htons(4789);
    packetServerAddr.sin_addr.s_addr = INADDR_ANY;

    patricia_process(myIp4, arkime_print_tree);
    patricia_process(myIp6, arkime_print_tree);
    arkime_agent_print_rules();
}
/******************************************************************************/
void arkime_agent_get_my_ips()
{
    struct ifaddrs *ifas, *ifa;
    
    getifaddrs(&ifas);
    if (!ifas)
        LOGEXIT("Coudln't get ifaddrs %d", errno);

    for (ifa = ifas; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;

        int family = ifa->ifa_addr->sa_family;

        if (family != AF_INET && family != AF_INET6)
            continue;

        prefix_t       *prefix;
        if (family == AF_INET) {
            prefix = New_Prefix2(AF_INET, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr, 32, NULL);
            patricia_lookup(myIp4, prefix);
            Deref_Prefix(prefix);
        } else {
            prefix = New_Prefix2(AF_INET6, &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr.s6_addr, 128, NULL);
            patricia_lookup(myIp6, prefix);
            Deref_Prefix(prefix);
        }
    }

    freeifaddrs(ifas);
}
/******************************************************************************/
void arkime_agent_session_new(MolochSession_t *session)
{
    if (!session->ipProtocol)
        return;

    uint32_t checkIp = 0;
    uint32_t checkPort = 0;
    struct in6_addr *checkIp6;

    if (IN6_IS_ADDR_V4MAPPED(&session->addr1)) {
        uint32_t ip1 = MOLOCH_V6_TO_V4(session->addr1);
        uint32_t ip2 = MOLOCH_V6_TO_V4(session->addr2);

        int isIp1 = patricia_search_best3(myIp4, (unsigned char *)&ip1, 32) != NULL;
        int isIp2 = patricia_search_best3(myIp4, (unsigned char *)&ip2, 32) != NULL;

        if (isIp1 && isIp2) {
            char buf[200];
            LOG("Dropping local traffic: %s", moloch_session_pretty_string (session, buf, sizeof(buf)));
            return;
        }

        if (isIp1) {
            checkIp = ip2;
            checkPort = session->port2;
        } else {
            checkIp = ip1;
            checkPort = session->port1;
        }
    } else {
        int isIp1 = patricia_search_best3(myIp6, (unsigned char *)&session->addr1, 128) != NULL;
        int isIp2 = patricia_search_best3(myIp6, (unsigned char *)&session->addr2, 128) != NULL;

        if (isIp1 && isIp2) {
            char buf[200];
            LOG("Dropping local traffic: %s", moloch_session_pretty_string (session, buf, sizeof(buf)));
            return;
        }

        if (isIp1) {
            checkIp6 = &session->addr2;
            checkPort = session->port2;
        } else {
            checkIp6 = &session->addr1;
            checkPort = session->port1;
        }
    }

    char buf[200];

    int found = 0;
    AgentSendRule_t *asr;
    for (int r = 0; r < agentRulesCnt; r++) {
        asr = &agentRules[r];

        int portMatch = asr->portsCnt == 0;
        if (asr->portsCnt > 0) {
            for (int p = 0; p < asr->portsCnt; p++) {
                if (checkPort == asr->ports[p]) {
                    portMatch = 1;
                    break;
                }
            }
        }

        if (!portMatch)
            continue;

        if (!asr->ipTree4) {
            found = 1;
            break;
        }

        if (checkIp6) { 
            if (patricia_search_best3(asr->ipTree6, (unsigned char*)checkIp6, 128)) {
                found = 1;
                break;
            }
        } else {
            if (patricia_search_best3(asr->ipTree4, (unsigned char *)&checkIp, 32)) {
                found = 1;
                break;
            }
        }
    }

    if (found) {
        session->agentAction = asr->action;
        if (asr->maxPackets != -1)
            session->stopSaving = asr->maxPackets;
    }
}
/******************************************************************************/
LOCAL void agent_writer_write(const MolochSession_t *const UNUSED(session), MolochPacket_t * const UNUSED(packet))
{
    if (((long)session->agentAction & AGENT_SEND_PACKETS) == 0)
        return;

    char buf[1000];
    //LOG("packet %p %p %s", packet, session, moloch_session_pretty_string (session, buf, sizeof(buf)));

    struct iovec msg_iov[2];
    msg_iov[0].iov_base = vxlanHeader;
    msg_iov[0].iov_len = sizeof(vxlanHeader);
    msg_iov[1].iov_base = packet->pkt;
    msg_iov[1].iov_len = packet->pktlen;

    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_name = &packetServerAddr;
    msg.msg_namelen = sizeof(packetServerAddr);
    msg.msg_iov = msg_iov;
    msg.msg_iovlen = 2;
    int rc = sendmsg(packetServerFd, &msg, 0);
    if (rc < 0)
        perror("sad");
}
/******************************************************************************/
LOCAL void agent_writer_exit()
{
}
/******************************************************************************/
LOCAL uint32_t agent_writer_queue_length()
{
    return 0;
}
/******************************************************************************/
LOCAL void agent_writer_init(char *UNUSED(name))
{
    moloch_writer_queue_length = agent_writer_queue_length;
    moloch_writer_exit         = agent_writer_exit;
    moloch_writer_write        = agent_writer_write;
}
/******************************************************************************/
int arkime_agent_can_quit()
{
    if (moloch_http_queue_length(agentServer) > 0) {
        if (config.debug)
            LOG ("Can't quit, moloch_http_queue_length(agentServer) %d", moloch_http_queue_length(agentServer));
        return 1;
    }

    return 0;
}
/******************************************************************************/
LOCAL void arkime_agent_send_telemetry_cb(int code, unsigned char *data, int data_len, gpointer UNUSED(uw))
{
    if (code != 200)
        LOG("Bulk issue.  Code: %d\n%.*s", code, data_len, data);
    else if (config.debug > 4)
        LOG("Bulk Reply code:%d :>%.*s<", code, data_len, data);
}
/******************************************************************************/
LOCAL void arkime_agent_send_telemetry(char *json, int len)
{
    if (config.debug > 1)
        LOG("Sending Bulk:>%.*s<", len, json);
    moloch_http_schedule(agentServer, "POST", "/telemetry", 10, json, len, NULL, MOLOCH_HTTP_PRIORITY_NORMAL, arkime_agent_send_telemetry_cb, NULL);
}
/******************************************************************************/
void arkime_agent_init()
{
    myIp4 = New_Patricia(32);
    myIp6 = New_Patricia(128);

    arkime_agent_get_my_ips();

    // Get AWS Meta
    awsMetaEndpoint = moloch_config_str(NULL, "awsMetaEndpoint", "http://169.254.169.254");
    awsMetaServer = moloch_http_create_server(awsMetaEndpoint, 2, 2, 0);
    moloch_http_set_print_errors(awsMetaServer);

    size_t rlen;
    unsigned char *data;
    data = arkime_agent_get_aws_metadata("/latest/dynamic/instance-identity/document", -1, &rlen);

    char *accountId = moloch_js0n_get_str(data, rlen, "accountId");
    char *region = moloch_js0n_get_str(data, rlen, "region");
    char *availabilityZone = moloch_js0n_get_str(data, rlen, "availabilityZone");
    char *instanceId = moloch_js0n_get_str(data, rlen, "instanceId");

    sharedInfoLen = snprintf(sharedInfo, sizeof(sharedInfo), 
        "\"accountId\":\"%s\","
        "\"region\":\"%s\","
        "\"availabilityZone\":\"%s\","
        "\"instanceId\":\"%s\"",
        accountId,
        region,
        availabilityZone,
        instanceId
    );

    constantItemsLen = snprintf(constantItems, sizeof(constantItems), 
        "\"accountId\":\"%s\","
        "\"region\":\"%s\","
        "\"availabilityZone\":\"%s\","
        "\"instanceId\":\"%s\","
        "\"node\":\"%s\"",
        accountId,
        region,
        availabilityZone,
        instanceId,
        config.nodeName
    );

    // Setup Agent Endpoint
    agentEndpoint = moloch_config_str(NULL, "agentEndpoint", NULL);
    agentServer = moloch_http_create_server(agentEndpoint, 2, 2, 0);
    moloch_http_set_print_errors(agentServer);

    // Setup config
    arkime_agent_config_update();
    // ALW - should fetch new config periodically

    // Stuff
    moloch_add_can_quit(arkime_agent_can_quit, "agent ready to quit");
    moloch_writers_add("agent", agent_writer_init);
    moloch_db_set_send_bulk(arkime_agent_send_telemetry);
}
/******************************************************************************/
void arkime_agent_exit()
{
}
