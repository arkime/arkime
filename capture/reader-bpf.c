/******************************************************************************/
/* reader-bpf.c  -- Reader using Berkeley Packet Filter (BPF)
 *
 * For FreeBSD and macOS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define _FILE_OFFSET_BITS 64
#include "arkime.h"

#if !defined(__FreeBSD__) && !defined(__APPLE__)
void reader_bpf_init(const char *UNUSED(name))
{
    CONFIGEXIT("BPF reader only supported on FreeBSD and macOS");
}
#else

#include <errno.h>
#include <net/if.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <poll.h>

#include <net/bpf.h>

#include "pcap.h"
#include <ifaddrs.h>

extern ArkimeConfig_t        config;
extern ArkimePcapFileHdr_t   pcapFileHeader;

typedef struct {
    int                  fd;
    uint8_t              interfacePos;
    u_int                bufsize;
    u_char              *buf;
    struct bpf_program   bpf;
    gboolean             hasBpf;
} ArkimeBpf_t;

#define MAX_BPF_READERS MAX_INTERFACES

LOCAL ArkimeBpf_t           readers[MAX_BPF_READERS];
LOCAL int                   numReaders;
LOCAL ArkimeReaderStats_t   gStats;
LOCAL ARKIME_LOCK_DEFINE(gStats);

/******************************************************************************/
int reader_bpf_stats(ArkimeReaderStats_t *stats)
{
    ARKIME_LOCK(gStats);
    *stats = gStats;
    ARKIME_UNLOCK(gStats);
    return 0;
}

/******************************************************************************/
LOCAL void *reader_bpf_thread(gpointer readerv)
{
    ArkimeBpf_t *reader = (ArkimeBpf_t *)readerv;
    struct pollfd pfd;
    struct bpf_hdr *bh;
    u_char *p;
    int bytes;
    int caplen;
    int initFunc = arkime_get_named_func("arkime_reader_thread_init");
    arkime_call_named_func(initFunc, reader->interfacePos, NULL);

    ArkimePacketBatch_t batch;
    arkime_packet_batch_init(&batch);

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = reader->fd;
    pfd.events = POLLIN;

    while (!config.quitting) {
        bytes = read(reader->fd, reader->buf, reader->bufsize);
        if (bytes < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                LOG("BPF read error on %s: %s", config.interface[reader->interfacePos], strerror(errno));
            }
            if (poll(&pfd, 1, 100) <= 0) {
                continue;
            }
            continue;
        }

        if (bytes == 0) {
            if (poll(&pfd, 1, 100) <= 0) {
                continue;
            }
            continue;
        }

        // Process packets in the buffer
        p = reader->buf;
        while (p < reader->buf + bytes) {
            bh = (struct bpf_hdr *)p;

            caplen = bh->bh_caplen;
            u_char *pkt = p + bh->bh_hdrlen;

            // Check for truncated packets
            if (unlikely(caplen != (int)bh->bh_datalen) && !config.readTruncatedPackets && !config.ignoreErrors) {
                LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d\n"
                        "See https://arkime.com/faq#arkime_requires_full_packet_captures_error",
                        caplen, bh->bh_datalen);
            }

            ArkimePacket_t *packet = arkime_packet_alloc();
            packet->pkt           = pkt;
            packet->pktlen        = caplen;
            packet->ts.tv_sec     = bh->bh_tstamp.tv_sec;
            packet->ts.tv_usec    = bh->bh_tstamp.tv_usec;
            packet->readerPos     = reader->interfacePos;

            arkime_packet_batch(&batch, packet);

            // Move to next packet, accounting for alignment
            p += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
        }

        arkime_packet_batch_flush(&batch);
    }

    int exitFunc = arkime_get_named_func("arkime_reader_thread_exit");
    arkime_call_named_func(exitFunc, reader->interfacePos, NULL);
    return NULL;
}

/******************************************************************************/
LOCAL char *find_bpf_device()
{
    static char bpf_device[256];
    struct stat sb;
    int n;

    // Try numbered BPF devices
    for (n = 0; n < 256; n++) {
        snprintf(bpf_device, sizeof(bpf_device), "/dev/bpf%d", n);
        if (stat(bpf_device, &sb) == 0) {
            return bpf_device;
        }
    }

    // Fall back to generic BPF device
    if (stat("/dev/bpf", &sb) == 0) {
        return "/dev/bpf";
    }

    return NULL;
}

/******************************************************************************/
LOCAL void bpf_set_filter(ArkimeBpf_t *reader, const char *filterstr)
{
    pcap_t *dpcap = pcap_open_dead(pcapFileHeader.dlt, pcapFileHeader.snaplen);
    if (!dpcap) {
        CONFIGEXIT("Failed to create dead pcap handle for BPF filter compilation");
    }

    if (pcap_compile(dpcap, &reader->bpf, filterstr, 1, PCAP_NETMASK_UNKNOWN) == -1) {
        CONFIGEXIT("BPF filter compilation failed for '%s': %s", filterstr, pcap_geterr(dpcap));
    }

    if (ioctl(reader->fd, BIOCSETF, &reader->bpf) < 0) {
        CONFIGEXIT("Failed to set BPF filter: %s", strerror(errno));
    }

    reader->hasBpf = TRUE;
    pcap_close(dpcap);
}

/******************************************************************************/
void reader_bpf_start()
{
    char name[100];
    for (int i = 0; i < numReaders; i++) {
        snprintf(name, sizeof(name), "arkime-bpf%d", readers[i].interfacePos);
        g_thread_unref(g_thread_new(name, &reader_bpf_thread, &readers[i]));
    }
}

/******************************************************************************/
void reader_bpf_exit()
{
    for (int i = 0; i < numReaders; i++) {
        if (readers[i].fd >= 0) {
            close(readers[i].fd);
        }
        if (readers[i].buf) {
            free(readers[i].buf);
        }
        if (readers[i].hasBpf) {
            pcap_freecode(&readers[i].bpf);
        }
    }
}

/******************************************************************************/
void reader_bpf_init(char *UNUSED(name))
{
    arkime_config_check("bpf", "", "bpfBufferSize", NULL);

    int buffersize = arkime_config_int(NULL, "bpfBufferSize", 1 << 21, 1 << 16, 1U << 31);

    if (buffersize % getpagesize() != 0) {
        CONFIGEXIT("bpfBufferSize=%d not divisible by pagesize %d", buffersize, getpagesize());
    }

    arkime_packet_set_dltsnap(DLT_EN10MB, config.snapLen);

    // Open BPF device
    char *bpf_dev = find_bpf_device();
    if (!bpf_dev) {
        CONFIGEXIT("No BPF device found. Check permissions on /dev/bpf*");
    }

    struct ifreq ifr;
    numReaders = 0;

    for (int i = 0; config.interface[i]; i++) {
        if (numReaders >= MAX_BPF_READERS) {
            CONFIGEXIT("Too many interfaces, max is %d", MAX_BPF_READERS);
        }

        ArkimeBpf_t *reader = &readers[numReaders];

        reader->interfacePos = i;

        // Open BPF device
        reader->fd = open(bpf_dev, O_RDONLY);
        if (reader->fd < 0) {
            CONFIGEXIT("Failed to open BPF device %s: %s", bpf_dev, strerror(errno));
        }

        // Set buffer size
        int blen = buffersize;
        if (ioctl(reader->fd, BIOCSBLEN, &blen) < 0) {
            CONFIGEXIT("Failed to set BPF buffer size to %d: %s", blen, strerror(errno));
        }

        // Set interface
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", config.interface[i]);

        if (ioctl(reader->fd, BIOCSETIF, &ifr) < 0) {
            CONFIGEXIT("Failed to bind BPF to interface %s: %s", config.interface[i], strerror(errno));
        }

        // Set non-blocking mode
        int n = 1;
        if (ioctl(reader->fd, BIOCIMMEDIATE, &n) < 0) {
            CONFIGEXIT("Failed to set BPF immediate mode: %s", strerror(errno));
        }

        // Get actual buffer size
        if (ioctl(reader->fd, BIOCGBLEN, &blen) < 0) {
            CONFIGEXIT("Failed to get BPF buffer size: %s", strerror(errno));
        }

        if (config.debug) {
            LOG("BPF buffer size for interface %s: %u", config.interface[i], blen);
        }

        reader->bufsize = blen;
        reader->buf = malloc(blen);
        if (!reader->buf) {
            CONFIGEXIT("Failed to allocate BPF buffer of size %u", blen);
        }

        // Set promiscuous mode
        n = 1;
        if (ioctl(reader->fd, BIOCPROMISC, &n) < 0) {
            CONFIGEXIT("Failed to set promiscuous mode: %s", strerror(errno));
        }

        // Set up BPF filter if provided
        if (config.bpf) {
            bpf_set_filter(reader, config.bpf);
        }

        numReaders++;
    }

    arkime_reader_start = reader_bpf_start;
    arkime_reader_exit = reader_bpf_exit;
    arkime_reader_stats = reader_bpf_stats;
}

#endif // __FreeBSD__ || __APPLE__
