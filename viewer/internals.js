'use strict';

const EventEmitter = require('events').EventEmitter;
const http = require('http');
const https = require('https');
const RE2 = require('re2');

module.exports = (app, Config) => {
  const iModule = {};

  // build internals
  iModule.internals = {
    isProduction: app.get('env') === 'production',
    CYBERCHEFVERSION: '9.28.0',
    elasticBase: Config.getArray('elasticsearch', ',', 'http://localhost:9200'),
    remoteClusters: Config.configMap('remote-clusters', 'moloch-clusters'),
    esQueryTimeout: Config.get('elasticsearchTimeout', 300) + 's',
    esScrollTimeout: Config.get('elasticsearchScrollTimeout', 900) + 's',
    userNameHeader: Config.get('userNameHeader'),
    requiredAuthHeader: Config.get('requiredAuthHeader'),
    requiredAuthHeaderVal: Config.get('requiredAuthHeaderVal'),
    userAutoCreateTmpl: Config.get('userAutoCreateTmpl'),
    esAdminUsersSet: Config.get('esAdminUsers', false) !== false,
    esAdminUsers: Config.get('multiES', false) ? [] : Config.getArray('esAdminUsers', ',', ''),
    httpAgent: new http.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 40 }),
    httpsAgent: new https.Agent({ keepAlive: true, keepAliveMsecs: 5000, maxSockets: 40, rejectUnauthorized: !Config.insecure }),
    previousNodesStats: [],
    caTrustCerts: {},
    cronRunning: false,
    rightClicks: {},
    pluginEmitter: new EventEmitter(),
    writers: {},
    oldDBFields: {},
    isLocalViewRegExp: Config.get('isLocalViewRegExp') ? new RE2(Config.get('isLocalViewRegExp')) : undefined,
    uploadLimits: {},
    allowUploads: Config.get('uploadCommand') !== undefined,
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
      createEnabled: false,
      webEnabled: true,
      headerAuthEnabled: false,
      emailSearch: true,
      removeEnabled: true,
      packetSearch: true,
      settings: {},
      welcomeMsgNum: 1,
      found: true
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
      createEnabled: 0,
      webEnabled: 0,
      headerAuthEnabled: 0,
      emailSearch: 0,
      removeEnabled: 0,
      lastUsed: 0
    }
  };

  iModule.internals.scriptAggs['ip.dst:port'] = {
    script: 'if (doc["destination.ip"].value.indexOf(".") > 0) {return doc["destination.ip"].value + ":" + doc["destination.port"].value} else {return doc["destination.ip"].value + "." + doc["destination.port"].value}',
    dbField: 'destination.ip'
  };

  // make sure there's an _ after the prefix
  if (iModule.internals.prefix && !iModule.internals.prefix.endsWith('_')) {
    iModule.internals.prefix = `${iModule.internals.prefix}_`;
  }

  if (Config.get('uploadFileSizeLimit')) {
    iModule.internals.uploadLimits.fileSize = parseInt(Config.get('uploadFileSizeLimit'));
  }

  if (!iModule.internals.elasticBase[0].startsWith('http')) {
    iModule.internals.elasticBase[0] = 'http://' + iModule.internals.elasticBase[0];
  }

  return iModule;
};
