/******************************************************************************/
/*
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

var fs             = require('fs')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , elasticsearch  = require('elasticsearch')
  ;

//////////////////////////////////////////////////////////////////////////////////
function ElasticsearchSource (api, section) {
  ElasticsearchSource.super_.call(this, api, section);

  this.esIndex          = api.getConfig(section, "esIndex");
  this.esTimestampField = api.getConfig(section, "esTimestampField");
  this.esQueryField     = api.getConfig(section, "esQueryField");
  this.esResultField    = api.getConfig(section, "esResultField", 0);
  this.esMaxTimeMS      = api.getConfig(section, "esMaxTimeMS", 60*60*1000);
  this.elasticsearch    = api.getConfig(section, "elasticsearch");

  this.typeSetting();
  this.tagsSetting();

  ["esIndex", "esTimestampField", "esQueryField", "esResultField", "elasticsearch"].forEach((item) => {
    if (this[item] === undefined) {
      console.log(this.section, `- ERROR not loading since no ${item} specified in config file`);
      return;
    }
  });

  switch (this.type) {
  case "domain":
    this.getDomain = this.sendResult;
    break;
  case "ip":
    this.getIp = this.sendResult;
    break;
  case "md5":
    this.getMd5 = this.sendResult;
    break;
  case "email":
    this.getEmail = this.sendResult;
    break;
  case "url":
    this.getURL = this.sendResult;
    break;
  default:
    console.log(this.section, "- ERROR not loading since unknown type specified in config file", this.type);
    return false;
  }

  this.client = new elasticsearch.Client({
                      host: this.elasticsearch.split(","),
                      keepAlive: true,
                      minSockets: 5,
                      maxSockets: 51
                    });

  api.addSource(section, this);

  this.sourceFields = [this.esResultField];
  for (var k in this.shortcuts) {
    if (this.sourceFields.indexOf(k) === -1)
      this.sourceFields.push(k);
  }
}
util.inherits(ElasticsearchSource, wiseSource);
//////////////////////////////////////////////////////////////////////////////////
ElasticsearchSource.prototype.sendResult = function(key, cb) {
  var query = {
    query: {
      bool: {
        filter: [
          { "range": { }},
          { "exists": {field: this.esResultField}},
          { "term":  { }}
        ]
      }
    },
    sort: {},
    _source: this.sourceFields  // ALW: Need to change to docs_values for ES 5
  };

  query.query.bool.filter[0].range[this.esTimestampField] = {gte: new Date() - this.esMaxTimeMS};
  query.query.bool.filter[2].term[this.esQueryField] = key;
  query.sort[this.esTimestampField] = {order: "desc"};

  // TODO: Should be option to do search vs get
  // TODO: Should be an option to add more then most recent

  this.client.search({index: this.esIndex, body: query}, (err, result) => {
    if (err || result.error || !result.hits || result.hits.hits.length === 0) {
      return cb(null, undefined);
    }
    var json = result.hits.hits[0]._source;
    var key = json[this.esResultField];
    if (key === undefined) {
      return cb(null, undefined);
    }
    var args = [];
    for (var k in this.shortcuts) {
      if (json[k] !== undefined) {
        args.push(this.shortcuts[k]);
        if (Array.isArray(json[k]))
          args.push(json[k][0]);
        else
          args.push(json[k]);
      }
    }
    var newresult = {num: args.length/2 + this.tagsResult.num, buffer: Buffer.concat([wiseSource.encode.apply(null, args), this.tagsResult.buffer])};
    return cb(null, newresult);
  });
};
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  var sections = api.getConfigSections().filter((e) => {return e.match(/^elasticsearch:/);});
  sections.forEach((section) => {
    var source = new ElasticsearchSource(api, section);
  });
};

