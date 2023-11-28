/******************************************************************************/
/* wise.js -- wiseService viewer plugin
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const http = require('http');
const https = require('https');

exports.init = function (Config, emitter) {
  let baseURL = Config.get('wiseURL', undefined);
  if (baseURL === undefined) {
    baseURL = 'http://' + Config.get('wiseHost', '127.0.0.1') + ':' + Config.get('wisePort', 8081);
  }

  const client = baseURL.startsWith('https') ? https : http;

  emitter.on('makeSessionDetail', function (cb) {
    const url = baseURL + '/views';
    const req = client.request(url, function (res) {
      let body = '';
      res.on('data', function (chunk) {
        body += chunk;
      });
      res.on('end', function () {
        if (res.statusCode !== 200) {
          return cb(null, {});
        }
        const result = JSON.parse(body);
        return cb(null, result);
      });
    });
    req.on('error', function (err) {
      console.log('WISE Session Detail ERROR', err);
      return cb(err, {});
    });
    req.end();
  });

  emitter.on('makeRightClick', function (cb) {
    const url = baseURL + '/valueActions';
    const req = client.request(url, function (res) {
      let body = '';
      res.on('data', function (chunk) {
        body += chunk;
      });
      res.on('end', function () {
        if (res.statusCode !== 200) {
          return cb(null, {});
        }
        const result = JSON.parse(body);
        return cb(null, result);
      });
    });
    req.on('error', function (err) {
      console.log('WISE Right Click ERROR', err);
      return cb(err, {});
    });
    req.end();
  });

  emitter.on('makeFieldActions', function (cb) {
    const url = baseURL + '/fieldActions';
    const req = client.request(url, function (res) {
      let body = '';
      res.on('data', function (chunk) {
        body += chunk;
      });
      res.on('end', function () {
        if (res.statusCode !== 200) {
          return cb(null, {});
        }
        const result = JSON.parse(body);
        return cb(null, result);
      });
    });
    req.on('error', function (err) {
      console.log('WISE Right Click ERROR', err);
      return cb(err, {});
    });
    req.end();
  });
};
