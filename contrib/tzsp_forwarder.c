/******************************************************************************/
/* tzsp_forwarder.c
 *
 *
 * Simple TZSP forwarder that reads from the interface and sends to the hostname
 *
 * Usage: tzsp_forwarder <interface> <hostname>
 *
 * Compile: tzsp_forwarder.c -o tzsp_forwarder -l pcap
 *
 * Copyright 2025 All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#define TZSP_PORT 37008
#define TZSP_VERSION 1
#define TZSP_TYPE_RX_PACKET 0

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t protocol;
} tzsp_header_t;

int sock_fd;
struct sockaddr_in dest_addr;

void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    unsigned char tzsp_packet[0xffff];
    int offset = 0;
    
    // Build TZSP header
    tzsp_packet[offset++] = TZSP_VERSION;  // Version
    tzsp_packet[offset++] = TZSP_TYPE_RX_PACKET;  // Type
    tzsp_packet[offset++] = (1 >> 8) & 0xff;  // Protocol (Ethernet) - MSB
    tzsp_packet[offset++] = 1 & 0xff;  // Protocol - LSB
    
    // Add END tag (0x01)
    tzsp_packet[offset++] = 0x01;
    
    // Copy packet data
    memcpy(tzsp_packet + offset, packet, pkthdr->caplen);
    
    // Send TZSP packet
    sendto(sock_fd, tzsp_packet, pkthdr->caplen + offset, 0,
           (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <hostname>\n", argv[0]);
        return 1;
    }
    
    char *interface = argv[1];
    char *hostname = argv[2];
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    struct addrinfo hints, *res;
    
    //
    // Resolve hostname
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Failed to resolve hostname: %s\n", hostname);
        return 1;
    }
    
    // Create UDP socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return 1;
    }
    
    // Setup destination address
    memcpy(&dest_addr, res->ai_addr, sizeof(struct sockaddr_in));
    dest_addr.sin_port = htons(TZSP_PORT);
    freeaddrinfo(res);
    
    printf("Forwarding packets from %s to %s:%d\n", interface, hostname, TZSP_PORT);
    
    // Open pcap handle
    handle = pcap_open_live(interface, 65535, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "pcap_open_live failed: %s\n", errbuf);
        close(sock_fd);
        return 1;
    }
    
    // Start capture loop
    pcap_loop(handle, 0, packet_handler, NULL);
    
    // Cleanup (never reached in normal operation)
    pcap_close(handle);
    close(sock_fd);
    
    return 0;
}
