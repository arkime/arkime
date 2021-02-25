/******************************************************************************/
/* wise.js -- wiseService viewer plugin
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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
'use strict';

const http = require('http');

exports.init = function (Config, emitter) {
  const port = Config.get('wisePort', 8081);
  const host = Config.get('wiseHost', '127.0.0.1');

  emitter.on('makeSessionDetail', function (cb) {
    const url = 'http://' + host + ':' + port + '/views';
    const req = http.request(url, function (res) {
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
    const url = 'http://' + host + ':' + port + '/rightClicks';
    const req = http.request(url, function (res) {
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
