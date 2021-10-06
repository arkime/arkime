Kafka Write Plugin
====

The kafka writer plugin sends the SPI to Kafka instead of Elasticsearch.

Please note that communication to Elasticsearch is still needed, for the stats and other housekeeping tasks.

# Build

```
./easybutton-build.sh --kafka
```

# Configure

The table below list all the possible configuration option of the kafka plugin.

| Property | Details | Example |
|----------|---------|---------|
| kafkaBootstrapServers | bootstrap servers, comma separated, to connet to | 1.2.3.4:9020,5.6.7.8:9020 |
| kafkaTopic | topic to send the SPI to | arkime-spi |
| kafkaSSL | whether to enable SSL security protocol | `true` |
| kafkaSSLCALocation | path where the SSL CA is located  | `/path/to/ca.crt` |
| kafkaSSLCertificateLocation | path where the SSL client certificate is located | `/path/to/client.crt` |
| kafkaSSLKeyLocation | path where the SSL cilent key is located | `/path/to/client.key` |
| kafkaSSLKeyPassword | optional password for the client key |  |
