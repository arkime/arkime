/******************************************************************************/
/* tzsp_forwarder.c
 *
 *
 * Simple TZSP forwarder that reads from the interface and sends to the hostname
 *
 * Usage: tzsp_forwarder <interface> <hostname> [bpf_filter]
 *    bpf_filter defaults to "not (host <hostname> and port 37008)" if not provided, 
 *    which prevents forwarding packets that are already being sent to the destination.
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
#define SNAP_LEN 64000

int sock_fd;
struct sockaddr_in dest_addr;

void packet_handler(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    unsigned char tzsp_packet[SNAP_LEN + 6]; // 6 bytes for TZSP header
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
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <interface> <hostname> [bpf_filter]\n", argv[0]);
        return 1;
    }

    char *interface = argv[1];
    char *hostname = argv[2];
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    struct addrinfo hints, *res;

    char default_filter_exp[256];
    char *filter_exp = NULL;
    struct bpf_program fp;

    // Resolve hostname (needed for socket setup)
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
        fprintf(stderr, "Failed to resolve hostname: %s\n", hostname);
        return 1;
    }

    // Determine BPF filter
    if (argc == 4) {
        filter_exp = argv[3]; // Use provided filter
    } else {
        snprintf(default_filter_exp, sizeof(default_filter_exp),
                 "not (host %s and port %d)", hostname, TZSP_PORT);
        filter_exp = default_filter_exp;
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

    printf("Forwarding packets from %s to %s:%d with bpf '%s'\n", interface, hostname, TZSP_PORT, filter_exp);

    // Open pcap handle
    handle = pcap_open_live(interface, SNAP_LEN, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "pcap_open_live failed: %s\n", errbuf);
        close(sock_fd);
        return 1;
    }

    if (pcap_compile(handle, &fp, filter_exp, 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_close(handle);
        close(sock_fd);
        return 1;
    }

    // Apply BPF filter
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_freecode(&fp);
        pcap_close(handle);
        close(sock_fd);
        return 1;
    }

    pcap_freecode(&fp); // Free the compiled filter

    // Start capture loop
    pcap_loop(handle, 0, packet_handler, NULL);

    // Cleanup (never reached in normal operation)
    pcap_close(handle);
    close(sock_fd);

    return 0;
}
