/******************************************************************************/
/* hodi - History of Observed Data Indictors
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
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

var wiseSource     = require('./wiseSource.js')
  , elasticsearch  = require('elasticsearch')
  , util           = require('util')
  , LRU            = require('lru-cache')
  ;
//////////////////////////////////////////////////////////////////////////////////
function HODISource (api, section) {
  HODISource.super_.call(this, api, section);
  this.esHost  = api.getConfig("hodi", "esHost");
  this.bulk = [];

  if (this.esHost === undefined) {
    console.log(this.section, "- No esHost defined");
    return;
  }

  this.domain = LRU({max: this.api.getConfig("hodi", "cacheSize", 100000),
                      maxAge: 1000 * 60 * +this.api.getConfig("hodi", "cacheAgeMin", "5")});
  this.ip = LRU({max: this.api.getConfig("hodi", "cacheSize", 100000),
                      maxAge: 1000 * 60 * +this.api.getConfig("hodi", "cacheAgeMin", "5")});
  this.md5 = LRU({max: this.api.getConfig("hodi", "cacheSize", 100000),
                      maxAge: 1000 * 60 * +this.api.getConfig("hodi", "cacheAgeMin", "5")});
  this.email = LRU({max: this.api.getConfig("hodi", "cacheSize", 100000),
                      maxAge: 1000 * 60 * +this.api.getConfig("hodi", "cacheAgeMin", "5")});

  this.client = new elasticsearch.Client({
                      host: this.esHost,
                      keepAlive: true,
                      minSockets: 5,
                      maxSockets: 51
                    });

  ["hodi-domain", "hodi-ip", "hodi-md5", "hodi-email"].forEach((index) => {
    this.client.indices.exists({index: index}, (err, exists) => {
      if (exists) {
        this.client.indices.putSettings({index: index, body: {
          "index.refresh_interval": "60s"
        }});
        return;
      }
      this.client.indices.create({index: index, body: {
        settings: {
          "index.refresh_interval": "60s"
        },
        mappings: {
          hodi: {
            _all : {enabled: false},
            properties: {
              firstSeen: {type: "date", index: "not_analyzed"},
               lastSeen: {type: "date", index: "not_analyzed"},
               count:    {type: "long", index: "not_analyzed"}
            }
          }
        }
      }});
    });
  });

  this.api.addSource("hodi", this);
  setInterval(this.sendBulk.bind(this), 1000);
}
util.inherits(HODISource, wiseSource);

//////////////////////////////////////////////////////////////////////////////////
HODISource.prototype.sendBulk = function () {
  if (this.bulk.length === 0) {
    return;
  }
  if (this.api.debug > 0) {
    console.log("HODI", this.bulk.length);
  }
  this.client.bulk({body: this.bulk});
  this.bulk = [];
};
//////////////////////////////////////////////////////////////////////////////////
HODISource.prototype.process = function (index, id, cb) {
  cb(null, undefined);

  var info = this[index].get(id);
  if (info) {
    return;
  }

  this[index].set(id, true);

  var date = new Date().toISOString();
  this.bulk.push({update: {_index: `hodi-${index}`, _type: "hodi", _id: id}});
  this.bulk.push({script_file: "hodi", params: {lastSeen: date}, upsert: {count: 1, firstSeen: date, lastSeen: date}});
  if (this.bulk.length >= 1000) {
    this.sendBulk();
  }
};
//////////////////////////////////////////////////////////////////////////////////
HODISource.prototype.getDomain = function(domain, cb) {
  this.process("domain", domain, cb);
};
//////////////////////////////////////////////////////////////////////////////////
HODISource.prototype.getIp = function(ip, cb) {
  this.process("ip", ip, cb);
};
//////////////////////////////////////////////////////////////////////////////////
HODISource.prototype.getMd5 = function(md5, cb) {
  this.process("md5", md5, cb);
};
//////////////////////////////////////////////////////////////////////////////////
HODISource.prototype.getEmail = function(email, cb) {
  this.process("email", email, cb);
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var source = new HODISource(api, "hodi");
};
