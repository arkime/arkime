/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

The tests' ARKIME_TEST_SESSIONS_HELPER for a mongodb:// sessionsDbUrl, see tests/ArkimeTest.pm
  node testSessions.mjs <url> wipe <prefix>
  node testSessions.mjs <url> post <index> <id> <json>
  node testSessions.mjs <url> deleteByTag <index> <tag>
  node testSessions.mjs <url> find <index> <filter json>, prints the docs without _ip, with their collection as index, sorted by _id
*/
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const { MongoClient } = cjsRequire('mongodb');
const { sessionDoc } = cjsRequire('./mongodb.js');

const [url, cmd, ...args] = process.argv.slice(2);
const client = new MongoClient(url);
const db = client.db();
const escapeRegex = (s) => s.replace(/[\\^$.*+?()[\]{}|/-]/g, '\\$&');

async function collections (pattern) {
  const re = new RegExp(`^${pattern.split('*').map(escapeRegex).join('.*')}$`);
  return (await db.listCollections({}, { nameOnly: true }).toArray()).map(c => c.name).filter(n => re.test(n));
}

try {
  if (cmd === 'wipe') {
    for (const coll of await collections(`${args[0]}sessions3-*`)) { await db.collection(coll).drop(); }
  } else if (cmd === 'post') {
    const [index, id, json] = args;
    const coll = db.collection(index);
    await coll.createIndexes([{ key: { '$**': 1 }, name: 'all', wildcardProjection: { _id: 0 } }, { key: { lastPacket: 1, _id: 1 }, name: 'lastPacketId' }, { key: { firstPacket: 1, _id: 1 }, name: 'firstPacketId' }]);
    await coll.replaceOne({ _id: id }, sessionDoc(id, JSON.parse(json)), { upsert: true });
  } else if (cmd === 'deleteByTag') {
    const [index, tag] = args;
    for (const coll of await collections(index)) { await db.collection(coll).deleteMany({ tags: tag }); }
  } else if (cmd === 'find') {
    const [index, filter] = args;
    const docs = [];
    for (const coll of await collections(index)) { docs.push(...(await db.collection(coll).find(JSON.parse(filter), { projection: { _ip: 0 } }).toArray()).map(d => ({ ...d, index: coll }))); }
    console.log(JSON.stringify(docs.sort((a, b) => String(a._id).localeCompare(String(b._id)))));
  } else {
    throw new Error(`Unknown command ${cmd}`);
  }
} catch (err) {
  console.log(err.message);
  process.exitCode = 1;
} finally {
  await client.close();
}
