Kafka Write Plugin
====

The kafka writer plugin sends the SPI to Kafka instead of Elastic

# Build

```
./easybutton-build.sh --kafka
```

# Configure
The table below list all the possible configuration option of the kafka plugin.

| Property | Details | Example |
|----------|---------|---------|
| kafkaBootstrapServers | bootstrap servers, comma separated, to connet to | 1.2.3.4:9020,5.6.7.8:9020 |
| kafkaTopic | topic to send the SPI to | moloch-spi |
