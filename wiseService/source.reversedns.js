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

const Dns = require('dns');
const iptrie = require('iptrie');
const WISESource = require('./wiseSource.js');
let resolver = Dns;

// ----------------------------------------------------------------------------
function removeArray (arr, value) {
  let pos = 0;
  while ((pos = arr.indexOf(value, pos)) !== -1) {
    arr.splice(pos, 1);
  }
  return arr;
}

class ReverseDNSSource extends WISESource {
  // ----------------------------------------------------------------------------
  constructor (api, section) {
    super(api, section, { });
    this.field = api.getConfig('reversedns', 'field');
    this.ips = api.getConfig('reversedns', 'ips');
    this.servers = api.getConfig('reversedns', 'servers');
    if (this.servers !== undefined) {
      resolver = new Dns();
      resolver.setServers(this.servers.split(';'));
    }
    this.stripDomains = removeArray(api.getConfig('reversedns', 'stripDomains', '').split(';').map(item => item.trim()), '');

    if (this.field === undefined) {
      console.log(this.section, '- No field defined');
      return;
    }

    if (this.ips === undefined) {
      console.log(this.section, '- No ips defined');
      return;
    }

    this.theField = this.api.addField(`field:${this.field}`);
    this.trie = new iptrie.IPTrie();
    this.ips.split(';').forEach((item) => {
      if (item === '') {
        return;
      }
      const parts = item.split('/');
      this.trie.add(parts[0], +parts[1] || (parts[0].includes(':') ? 128 : 32), true);
    });

    this.api.addSource('reversedns', this);
  }

  // ----------------------------------------------------------------------------
  getIp (ip, cb) {
    if (!this.trie.find(ip)) {
      return cb(null, undefined);
    }

    resolver.reverse(ip, (err, domains) => {
      // console.log("answer", ip, err, domains);
      if (err || domains.length === 0) {
        return cb(null, WISESource.emptyResult);
      }
      const args = [];
      for (let i = 0; i < domains.length; i++) {
        const domain = domains[i];
        if (this.stripDomains.length === 0) {
          const parts = domain.split('.');
          args.push(this.theField, parts[0].toLowerCase());
        } else {
          for (let j = 0; j < this.stripDomains.length; j++) {
            const stripDomain = this.stripDomains[j];
            if (domain.indexOf(stripDomain, domain.length - stripDomain.length) !== -1) {
              args.push(this.theField, domain.slice(0, domain.length - stripDomain.length));
            }
          }
        }
      }
      const wiseResult = WISESource.encodeResult.apply(null, args);
      cb(null, wiseResult);
    });
  };
}

// ----------------------------------------------------------------------------
exports.initSource = function (api) {
  api.addSourceConfigDef('reversedns', {
    singleton: true,
    name: 'reversedns',
    description: 'For IPs that are included by the ips setting, do a reverse lookup and place everything before the first dot in the field specified',
    types: ['ip'],
    fields: [
      { name: 'field', required: true, help: 'The field to set with the hostname' },
      { name: 'ips', required: true, help: 'Semicolon separated list of IPs or CIDRs to lookups. IPs that don’t match this list will NOT be reverse lookuped' },
      { name: 'servers', required: false, help: 'If set, the reversedns source will use the semicolon separated list of ip addresses to reverse lookuped' },
      { name: 'stripDomains', required: false, help: 'If EMPTY then all domains are stripped after the FIRST period. When set ONLY domains that match the semicolon separated list of domain names are modified, and only the matching part is removed. Those that don’t match will be saved in full. The list is checked in order. A leading dot is recommended' }
    ]
  });

  return new ReverseDNSSource(api, 'reversedns');
};
// ----------------------------------------------------------------------------
