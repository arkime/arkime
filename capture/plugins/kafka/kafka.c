/* kafka.c  -- Simple plugin that sends json encoded data to Kafka
 *
 * Copyright 2020 Sqooba AG. All rights reserved.
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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <uuid/uuid.h>
#include <ctype.h>
#include <errno.h>
#include "moloch.h"
#include "bsb.h"
#include "rdkafka.h"

LOCAL rd_kafka_t *rk = NULL; /* Producer instance handle */
LOCAL rd_kafka_conf_t *conf; /* Temporary configuration object */
LOCAL char errstr[512];      /* librdkafka API error reporting buffer */
LOCAL const char *brokers;   /* Argument: broker list */
LOCAL const char *topic;     /* Argument: topic to produce to */
LOCAL const char kafkaSSL;
LOCAL const char *kafkaSSLCALocation;
LOCAL const char *kafkaSSLCertificateLocation;
LOCAL const char *kafkaSSLKeyLocation;
LOCAL const char *kafkaSSLKeyPassword;

extern MolochConfig_t config;

/******************************************************************************
 * Message delivery report callback using the richer rd_kafka_message_t object.
 */
LOCAL void kafka_msg_delivered_cb(rd_kafka_t *rk,
                           const rd_kafka_message_t *rkmessage, void *opaque) {

    if (rkmessage->err)
        LOG(
            "%% Message delivery failed (broker %"PRId32"): %s\n",
            rd_kafka_message_broker_id(rkmessage),
            rd_kafka_err2str(rkmessage->err));
    else if (config.debug)
        LOG(
            "%% Message delivered in %.2fms (%zd bytes, offset %"PRId64", "
            "partition %"PRId32", broker %"PRId32"): %.*s\n",
            (float)rd_kafka_message_latency(rkmessage) / 1000.0,
            rkmessage->len, rkmessage->offset,
            rkmessage->partition,
            rd_kafka_message_broker_id(rkmessage),
            (int)rkmessage->len, (const char *)rkmessage->payload);

    char *json = (char *)rkmessage->_private; /* V_OPAQUE */
    if (config.debug > 2)
        LOG("%% opaque=%p\n", json);
    MOLOCH_SIZE_FREE(buffer, json);
}

/******************************************************************************/
LOCAL void kafka_send_session(char *json, int len)
{

    rd_kafka_resp_err_t err;

    if (config.debug)
        LOG("About to send %d bytes in kafka %s, opaque=%p\n", len, topic, json);

retry:
    err = rd_kafka_producev(
        /* Producer handle */
        rk,
        /* Topic name */
        RD_KAFKA_V_TOPIC(topic),
        /* Message value and length */
        RD_KAFKA_V_VALUE(json, len),
        /* Per-Message opaque, provided in
                    * delivery report callback as
                    * msg_opaque. */
        RD_KAFKA_V_OPAQUE(json),
        /* End sentinel */
        RD_KAFKA_V_END);

    if (err)
    {
        /*
                * Failed to *enqueue* message for producing.
                */
        LOG("%% Failed to produce to topic %s: %s\n",
            topic, rd_kafka_err2str(err));

        if (err == RD_KAFKA_RESP_ERR__QUEUE_FULL)
        {
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
            rd_kafka_poll(rk, 1000 /*block for max 1000ms*/);
            goto retry;
        }
    }
    else
    {
        if (config.debug)
            LOG("%% Enqueued message (%d bytes) "
                "for topic %s\n",
                len, topic);
    }

    /* A producer application should continually serve
        * the delivery report queue by calling rd_kafka_poll()
        * at frequent intervals.
        * Either put the poll call in your main loop, or in a
        * dedicated thread, or call it after every
        * rd_kafka_produce() call.
        * Just make sure that rd_kafka_poll() is still called
        * during periods where you are not producing any messages
        * to make sure previously produced messages have their
        * delivery report callback served (and any other callbacks
        * you register). */
    rd_kafka_poll(rk, 0 /*non-blocking*/);
}

LOCAL MolochDbSendBulkFunc send_to_kafka = kafka_send_session;

/******************************************************************************/
/*
 * Called by moloch when moloch is quiting
 */
LOCAL void kafka_plugin_exit()
{
    if (rk != NULL)
    {
        if (config.debug)
            LOG("%% Flushing final messages..\n");
        rd_kafka_flush(rk, 10 * 1000 /* wait for max 10 seconds */);

        /* If the output queue is still not empty there is an issue
            * with producing messages to the clusters. */
        if (rd_kafka_outq_len(rk) > 0)
            LOG("%% %d message(s) were not delivered\n",
                rd_kafka_outq_len(rk));

        /* Destroy the producer instance */
        rd_kafka_destroy(rk);
    }
}

/******************************************************************************/
/*
 * Called by moloch when the plugin is loaded
 */
void moloch_plugin_init()
{
    moloch_plugins_register("kafka", TRUE);

    LOG("%% Loading Kafka plugin");

    moloch_plugins_set_cb("kafka",
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          NULL,
                          kafka_plugin_exit,
                          NULL);

    conf = rd_kafka_conf_new();
    brokers = *moloch_config_str_list(NULL, "kafkaBootstrapServers", "");
    topic = moloch_config_str(NULL, "kafkaTopic", "arkime-json");

    // See more config on https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md

    if (rd_kafka_conf_set(conf, "metadata.broker.list", brokers,
                          errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        LOGEXIT("Error configuring kafka:metadata.broker.list, error = %s\n", errstr);
    }

    if (config.debug)
        LOG("%% kafka broker %s", brokers);

    kafkaSSL = moloch_config_boolean(NULL, "kafkaSSL", false);
    if kafkaSSL
    {
        if (config.debug)
            LOG("%% kafka SSL is turned on");

        if (rd_kafka_conf_set(conf, "security.protocol", "SSL",
                              errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
        {
            LOGEXIT("Error configuring kafka:security.protocol, error = %s\n", errstr);
        }

        kafkaSSLCALocation = moloch_config_str(NULL, "kafkaSSLCALocation", NULL);
        if kafkaSSLCALocation
        {
            if (rd_kafka_conf_set(conf, "ssl.ca.location", ca_cert,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
            {
                LOGEXIT("Error configuring kafka:ssl.ca.location, error = %s\n", errstr);
            }
        }

        kafkaSSLCertificateLocation = moloch_config_str(NULL, "kafkaSSLCertificateLocation", NULL);
	    if kafkaSSLCertificateLocation
	    {
	        if (rd_kafka_conf_set(conf, "ssl.certificate.location", client_cert,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
            {
                LOGEXIT("Error configuring kafka:ssl.certificate.location, error = %s\n", errstr);
            }
	    }

        kafkaSSLKeyLocation = moloch_config_str(NULL, "kafkaSSLKeyLocation", NULL);
        if kafkaSSLKeyLocation {
            if (rd_kafka_conf_set(conf, "ssl.key.location", client_key,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
            {
                LOGEXIT("Error configuring kafka:ssl.key.location, error = %s\n", errstr);
            }
        }

        kafkaSSLKeyPassword = moloch_config_str(NULL, "kafkaSSLKeyPassword", NULL);
        if kafkaSSLKeyPassword {
             if (rd_kafka_conf_set(conf, "ssl.key.password", client_key_password,
                                  errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
            {
                LOGEXIT("Error configuring kafka:ss.key.password, error = %s\n", errstr);
            }
        }
    }

    rd_kafka_conf_set_dr_msg_cb(conf, kafka_msg_delivered_cb);

    rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        LOGEXIT("%% Failed to create new producer: %s\n", errstr);
    }

    moloch_db_set_send_bulk(send_to_kafka);

    LOG("%% Kafka plugin loaded");
}
