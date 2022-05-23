'use strict';

const express = require('express');
const app = express();
const logger = require('morgan');
const http = require('http');
const bp = require('body-parser');
const jsonParser = bp.json();
const rawParser = bp.raw({ inflate: true, limit: '1000kb', type: '*/*' });
const request = require('request');
const Db = require('./db');

// ----------------------------------------------------------------------------
// Logging
// ----------------------------------------------------------------------------
app.use(logger(':date :username \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));
logger.token('username', (req, res) => {
  return req.user ? req.user.userId : '-';
});

// FAKE META DATA
app.get('/latest/dynamic/instance-identity/document', (req, res) => {
  res.send({
    accountId: '422770237620',
    architecture: 'x86_64',
    availabilityZone: 'us-east-1a',
    billingProducts: null,
    devpayProductCodes: null,
    marketplaceProductCodes: null,
    imageId: 'ami-03c83d92253831d77',
    instanceId: 'i-02c486d65c3b3b739',
    instanceType: 'c4.large',
    kernelId: null,
    pendingTime: '2022-03-24T20:22:47Z',
    privateIp: '172.135.147.2',
    ramdiskId: null,
    region: 'us-east-1',
    version: '2017-09-30'
  });
});

app.post('/config', [jsonParser], async (req, res) => {
  console.log('CONFIG QUERY', req.body);

  const rules = await Db.getMatchingConfig(req.body);
  console.log('RULES', JSON.stringify(rules));

  res.send({
    status: true,
    vxlanId: 12345,
    packetEndpoint: '10.89.81.198',
    source: '172.0.0.0/8',
    rules: rules
  });
});

app.post('/telemetry', [rawParser], (req, res) => {
  request({
    method: 'POST',
    url: 'http://localhost:9200/_bulk',
    body: req.body,
    headers: {
      'content-type': 'application/json'
    }
  }).on('response', function (response) {
  }).on('error', function (error) {
    console.log('ERROR', error);
  });

  res.send({
    status: true
  });
});

// MAIN
async function main () {
  await Db.initialize({
    node: 'http://localhost:9200',
    debug: 2
  });

  const count = await Db.countConfig();
  if (count === 0) {
    console.log('ALW - adding fake config');
    Db.putConfig({
      instanceId: 'i-02c486d65c3b3b739',
      priority: 10,
      rule: {
        name: 'rule1', ports: [443, 4443], action: 'none'
      }
    });

    Db.putConfig({
      accountId: '422770237620',
      availabilityZone: 'us-east-1a',
      region: 'us-east-1',
      priority: 20,
      rule: {
        name: 'rule2', ports: [22, 53], action: 'telemetry'
      }
    });

    Db.putConfig({
      accountId: '422770237620',
      availabilityZone: 'us-east-1a',
      region: 'us-east-1',
      priority: 30,
      rule: {
        name: 'rule3', ports: [80, 8081], action: 'packets'
      }
    });

    Db.putConfig({
      accountId: '422770237620',
      availabilityZone: 'us-east-1a',
      region: 'us-east-1',
      priority: 40,
      rule: {
        name: 'rule4', cidrs: ['3.0.0.0/8', '54.0.0.0/8'], action: 'both'
      }
    });
  }

  const server = http.createServer(app);
  server.listen(6666);
}

main();
