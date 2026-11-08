/******************************************************************************/
/* taggerUpload.js -- Upload a file for capture's tagger plugin
 *
 * taggerUpload.js [<config options>] (ip|host|md5|email|uri) <filename> [<tag1>..<tagN>]
 * taggerUpload.js [--insecure] --url <db url> [--prefix <prefix>] (ip|host|md5|email|uri) <filename> [<tag1>..<tagN>]
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';
const fs = require('fs');
const path = require('path');
const cryptoLib = require('crypto');
// Installed it's in plugins/ beside viewer/, in the source tree it's in capture/plugins/
const root = path.join(__dirname, fs.existsSync(path.join(__dirname, '../viewer/config.js')) ? '..' : '../..');
const Config = require(path.join(root, 'viewer/config.js'));
const ArkimeConfig = require(path.join(root, 'common/arkimeConfig'));
const ArkimeDb = require(path.join(root, 'common/arkimeDb'));
const ArkimeUtil = require(path.join(root, 'common/arkimeUtil'));

const TYPES = ['ip', 'host', 'md5', 'email', 'uri'];

function help (msg) {
  if (msg) {
    console.log(`${msg}\n`);
  }
  console.log('taggerUpload.js [<config options>] (ip|host|md5|email|uri) <filename> [<tag1>..<tagN>]');
  console.log('taggerUpload.js [--insecure] --url <db url> [--prefix <prefix>] (ip|host|md5|email|uri) <filename> [<tag1>..<tagN>]');
  console.log('');
  console.log('  Uploads to where capture\'s tagger plugin reads, the OpenSearch/Elasticsearch tagger index');
  console.log('  or with no OpenSearch/Elasticsearch the dbUrl tagger table. The file name is the id, so');
  console.log('  uploading the same file name again replaces it.');
  console.log('');
  console.log('Config Options:');
  console.log('  -c, --config <file|url>   Where to fetch the config file from');
  console.log('  -n <node name>            Node name section to use in config file');
  console.log('  --insecure                Don\'t verify https certificates');
  console.log('');
  console.log('Without a config:');
  console.log('  --url <db url>            An OpenSearch/Elasticsearch, or the dbUrl to use its tagger table');
  console.log('  --prefix <prefix>         The dbUrl tagger table prefix, default arkime_');

  process.exit(msg ? 1 : 0);
}

// The doc taggerUpload.pl made
function taggerDoc (type, file, tags) {
  const elements = [];
  const fields = [];
  for (const line of fs.readFileSync(file, 'utf8').split(/\r?\n/)) {
    if (line.startsWith('#field:')) {
      fields.push(line.slice(1));
    } else if (line !== '' && !line.startsWith('#')) {
      elements.push(line);
    }
  }

  const data = elements.join(',');
  const doc = {};
  if (fields.length) { doc.fields = fields.join(','); }
  if (tags.length) { doc.tags = tags.join(','); }
  doc.md5 = cryptoLib.createHash('md5').update(data).digest('hex');
  doc.type = type;
  doc.data = data;
  return doc;
}

// The unprefixed tagger index
function esCollection (urls, options = {}) {
  const keyword = { type: 'keyword', index: false };
  urls = urls.map(u => ArkimeConfig.esUrl(u));
  const conn = ArkimeDb.connect(urls.join(','), { insecure: ArkimeConfig.isInsecure([urls]), ...options });
  return conn.collection('tagger', { es: { index: 'tagger', create: { mappings: { properties: { tags: keyword, type: keyword, data: keyword, md5: keyword } } } } });
}

function clusterCollection (urls, prefix) {
  if (!ArkimeUtil.dbHoldsCluster(urls[0])) {
    help(`ERROR - '${urls[0].split(':')[0]}' can't hold the tagger table`);
  }
  const conn = ArkimeDb.connect(urls.join(','), { prefix: ArkimeUtil.formatPrefix(prefix), insecure: ArkimeConfig.isInsecure([urls]) });
  return conn.collection('tagger');
}

// capture's tagger reads the sessions OpenSearch/Elasticsearch, else an OpenSearch/Elasticsearch dbUrl, else the dbUrl's tagger table
function configCollection () {
  const sessions = ArkimeConfig.sessionsDbUrls();
  const db = ArkimeConfig.dbUrls();
  const esUrls = ArkimeConfig.isEsUrl(sessions[0]) ? sessions : ArkimeConfig.isEsUrl(db[0]) ? db : undefined;
  if (esUrls) {
    return esCollection(esUrls, { apiKey: Config.get('elasticsearchAPIKey'), basicAuth: Config.get('elasticsearchBasicAuth') });
  }
  return clusterCollection(db, ArkimeConfig.dbPrefix());
}

async function main () {
  const args = process.argv.slice(2);
  const option = (flag) => {
    const i = args.indexOf(flag);
    if (i === -1) { return undefined; }
    if (i + 1 >= args.length) { help(`Missing value for ${flag}`); }
    return args.splice(i, 2)[1];
  };
  let url = option('--url');
  const prefix = option('--prefix');

  const [type, file, ...tags] = args;
  if (!TYPES.includes(type)) { help(`Must be ${TYPES.join(', ')} for file type instead of ${type}`); }
  if (!file) { help('Missing filename'); }
  if (!fs.existsSync(file)) { help(`file '${file}' not found`); }
  if (fs.statSync(file).size === 0) { help(`file '${file}' empty`); }

  let coll;
  if (url) {
    if (!url.includes('://')) { url = `http://${url}`; }
    coll = ArkimeConfig.isEsUrl(url) ? esCollection([url]) : clusterCollection([url], prefix);
  } else {
    await Config.initialize({});
    coll = configCollection();
  }

  await coll.init();
  await coll.put(path.basename(file), taggerDoc(type, file, tags));
  console.log(`Uploaded ${path.basename(file)}`);
  process.exit(0);
}

main().catch((err) => {
  console.log('ERROR -', err.message);
  process.exit(1);
});
