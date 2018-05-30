/* reader-daq.c  -- daq instead of libpcap
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

#include "moloch.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "daq.h"
#include "pcap.h"

extern MolochConfig_t        config;

LOCAL const DAQ_Module_t    *module;
LOCAL void                  *handles[MAX_INTERFACES];

/******************************************************************************/
int reader_daq_stats(MolochReaderStats_t *stats)
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
        LOGEXIT("ERROR - Moloch requires full packet captures caplen: %d pktlen: %d", h->caplen, h->pktlen);
    }

    MolochPacket_t *packet = MOLOCH_TYPE_ALLOC0(MolochPacket_t);

    packet->pkt           = (u_char *)data;
    packet->ts            = h->ts;
    packet->pktlen        = h->pktlen;
    packet->readerPos     = ((MolochPacketBatch_t *)batch)->readerPos;

    moloch_packet_batch((MolochPacketBatch_t *)batch, packet);
    return DAQ_VERDICT_PASS;
}
/******************************************************************************/
LOCAL void *reader_daq_thread(gpointer posv)
{
    long pos = (long)posv;
    gpointer handle = handles[pos]

    MolochPacketBatch_t   batch;
    moloch_packet_batch_init(&batch);
    batch.readerPos = pos;
    while (1) {
        int r = daq_acquire(module, handle, 10000, reader_daq_packet_cb, &batch);
        moloch_packet_batch_flush(&batch);

        // Some kind of failure we quit
        if (unlikely(r)) {
            LOG("DAQ quiting %d %s", r, daq_get_error(module, handle));
            moloch_quit();
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
    moloch_packet_set_linksnap(daq_get_datalink_type(module, handles[0]), config.snapLen);

    int i;
    for (i = 0; i < MAX_INTERFACES && config.interface[i]; i++) {
        if (config.bpf) {
            err = daq_set_filter(module, handles[i], config.bpf);

            if (err) {
                LOGEXIT("DAQ set filter error %d %s for  %s", err, daq_get_error(module, handles[i]), config.bpf);
            }
        }

        err = daq_start(module, handles[i]);

        if (err) {
            LOGEXIT("DAQ start error %d %s", err, daq_get_error(module, handles[i]));
        }

        char name[100];
        snprintf(name, sizeof(name), "moloch-daq%d", i);
        g_thread_new(name, &reader_daq_thread, NULL);
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
void reader_daq_init(char *UNUSED(name))
{
    int err;
    DAQ_Config_t cfg;


    gchar **dirs = moloch_config_str_list(NULL, "daqModuleDirs", "/usr/local/lib/daq");
    gchar *moduleName = moloch_config_str(NULL, "daqModule", "pcap");

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

    moloch_reader_start         = reader_daq_start;
    moloch_reader_stop          = reader_daq_stop;
    moloch_reader_stats         = reader_daq_stats;
}
/******************************************************************************/
void moloch_plugin_init()
{
    moloch_readers_add("daq", reader_daq_init);
}
