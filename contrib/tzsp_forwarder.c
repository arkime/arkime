/******************************************************************************/
/* tzsp_forwarder.c
 *
 *
 * Simple TZSP forwarder that reads from the interface and sends to one or more hostnames
 *
 * Usage: tzsp_forwarder <interface> <hostname>[,<hostname>...] [bpf_filter]
 *    Each packet is sent to every hostname in the comma separated list.
 *    bpf_filter defaults to "not (port 37008 and (host <hostname> or ...))" if not provided,
 *    which prevents forwarding packets that are already being sent to any destination.
 *
 * Compile: cc tzsp_forwarder.c -o tzsp_forwarder -lpcap
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
#define MAX_DESTS 16

int sock_fd;
struct sockaddr_in dest_addrs[MAX_DESTS];
char *dest_names[MAX_DESTS];
int num_dests;

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

    // Defensive bound: cap caplen to actual remaining buffer space after header offset
    unsigned int caplen = pkthdr->caplen;
    unsigned int max_caplen = (unsigned int)(sizeof(tzsp_packet) - offset);
    if (caplen > max_caplen)
        caplen = max_caplen;

    // Copy packet data
    memcpy(tzsp_packet + offset, packet, caplen);

    // Send TZSP packet to each destination
    for (int i = 0; i < num_dests; i++) {
        sendto(sock_fd, tzsp_packet, caplen + offset, 0,
               (struct sockaddr *)&dest_addrs[i], sizeof(dest_addrs[i]));
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "Usage: %s <interface> <hostname>[,<hostname>...] [bpf_filter]\n", argv[0]);
        return 1;
    }

    char *interface = argv[1];
    char *hostnames = argv[2];
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    struct addrinfo hints, *res;

    char *default_filter_exp = NULL;
    char *filter_exp = NULL;
    struct bpf_program fp;

    // Split the comma separated hostnames and resolve each (needed for socket setup)
    char *hostlist = strdup(hostnames);
    if (!hostlist) {
        perror("strdup");
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    char *saveptr = NULL;
    for (char *hostname = strtok_r(hostlist, ",", &saveptr); hostname; hostname = strtok_r(NULL, ",", &saveptr)) {
        if (num_dests >= MAX_DESTS) {
            fprintf(stderr, "Too many hostnames, max is %d\n", MAX_DESTS);
            return 1;
        }

        if (getaddrinfo(hostname, NULL, &hints, &res) != 0) {
            fprintf(stderr, "Failed to resolve hostname: %s\n", hostname);
            return 1;
        }

        memcpy(&dest_addrs[num_dests], res->ai_addr, sizeof(struct sockaddr_in));
        dest_addrs[num_dests].sin_port = htons(TZSP_PORT);
        dest_names[num_dests] = hostname;
        num_dests++;
        freeaddrinfo(res);
    }

    if (num_dests == 0) {
        fprintf(stderr, "No hostnames provided\n");
        return 1;
    }

    // Determine BPF filter
    if (argc == 4) {
        filter_exp = argv[3]; // Use provided filter
    } else {
        // "not (port 37008 and (host a or host b ...))"
        size_t len = strlen(hostnames) + num_dests * 10 + 64;
        default_filter_exp = malloc(len);
        if (!default_filter_exp) {
            perror("malloc");
            return 1;
        }

        int pos = snprintf(default_filter_exp, len, "not (port %d and (", TZSP_PORT);
        for (int i = 0; i < num_dests; i++) {
            int n = snprintf(default_filter_exp + pos, len - pos, "%shost %s", i > 0 ? " or " : "", dest_names[i]);
            if (n < 0 || (size_t)n >= len - pos) {
                fprintf(stderr, "Hostnames too long\n");
                return 1;
            }
            pos += n;
        }
        snprintf(default_filter_exp + pos, len - pos, "))");
        filter_exp = default_filter_exp;
    }

    // Create UDP socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return 1;
    }

    printf("Forwarding packets from %s to", interface);
    for (int i = 0; i < num_dests; i++) {
        printf("%s %s:%d", i > 0 ? "," : "", dest_names[i], TZSP_PORT);
    }
    printf(" with bpf '%s'\n", filter_exp);

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
    free(default_filter_exp);
    free(hostlist);

    return 0;
}
