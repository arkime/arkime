'use strict';

const { Client } = require('@elastic/elasticsearch');

const fs = require('fs');
const Links = require('./links');

class Db {
  static debug;
  static client;

  static initialize (options) {
    Db.debug = options.debug;

    const esSSLOptions = { rejectUnauthorized: !options.insecure, ca: options.ca };
    if (options.clientKey) {
      esSSLOptions.key = fs.readFileSync(options.clientKey);
      esSSLOptions.cert = fs.readFileSync(options.clientCert);
      if (options.clientKeyPass) {
        esSSLOptions.passphrase = options.clientKeyPass;
      }
    }

    const esOptions = {
      node: options.node,
      maxRetries: 2,
      requestTimeout: (parseInt(options.requestTimeout) + 30) * 1000 || 330000,
      ssl: esSSLOptions
    };

    if (options.apiKey) {
      esOptions.auth = {
        apiKey: options.apiKey
      };
    } else if (options.basicAuth) {
      let basicAuth = options.basicAuth;
      if (!basicAuth.includes(':')) {
        basicAuth = Buffer.from(basicAuth, 'base64').toString();
      }
      basicAuth = basicAuth.split(':');
      esOptions.auth = {
        username: basicAuth[0],
        password: basicAuth[1]
      };
    }

    Db.client = new Client(esOptions);
  };

  static getLinks (query) {
    console.log('ENTER Db.getLinks', query);

    const f = new Links();

    return Db.client.search({
      index: 'cont3xt_links',
      body: query,
      rest_total_hits_as_int: true
    });
  }
}

module.exports = Db;
