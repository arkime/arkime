/******************************************************************************/
/*
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Dns = require('dns');
const iptrie = require('iptrie');
const WISESource = require('./wiseSource.js');
let resolver;

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
    resolver = new Dns.Resolver({ timeout: 2000 });
    if (this.servers !== undefined) {
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

    this.api.addSource('reversedns', this, ['ip']);
  }

  // ----------------------------------------------------------------------------
  getIp (ip, cb) {
    if (!this.trie.find(ip)) {
      return cb(null, undefined);
    }

    resolver.reverse(ip, (err, domains) => {
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
    link: 'https://arkime.com/wise#reversedns',
    types: ['ip'],
    fields: [
      { name: 'field', required: true, help: 'The field to set with the hostname' },
      { name: 'ips', required: true, multiline: ';', help: 'List of IPs or CIDRs that WISE will attempt to reverse lookup. IPs that don’t match this list will NOT be reverse lookup' },
      { name: 'servers', required: false, multiline: ';', help: 'List of ip addresses to use as the resolver. Default is to just use the OS configuration.' },
      { name: 'stripDomains', required: false, multiline: ';', help: 'If EMPTY then all domains are stripped after the FIRST period. When set ONLY domains that match the list of domain names are modified, and only the matching part is removed. Those that don’t match will be saved in full. The list is checked in order. A leading dot is recommended.' }
    ]
  });

  return new ReverseDNSSource(api, 'reversedns');
};
// ----------------------------------------------------------------------------
