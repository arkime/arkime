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

const WISESource = require('./wiseSource.js');
const elasticsearch = require('elasticsearch');

class ElasticsearchSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { typeSetting: true, tagsSetting: true });

    this.esIndex = api.getConfig(section, 'esIndex');
    this.esTimestampField = api.getConfig(section, 'esTimestampField');
    this.esQueryField = api.getConfig(section, 'esQueryField');
    this.esResultField = api.getConfig(section, 'esResultField', 0);
    this.esMaxTimeMS = api.getConfig(section, 'esMaxTimeMS', 60 * 60 * 1000);
    this.elasticsearch = api.getConfig(section, 'elasticsearch');

    ['esIndex', 'esTimestampField', 'esQueryField', 'esResultField', 'elasticsearch'].forEach((item) => {
      if (this[item] === undefined) {
        console.log(this.section, `- ERROR not loading since no ${item} specified in config file`);
      }
    });

    this[this.api.funcName(this.type)] = this.sendResult;

    this.client = new elasticsearch.Client({
      host: this.elasticsearch.split(','),
      keepAlive: true,
      minSockets: 5,
      maxSockets: 51,
      apiVersion: '7.7'
    });

    api.addSource(section, this, [this.type]);

    this.sourceFields = [this.esResultField];
    for (const k in this.shortcuts) {
      if (this.sourceFields.indexOf(k) === -1) {
        this.sourceFields.push(k);
      }
    }
  }

  // ----------------------------------------------------------------------------
  sendResult (key, cb) {
    const query = {
      query: {
        bool: {
          filter: [
            { range: { } },
            { exists: { field: this.esResultField } },
            { term: { } }
          ]
        }
      },
      sort: {},
      _source: this.sourceFields // ALW: Need to change to docs_values for ES 5
    };

    query.query.bool.filter[0].range[this.esTimestampField] = { gte: new Date() - this.esMaxTimeMS };
    query.query.bool.filter[2].term[this.esQueryField] = key;
    query.sort[this.esTimestampField] = { order: 'desc' };

    // TODO: Should be option to do search vs get
    // TODO: Should be an option to add more then most recent

    this.client.search({ index: this.esIndex, body: query }, (err, result) => {
      if (err || result.error || !result.hits || result.hits.hits.length === 0) {
        return cb(null, undefined);
      }
      const json = result.hits.hits[0]._source;
      const key = json[this.esResultField];
      if (key === undefined) {
        return cb(null, undefined);
      }
      const args = [];
      for (const k in this.shortcuts) {
        if (json[k] !== undefined) {
          args.push(this.shortcuts[k].pos);
          if (Array.isArray(json[k])) {
            args.push(json[k][0]);
          } else {
            args.push(json[k]);
          }
        }
      }
      const newresult = WISESource.combineResults([WISESource.encodeResult.apply(null, args), this.tagsResult]);
      return cb(null, newresult);
    });
  };
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('elasticsearch', {
    singleton: false,
    name: 'elasticsearch',
    description: 'Link to the elasticsearch data',
    fields: [
      { name: 'type', required: true, help: 'The wise type of this source' },
      { name: 'tag', required: true, help: 'The tags to set on matches' },
      { name: 'esQueryField', required: true, help: 'The elasticsearch field in each document that is being queried' },
      { name: 'elasticsearch', required: true, help: 'Elasticsearch base url' },
      { name: 'esIndex', required: true, help: 'The index pattern to look at' },
      { name: 'esTimestampField', required: true, help: 'The field to use in queries that has the timestamp in ms' },
      { name: 'esMaxTimeMS', required: false, help: 'Timestamp field must be less then this (default: 1hr)' },
      { name: 'esResultField', required: true, help: 'Field that is required to be in the result' }
    ]
  });

  const sections = api.getConfigSections().filter((e) => { return e.match(/^elasticsearch:/); });
  sections.forEach((section) => {
    return new ElasticsearchSource(api, section);
  });
};
