/* reader-daq.c  -- daq instead of libpcap
 *
 * Copyright 2012-2017 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arkime.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "daq.h"
#include "pcap.h"

extern ArkimeConfig_t        config;

LOCAL const DAQ_Module_t    *module;
LOCAL void                  *handles[MAX_INTERFACES];

/******************************************************************************/
int reader_daq_stats(ArkimeReaderStats_t *stats)
{
    DAQ_Stats_t daq_stats;

    stats->dropped = 0;
    stats->total = 0;

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        int err = daq_get_stats(module, handles[i], &daq_stats);

        if (err)
            continue;
        stats->dropped += daq_stats.hw_packets_dropped;
        stats->total += daq_stats.hw_packets_received;
    }
    return 0;
}
/******************************************************************************/
DAQ_Verdict reader_daq_packet_cb(void *batch, const DAQ_PktHdr_t *h, const uint8_t *data)
{
    if (unlikely(h->caplen != h->pktlen)) {
        LOGEXIT("ERROR - Arkime requires full packet captures caplen: %d pktlen: %d", h->caplen, h->pktlen);
    }

    ArkimePacket_t *packet = ARKIME_TYPE_ALLOC0(ArkimePacket_t);

    packet->pkt           = (u_char *)data;
    packet->ts            = h->ts;
    packet->pktlen        = h->pktlen;
    packet->readerPos     = ((ArkimePacketBatch_t *)batch)->readerPos;

    arkime_packet_batch((ArkimePacketBatch_t *)batch, packet);
    return DAQ_VERDICT_PASS;
}
/******************************************************************************/
LOCAL void *reader_daq_thread(gpointer posv)
{
    long pos = (long)posv;
    gpointer handle = handles[pos];

    ArkimePacketBatch_t   batch;
    arkime_packet_batch_init(&batch);
    batch.readerPos = pos;
    while (1) {
        int r = daq_acquire(module, handle, 10000, reader_daq_packet_cb, &batch);
        arkime_packet_batch_flush(&batch);

        // Some kind of failure we quit
        if (unlikely(r)) {
            LOG("DAQ quiting %d %s", r, daq_get_error(module, handle));
            arkime_quit();
            module = 0;
            break;
        }
    }
    return NULL;
}
/******************************************************************************/
void reader_daq_start() {
    int err;

    //ALW - Bug: assumes all linktypes are the same
    arkime_packet_set_dltsnap(daq_get_datalink_type(module, handles[0]), config.snapLen);

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (config.bpf) {
            err = daq_set_filter(module, handles[i], config.bpf);

            if (err) {
                LOGEXIT("ERROR - DAQ set filter error %d %s for  %s", err, daq_get_error(module, handles[i]), config.bpf);
            }
        }

        err = daq_start(module, handles[i]);

        if (err) {
            LOGEXIT("DAQ start error %d %s", err, daq_get_error(module, handles[i]));
        }

        char name[100];
        snprintf(name, sizeof(name), "arkime-daq%d", i);
        g_thread_unref(g_thread_new(name, &reader_daq_thread, NULL));
    }
}
/******************************************************************************/
void reader_daq_stop()
{
    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (handles[i])
            daq_breakloop(module, handles[i]);
    }
}
/******************************************************************************/
void reader_daq_exit()
{
    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (handles[i])
            daq_shutdown(module, handles[i]);
    }
}
/******************************************************************************/
void reader_daq_init(char *UNUSED(name))
{
    int err;
    DAQ_Config_t cfg;


    gchar **dirs = arkime_config_str_list(NULL, "daqModuleDirs", "/usr/local/lib/daq");
    gchar *moduleName = arkime_config_str(NULL, "daqModule", "pcap");

    err = daq_load_modules((const char **)dirs);
    if (err) {
        LOGEXIT("Can't load DAQ modules = %d\n", err);
    }

    module = daq_find_module(moduleName);
    if (!module) {
        LOGEXIT("Can't find %s DAQ module\n", moduleName);
    }


    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.name = config.interface[i];
        cfg.snaplen = config.snapLen;
        cfg.timeout = -1;
        cfg.mode = DAQ_MODE_PASSIVE;

        char buf[256] = "";
        err = daq_initialize(module, &cfg, &handles[i], buf, sizeof(buf));

        if (err) {
            LOGEXIT("Can't initialize DAQ %s %d %s\n", config.interface[i], err, buf);
        }
    }

    arkime_reader_start         = reader_daq_start;
    arkime_reader_stop          = reader_daq_stop;
    arkime_reader_stats         = reader_daq_stats;
    arkime_reader_exit          = reader_daq_exit;
}
/******************************************************************************/
void arkime_plugin_init()
{
    arkime_readers_add("daq", reader_daq_init);
}
