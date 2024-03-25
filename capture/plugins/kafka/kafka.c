/* kafka.c  -- Simple plugin that sends json encoded data to Kafka
 *
 * Copyright 2020 Sqooba AG. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include <ctype.h>
#include <errno.h>
#include "arkime.h"
#include "bsb.h"
#include "rdkafka.h"

LOCAL rd_kafka_t *rk = NULL; /* Producer instance handle */
LOCAL rd_kafka_conf_t *conf; /* Temporary configuration object */
LOCAL char errstr[512];      /* librdkafka API error reporting buffer */
LOCAL const char *brokers;   /* Argument: broker list */
LOCAL const char *topic;     /* Argument: topic to produce to */
LOCAL char kafkaSSL;
LOCAL const char *kafkaSSLCALocation;
LOCAL const char *kafkaSSLCertificateLocation;
LOCAL const char *kafkaSSLKeyLocation;
LOCAL const char *kafkaSSLKeyPassword;

extern ArkimeConfig_t config;

/******************************************************************************
 * Message delivery report callback using the richer rd_kafka_message_t object.
 */
LOCAL void kafka_msg_delivered_bulk_cb(rd_kafka_t *UNUSED(rk), const rd_kafka_message_t *rkmessage, void *UNUSED(opaque))
{
    if (rkmessage->err) {
        LOG("Message delivery failed (broker %"PRId32"): %s",
            rd_kafka_message_broker_id(rkmessage),
            rd_kafka_err2str(rkmessage->err));
    } else if (config.debug) {
        LOG("Message delivered in %.2fms (%zd bytes, offset %"PRId64", "
            "partition %"PRId32", broker %"PRId32")",
            (float)rd_kafka_message_latency(rkmessage) / 1000.0,
            rkmessage->len, rkmessage->offset,
            rkmessage->partition,
            rd_kafka_message_broker_id(rkmessage));
        if (config.debug > 3) {
            LOG("Payload: %.*s", (int)rkmessage->len, (const char *)rkmessage->payload);
        }
    }

    char *json = (char *)rkmessage->_private; /* V_OPAQUE */
    if (config.debug > 2)
        LOG("opaque=%p", json);
    arkime_http_free_buffer(json);
}

/******************************************************************************/
LOCAL void kafka_send_session_bulk(char *json, int len)
{
    if (config.debug)
        LOG("About to send %d bytes in kafka %s, opaque=%p", len, topic, json);

    // We will retry up to 5 times if RD_KAFKA_RESP_ERR__QUEUE_FULL error
    for (int i = 0; i < 5; i++) {
        rd_kafka_resp_err_t err;
        err = rd_kafka_producev(
                  rk,
                  RD_KAFKA_V_TOPIC(topic),
                  RD_KAFKA_V_VALUE(json, len),
                  RD_KAFKA_V_OPAQUE(json),
                  RD_KAFKA_V_END);

        if (err) {
            /* Failed to *enqueue* message for producing. */
            LOG("Failed to produce to topic %s: %s", topic, rd_kafka_err2str(err));

            if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL) {
                /* If the internal queue is full, wait for
                 * messages to be delivered and then retry.
                 * The internal queue represents both
                 * messages to be sent and messages that have
                 * been sent or failed, awaiting their
                 * delivery report callback to be called.
                 *
                 * The internal queue is limited by the
                 * configuration property
                 * queue.buffering.max.messages */
                rd_kafka_poll(rk, 100 /*block for max 100ms*/);
                continue; // Maybe Retry
            }

            break; // Not retrying
        }

        // Success
        if (config.debug)
            LOG("Enqueued message (%d bytes) for topic %s", len, topic);

        rd_kafka_poll(rk, 0 /*non-blocking*/);
        return;
    }

    // Didn't send
    arkime_http_free_buffer(json);
    rd_kafka_poll(rk, 0 /*non-blocking*/);
}

/******************************************************************************/
/*
 * Called by arkime when arkime is quiting
 */
LOCAL void kafka_plugin_exit()
{
    if (rk != NULL) {
        if (config.debug)
            LOG("Flushing final messages..");
        rd_kafka_flush(rk, 10 * 1000 /* wait for max 10 seconds */);

        /* If the output queue is still not empty there is an issue
         * with producing messages to the clusters. */
        if (rd_kafka_outq_len(rk) > 0)
            LOG("%d message(s) were not delivered",
                rd_kafka_outq_len(rk));

        /* Destroy the producer instance */
        rd_kafka_destroy(rk);
    }
}

/******************************************************************************/
/*
 * Called by arkime when the plugin is loaded
 */
void arkime_plugin_init()
{
    arkime_plugins_register("kafka", TRUE);

    LOG("Loading Kafka plugin");

    arkime_plugins_set_cb("kafka",
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          kafka_plugin_exit,
                          NULL);

    conf = rd_kafka_conf_new();
    brokers = *arkime_config_str_list(NULL, "kafkaBootstrapServers", "");
    topic = arkime_config_str(NULL, "kafkaTopic", "arkime-json");

    // See more config on https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md

    if (rd_kafka_conf_set(conf, "metadata.broker.list", brokers,
                          errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
        LOGEXIT("Error configuring kafka:metadata.broker.list, error = %s", errstr);
    }

    if (config.debug)
        LOG("kafka broker %s", brokers);

    kafkaSSL = arkime_config_boolean(NULL, "kafkaSSL", FALSE);
    if (kafkaSSL) {
        if (config.debug)
            LOG("kafka SSL is turned on");

        if (rd_kafka_conf_set(conf, "security.protocol", "SSL",
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            LOGEXIT("Error configuring kafka:security.protocol, error = %s", errstr);
        }

        kafkaSSLCALocation = arkime_config_str(NULL, "kafkaSSLCALocation", NULL);
        if (kafkaSSLCALocation) {
            if (rd_kafka_conf_set(conf, "ssl.ca.location", kafkaSSLCALocation,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                LOGEXIT("Error configuring kafka:ssl.ca.location, error = %s", errstr);
            }
        }

        kafkaSSLCertificateLocation = arkime_config_str(NULL, "kafkaSSLCertificateLocation", NULL);
        if (kafkaSSLCertificateLocation) {
            if (rd_kafka_conf_set(conf, "ssl.certificate.location", kafkaSSLCertificateLocation,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                LOGEXIT("Error configuring kafka:ssl.certificate.location, error = %s", errstr);
            }
        }

        kafkaSSLKeyLocation = arkime_config_str(NULL, "kafkaSSLKeyLocation", NULL);
        if (kafkaSSLKeyLocation) {
            if (rd_kafka_conf_set(conf, "ssl.key.location", kafkaSSLKeyLocation,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                LOGEXIT("Error configuring kafka:ssl.key.location, error = %s", errstr);
            }
        }

        kafkaSSLKeyPassword = arkime_config_str(NULL, "kafkaSSLKeyPassword", NULL);
        if (kafkaSSLKeyPassword) {
            if (rd_kafka_conf_set(conf, "ssl.key.password", kafkaSSLKeyPassword,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
                LOGEXIT("Error configuring kafka:ss.key.password, error = %s", errstr);
            }
        }
    }

    gsize keys_len;
    gchar **keys = arkime_config_section_keys(NULL, "kafka-config", &keys_len);

    int i;
    for (i = 0; i < (int)keys_len; i++) {
        char *value = arkime_config_section_str(NULL, "kafka-config", keys[i], NULL);
        if (rd_kafka_conf_set(conf, keys[i], value,
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK) {
            LOGEXIT("Error configuring %s, error = %s", keys[i], errstr);
        }
        g_free(value);
    }
    g_strfreev(keys);


    rd_kafka_conf_set_dr_msg_cb(conf, kafka_msg_delivered_bulk_cb);

    const char *kafkaMsgFormat = arkime_config_str(NULL, "kafkaMsgFormat", "bulk");
    if (strcmp(kafkaMsgFormat, "bulk") == 0) {
        arkime_db_set_send_bulk2(kafka_send_session_bulk, TRUE, FALSE, 0xffff);
    } else if (strcmp(kafkaMsgFormat, "bulk1") == 0) {
        arkime_db_set_send_bulk2(kafka_send_session_bulk, TRUE, FALSE, 1);
    } else if (strcmp(kafkaMsgFormat, "doc") == 0) {
        arkime_db_set_send_bulk2(kafka_send_session_bulk, FALSE, TRUE, 1);
    } else {
        LOGEXIT("Unknown config kafkaMsgFormat value '%s'", kafkaMsgFormat);
    }

    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk) {
        LOGEXIT("Failed to create new producer: %s", errstr);
    }

    if (config.debug > 2) {
        rd_kafka_conf_properties_show(stdout);
    }

    LOG("Kafka plugin loaded");
}
