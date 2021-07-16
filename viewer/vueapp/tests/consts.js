'use strict';

export const users = [{
  createEnabled: true,
  emailSearch: false,
  enabled: true,
  expression: '',
  headerAuthEnabled: false,
  id: 'testuserid',
  lastUsed: 1624904128143,
  packetSearch: true,
  removeEnabled: false,
  userId: 'testuserid',
  userName: 'testuser',
  webEnabled: true,
  welcomeMsgNum: 1
}, {
  createEnabled: true,
  emailSearch: false,
  enabled: true,
  expression: '',
  headerAuthEnabled: false,
  id: 'testuserid2',
  lastUsed: 1624904128143,
  packetSearch: true,
  removeEnabled: false,
  userId: 'testuserid2',
  userName: 'testuser2',
  webEnabled: true,
  welcomeMsgNum: 1
}];

export const userSettings = {
  timezone: 'gmt',
  ms: false
};

export const userWithSettings = { ...users[0], settings: { ...userSettings } };

export const views = {
  'test view 1': {
    shared: false,
    user: 'testuser',
    expression: 'protocols == tls'
  }
};

export const histories = [{
  api: '/api/users',
  body: { length: 50, sortField: 'userId' },
  id: 'ZhC4Z3oBWTXqilPxr-_r',
  index: 'tests_history_v1-0w0',
  method: 'POST',
  query: 'id=admin',
  queryTime: 5,
  timestamp: 1625237794,
  uiPage: 'users',
  userId: 'admin'
}];

export const files = [{
  filesize: 1836,
  first: 1385474626,
  locked: 1,
  name: '/path/to/thefile.pcap',
  node: 'test',
  num: 1
}, {
  filesize: 8397,
  first: 1385478764,
  locked: 1,
  name: '/path/to/anotherfile.pcap',
  node: 'test',
  num: 2
}];

export const sessions = [{
  'destination.geo.country_iso_code': 'JP',
  'destination.ip': '139.162.123.134',
  'destination.port': 80,
  fileId: [],
  firstPacket: 1593995681271,
  id: '3@200706:JElsqnoB6EjWLLLrts6W',
  ipProtocol: 6,
  lastPacket: 1593995683015,
  'network.bytes': 70573,
  'network.packets': 101,
  node: 'test',
  'source.ip': '10.89.85.15',
  'source.port': 49570,
  totDataBytes: 64897
}];

export const hunts = [{
  created: 1625595118,
  dst: true,
  id: '75sFfXoBagMyHoVvBPRW',
  index: 'tests_hunts_v2',
  lastUpdated: 1625595118,
  matchedSessions: 4,
  name: 'coconut',
  query: { startTime: 0, stopTime: 1625595110 },
  removed: true,
  search: 'coconut',
  searchType: 'ascii',
  searchedSessions: 318,
  size: 10000,
  src: true,
  started: 1625595118,
  status: 'finished',
  totalSessions: 318,
  type: 'raw',
  userId: 'admin',
  users: ['test1']
}, {
  created: 1625595118,
  dst: true,
  id: 'D5scfXoBagMyHoVvG_UQ',
  index: 'tests_hunts_v2',
  lastUpdated: 1625595118,
  matchedSessions: 4,
  name: 'coconut2',
  query: { startTime: 0, stopTime: 1625595110 },
  removed: true,
  search: 'coconut',
  searchType: 'ascii',
  searchedSessions: 318,
  size: 10000,
  src: true,
  started: 1625595118,
  status: 'running',
  totalSessions: 318,
  type: 'raw',
  userId: 'admin',
  users: ['test1']
}, {
  created: 1625595118,
  dst: true,
  id: 'RJu9gnoBagMyHoVvN_xk',
  index: 'tests_hunts_v2',
  lastUpdated: 1625595118,
  matchedSessions: 4,
  name: 'coconut3',
  query: { startTime: 0, stopTime: 1625595110 },
  removed: true,
  search: 'coconut',
  searchType: 'ascii',
  searchedSessions: 318,
  size: 10000,
  src: true,
  started: 1625595118,
  status: 'queued',
  totalSessions: 318,
  type: 'raw',
  userId: 'admin',
  users: ['test1']
}, {
  created: 1625595118,
  dst: true,
  id: 'RpvAgnoBagMyHoVvKPwn',
  index: 'tests_hunts_v2',
  lastUpdated: 1625595118,
  matchedSessions: 4,
  name: 'coconut4',
  query: { startTime: 0, stopTime: 1625595110 },
  removed: true,
  search: 'coconut',
  searchType: 'ascii',
  searchedSessions: 318,
  size: 10000,
  src: true,
  started: 1625595118,
  status: 'queued',
  totalSessions: 318,
  type: 'raw',
  userId: 'admin',
  users: ['test1']
}, {
  created: 1625595118,
  dst: true,
  id: 'SJvCgnoBagMyHoVvivzx',
  index: 'tests_hunts_v2',
  lastUpdated: 1625595118,
  matchedSessions: 4,
  name: 'coconut5',
  query: { startTime: 0, stopTime: 1625595110 },
  removed: true,
  search: 'coconut',
  searchType: 'ascii',
  searchedSessions: 318,
  size: 10000,
  src: true,
  started: 1625595118,
  status: 'paused',
  totalSessions: 318,
  type: 'raw',
  userId: 'admin',
  users: ['test1']
}];

export const fields = [{
  dbField: '_id',
  dbField2: '_id',
  exp: 'id',
  friendlyName: 'Arkime ID',
  group: 'general',
  help: 'Arkime ID for the session',
  noFacet: 'true',
  type: 'termfield'
}, {
  aliases: ['http.host.tokens'],
  dbField: 'http.hostTokens',
  dbField2: 'http.hostTokens',
  exp: 'host.http.tokens',
  friendlyName: 'Hostname Tokens',
  group: 'http',
  help: 'HTTP host Tokens header field',
  transform: 'removeProtocolAndURI',
  type: 'lotextfield'
}, {
  dbField: 'fileand',
  dbField2: 'fileand',
  exp: 'file',
  friendlyName: 'Filename',
  group: 'general',
  help: 'Arkime offline pcap filename',
  type: 'fileand'
}, {
  dbField: 'info',
  exp: 'info',
  group: 'general',
  friendlyName: 'Info',
  help: 'Information'
}, {
  dbField: 'network.bytes',
  dbField2: 'totBytes',
  fieldECS: 'network.bytes',
  exp: 'bytes',
  friendlyName: 'Bytes',
  group: 'general',
  help: 'Total number of raw bytes sent AND received in a session',
  type: 'integer'
}, {
  dbField: 'destination.bytes',
  dbField2: 'dstBytes',
  fieldECS: 'destination.bytes',
  exp: 'bytes.dst',
  friendlyName: 'Dst Bytes',
  group: 'general',
  help: 'Total number of raw bytes sent by destination in a session',
  type: 'integer'
}, {
  dbField: 'source.bytes',
  dbField2: 'srcBytes',
  fieldECS: 'source.bytes',
  exp: 'bytes.src',
  friendlyName: 'Src Bytes',
  group: 'general',
  help: 'Total number of raw bytes sent by source in a session',
  type: 'integer'
}, {
  dbField: 'geoall',
  dbField2: 'geoall',
  exp: 'country',
  friendlyName: 'All country fields',
  group: 'general',
  help: 'Search all country fields',
  regex: '(^country\\.(?:(?!\\.cnt$).)*$|\\.country$)',
  type: 'uptermfield'
}, {
  dbField: 'totDataBytes',
  dbField2: 'totDataBytes',
  exp: 'databytes',
  friendlyName: 'Data bytes',
  group: 'general',
  help: 'Total number of data bytes sent AND received in a session',
  type: 'integer'
}, {
  aliases: ['dns.ip'],
  category: 'ip',
  dbField: 'dns.ip',
  dbField2: 'dns.ip',
  exp: 'ip.dns',
  friendlyName: 'IP',
  group: 'dns',
  help: 'IP from DNS result',
  type: 'ip'
}, {
  dbField: 'http.bodyMagic',
  dbField2: 'http.bodyMagic',
  exp: 'http.bodymagic',
  friendlyName: 'Body Magic',
  group: 'http',
  help: 'The content type of body determined by libfile/magic',
  type: 'termfield'
}, {
  category: ['url', 'host'],
  dbField: 'http.uri',
  dbField2: 'http.uri',
  exp: 'http.uri',
  friendlyName: 'URI',
  group: 'http',
  help: 'URIs for request',
  transform: 'removeProtocol',
  type: 'termfield'
}, {
  category: 'user',
  dbField: 'http.user',
  dbField2: 'http.user',
  exp: 'http.user',
  friendlyName: 'User',
  group: 'http',
  help: 'HTTP Auth User',
  type: 'termfield'
}, {
  dbField: 'ipall',
  dbField2: 'ipall',
  exp: 'ip',
  friendlyName: 'All IP fields',
  group: 'general',
  help: 'Search all ip fields',
  noFacet: 'true',
  type: 'ip'
}, {
  aliases: ['ip.dst:port'],
  category: 'ip',
  dbField: 'destination.ip',
  dbField2: 'dstIp',
  fieldECS: 'destination.ip',
  exp: 'ip.dst',
  friendlyName: 'Dst IP',
  group: 'general',
  help: 'Destination IP',
  portField: 'destination.port',
  portField2: 'dstPort',
  portFieldECS: 'destination.port',
  type: 'ip'
}, {
  category: 'ip',
  dbField: 'source.ip',
  dbField2: 'srcIp',
  fieldECS: 'source.ip',
  exp: 'ip.src',
  friendlyName: 'Src IP',
  group: 'general',
  help: 'Source IP',
  portField: 'source.port',
  portField2: 'srcPort',
  portFieldECS: 'source.port',
  type: 'ip'
}, {
  dbField: 'network.packets',
  dbField2: 'totPackets',
  fieldECS: 'network.packets',
  exp: 'packets',
  friendlyName: 'Packets',
  group: 'general',
  help: 'Total number of packets sent AND received in a session',
  type: 'integer'
}, {
  dbField: 'destination.packets',
  dbField2: 'dstPackets',
  fieldECS: 'destination.packets',
  exp: 'packets.dst',
  friendlyName: 'Dst Packets',
  group: 'general',
  help: 'Total number of packets sent by destination in a session',
  type: 'integer'
}, {
  dbField: 'source.packets',
  dbField2: 'srcPackets',
  fieldECS: 'source.packets',
  exp: 'packets.src',
  friendlyName: 'Src Packets',
  group: 'general',
  help: 'Total number of packets sent by source in a session',
  type: 'integer'
}, {
  dbField: 'portall',
  dbField2: 'portall',
  exp: 'port',
  friendlyName: 'All port fields',
  group: 'general',
  help: 'Search all port fields',
  regex: '(^port\\.(?:(?!\\.cnt$).)*$|\\.port$)',
  type: 'integer'
}, {
  category: 'port',
  dbField: 'destination.port',
  dbField2: 'dstPort',
  fieldECS: 'destination.port',
  exp: 'port.dst',
  friendlyName: 'Dst Port',
  group: 'general',
  help: 'Source Port',
  type: 'integer'
}, {
  category: 'port',
  dbField: 'source.port',
  dbField2: 'srcPort',
  fieldECS: 'source.port',
  exp: 'port.src',
  friendlyName: 'Src Port',
  group: 'general',
  help: 'Source Port',
  type: 'integer'
}, {
  dbField: 'tags',
  dbField2: 'tags',
  exp: 'tags',
  friendlyName: 'Tags',
  group: 'general',
  help: 'Tags set for session',
  type: 'termfield'
}, {
  dbField: 'tagsCnt',
  dbField2: 'tagsCnt',
  exp: 'tags.cnt',
  friendlyName: 'Tags Cnt',
  group: 'general',
  help: 'Unique number of Tags set for session',
  type: 'integer'
}];

export const fieldsMap = {};
for (const field of fields) {
  fieldsMap[field.exp] = field;
}
