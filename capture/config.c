/******************************************************************************/
/* config.c  -- Functions dealing with the config file
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <string.h>
#include <unistd.h>
#include "glib.h"
#include "moloch.h"

extern char          *configFile;
extern char          *nodeName;
extern gchar         *pcapFile;
extern gboolean       debug;
extern MolochConfig_t config;

/******************************************************************************/
gchar *moloch_config_str(GKeyFile *keyfile, char *key, char *d)
{
    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        return g_key_file_get_string(keyfile, nodeName, key, NULL);
    }

    if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        return g_key_file_get_string(keyfile, config.nodeClass, key, NULL);
    }

    if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        return g_key_file_get_string(keyfile, "default", key, NULL);
    }

    if (!d)
        return NULL;

    return g_strdup(d);
}

/******************************************************************************/
uint32_t moloch_config_int(GKeyFile *keyfile, char *key, uint32_t d, uint32_t min, uint32_t max)
{
    uint32_t value = d;

    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        value = g_key_file_get_integer(keyfile, nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_integer(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_integer(keyfile, "default", key, NULL);
    }

    if (value < min)
        value = min;
    if (value > max)
        value = max;

    return value;
}

/******************************************************************************/
char moloch_config_boolean(GKeyFile *keyfile, char *key, char d)
{
    gboolean value = d;

    if (g_key_file_has_key(keyfile, nodeName, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, nodeName, key, NULL);
    } else if (config.nodeClass && g_key_file_has_key(keyfile, config.nodeClass, key, NULL)) {
        value = g_key_file_get_boolean(keyfile, config.nodeClass, key, NULL);
    } else if (g_key_file_has_key(keyfile, "default", key, NULL)) {
        value = g_key_file_get_boolean(keyfile, "default", key, NULL);
    }

    return value;
}

/******************************************************************************/
void moloch_config_load() 
{

    gboolean  status;
    GKeyFile *keyfile = g_key_file_new();
    GError   *error = 0;

    status = g_key_file_load_from_file(keyfile, configFile, G_KEY_FILE_NONE, &error);
    if (!status || error) {
        printf("Couldn't load config file (%s) %s\n", configFile, (error?error->message:""));
        exit(1);
    }

    config.nodeClass        = moloch_config_str(keyfile, "nodeClass", NULL);

    config.elasticsearch    = moloch_config_str(keyfile, "elasticsearch", "localhost:9200");
    config.interface        = moloch_config_str(keyfile, "interface", NULL);
    config.pcapDir          = moloch_config_str(keyfile, "pcapDir", NULL);
    config.bpfFilter        = moloch_config_str(keyfile, "bpfFilter", NULL);
    config.yara             = moloch_config_str(keyfile, "yara", NULL);
    config.geoipFile        = moloch_config_str(keyfile, "geoipFile", NULL);
    config.dropUser         = moloch_config_str(keyfile, "dropUser", NULL);
    config.dropGroup        = moloch_config_str(keyfile, "dropGroup", NULL);

    config.maxFileSizeG     = moloch_config_int(keyfile, "maxFileSizeG", 4, 1, 63);
    config.udpTimeout       = moloch_config_int(keyfile, "udpTimeout", 60, 10, 0xffff);
    config.tcpTimeout       = moloch_config_int(keyfile, "tcpTimeout", 60*8, 10, 0xffff);
    config.tcpSaveTimeout   = moloch_config_int(keyfile, "tcpSaveTimeout", 60*8, 10, 60*120);
    config.maxStreams       = moloch_config_int(keyfile, "maxStreams", 1500000, 1, 16777215);
    config.maxPackets       = moloch_config_int(keyfile, "maxPackets", 10000, 1, 1000000);
    config.minFreeSpaceG    = moloch_config_int(keyfile, "freeSpaceG", 100, 1, 100000);
    config.dbBulkSize       = moloch_config_int(keyfile, "dbBulkSize", 200000, 1, 1000000);
    config.maxESConns       = moloch_config_int(keyfile, "maxESConns", 100, 10, 1000);
    config.maxESRequests    = moloch_config_int(keyfile, "maxESRequests", 500, 10, 5000);
    config.logEveryXPackets = moloch_config_int(keyfile, "logEveryXPackets", 50000, 1000, 1000000);
    config.packetsPerPoll   = moloch_config_int(keyfile, "packetsPerPoll", 50000, 1000, 1000000);
    config.pcapBufferSize   = moloch_config_int(keyfile, "pcapBufferSize", 300000000, 100000, 0xffffffff);
    config.pcapWriteSize    = moloch_config_int(keyfile, "pcapWriteSize", 0x3ffff, 0x3ffff, 0x1fffff);


    config.logUnknownProtocols   = moloch_config_boolean(keyfile, "logUnknownProtocols", debug);
    config.logESRequests         = moloch_config_boolean(keyfile, "logESRequests", debug);
    config.logFileCreation       = moloch_config_boolean(keyfile, "logFileCreation", debug);

    g_key_file_free(keyfile);

}
/******************************************************************************/
void moloch_config_init()
{
    moloch_config_load();

    if (debug) {
        LOG("nodeClass: %s", config.nodeClass);
        LOG("elasticsearch: %s", config.elasticsearch);
        LOG("interface: %s", config.interface);
        LOG("pcapDir: %s", config.pcapDir);
        LOG("bpfFilter: %s", config.bpfFilter);
        LOG("yara: %s", config.yara);
        LOG("geoipFile: %s", config.geoipFile);
        LOG("dropUser: %s", config.dropUser);
        LOG("dropGroup: %s", config.dropGroup);

        LOG("maxFileSizeG: %u", config.maxFileSizeG);
        LOG("udpTimeout: %u", config.udpTimeout);
        LOG("tcpTimeout: %u", config.tcpTimeout);
        LOG("tcpSaveTimeout: %u", config.tcpSaveTimeout);
        LOG("maxStreams: %u", config.maxStreams);
        LOG("maxPackets: %u", config.maxPackets);
        LOG("minFreeSpaceG: %u", config.minFreeSpaceG);
        LOG("dbBulkSize: %u", config.dbBulkSize);
        LOG("maxESConns: %u", config.maxESConns);
        LOG("maxESRequests: %u", config.maxESRequests);
        LOG("logEveryXPackets: %u", config.logEveryXPackets);
        LOG("packetsPerPoll: %u", config.packetsPerPoll);
        LOG("pcapBufferSize: %u", config.pcapBufferSize);
        LOG("pcapWriteSize: %u", config.pcapWriteSize);

        LOG("logUnknownProtocols: %s", (config.logUnknownProtocols?"true":"false"));
        LOG("logESRequests: %s", (config.logESRequests?"true":"false"));
        LOG("logFileCreation: %s", (config.logFileCreation?"true":"false"));
    }

    if (!config.interface && !pcapFile) {
        printf("Need to set interface or pcapfile\n");
        exit (1);
    }
}
/******************************************************************************/
void moloch_config_exit()
{
    if (config.nodeClass)
        g_free(config.nodeClass);
    if (config.interface)
        g_free(config.interface);
    if (config.elasticsearch)
        g_free(config.elasticsearch);
    if (config.bpfFilter)
        g_free(config.bpfFilter);
    if (config.yara)
        g_free(config.yara);
    if (config.pcapDir)
        g_free(config.pcapDir);
}
