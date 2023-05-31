'use strict';

const Config = require('./config.js');
const EventEmitter = require('events').EventEmitter;
const http = require('http');
const https = require('https');
const RE2 = require('re2');

// build internals
const internals = {
  isProduction: false,
  CYBERCHEFVERSION: '10.4.0',
  elasticBase: Config.getArray('elasticsearch', ',', 'http://localhost:9200'),
  remoteClusters: Config.configMap('remote-clusters', 'moloch-clusters'),
  esQueryTimeout: Config.get('elasticsearchTimeout', 5 * 60) + 's',
  esScrollTimeout: Config.get('elasticsearchScrollTimeout', 5 * 60) + 's',
  userNameHeader: Config.get('userNameHeader'),
  esAdminUsersSet: Config.get('esAdminUsers', false) !== false,
  esAdminUsers: Config.get('multiES', false) ? [] : Config.getArray('esAdminUsers', ',', ''),
  httpAgent: new http.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 40 }),
  httpsAgent: new https.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 40, rejectUnauthorized: !Config.insecure }),
  previousNodesStats: [],
  caTrustCerts: {},
  cronRunning: false,
  rightClicks: {},
  fieldActions: {},
  pluginEmitter: new EventEmitter(),
  writers: {},
  oldDBFields: {},
  isLocalViewRegExp: Config.get('isLocalViewRegExp') ? new RE2(Config.get('isLocalViewRegExp')) : undefined,
  uploadLimits: {},
  allowUploads: !!Config.get('uploadCommand'),
  cronTimeout: +Config.get('dbFlushTimeout', 5) + // How long capture holds items
               60 + // How long before ES reindexs
               20, // Transmit and extra time

  // http://garethrees.org/2007/11/14/pngcrush/
  emptyPNG: Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==', 'base64'),
  PNG_LINE_WIDTH: 256,
  runningHuntJob: undefined,
  proccessHuntJobsInitialized: false,
  notifiers: undefined,
  prefix: Config.get('prefix', 'arkime_'),
  shortcutTypeMap: {
    ip: 'ip',
    integer: 'number',
    termfield: 'string',
    uptermfield: 'string',
    lotermfield: 'string'
  },
  anonymousUser: {
    userId: 'anonymous',
    enabled: true,
    webEnabled: true,
    headerAuthEnabled: false,
    emailSearch: true,
    removeEnabled: true,
    packetSearch: true,
    settings: {},
    welcomeMsgNum: 1,
    found: true,
    roles: ['arkimeUser']
  },
  scriptAggs: {},
  // default settings for users with no settings
  settingDefaults: {
    timezone: 'local',
    detailFormat: 'last',
    showTimestamps: 'last',
    sortColumn: 'firstPacket',
    sortDirection: 'desc',
    spiGraph: 'node',
    connSrcField: 'source.ip',
    connDstField: 'ip.dst:port',
    numPackets: 'last',
    theme: 'default-theme',
    manualQuery: false,
    timelineDataFilters: ['network.packets', 'network.bytes', 'totDataBytes'] // dbField2 values from fields
  },
  usersMissing: {
    userId: '',
    userName: '',
    expression: '',
    enabled: 0,
    webEnabled: 0,
    headerAuthEnabled: 0,
    emailSearch: 0,
    removeEnabled: 0,
    lastUsed: 0
  }
};

internals.scriptAggs['ip.dst:port'] = {
  script: 'if (doc["destination.ip"].value.indexOf(".") > 0) {return doc["destination.ip"].value + ":" + doc["destination.port"].value} else {return doc["destination.ip"].value + "." + doc["destination.port"].value}',
  dbField: 'destination.ip'
};

// make sure there's an _ after the prefix
if (internals.prefix && !internals.prefix.endsWith('_')) {
  internals.prefix = `${internals.prefix}_`;
}

if (Config.get('uploadFileSizeLimit')) {
  internals.uploadLimits.fileSize = parseInt(Config.get('uploadFileSizeLimit'));
}

if (!internals.elasticBase[0].startsWith('http')) {
  internals.elasticBase[0] = 'http://' + internals.elasticBase[0];
}

internals.initialize = (app) => {
  internals.isProduction = app.get('env') === 'production';
};

module.exports = internals;
