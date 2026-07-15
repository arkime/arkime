#!/usr/bin/env node
/******************************************************************************/
/* changePasswordSecret.js -- Re-encrypt every user's stored password and
 *   cont3xt keys after the passwordSecret has been changed.
 *
 * Prompts for the OLD and NEW passwordSecret, then walks every document in the
 * users index. For each user it decrypts the stored password (passStore) and
 * cont3xt keys with the old secret and re-encrypts them with the new secret,
 * keeping the same document _id.
 *
 *   - If a user's passStore can't be decrypted with the old secret it is logged
 *     and left completely unchanged.
 *   - If a user is re-encrypted it is logged.
 *
 * This lives in JavaScript (not db.pl) because the encryption schemes
 * (aes-256-cbc for passStore, aes-256-gcm + pbkdf2 for cont3xt keys) reuse the
 * exact crypto in common/auth.js and can't be done in Perl without new deps.
 *
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const cryptoLib = require('crypto');
const readline = require('readline');
const version = require('./version');
const ArkimeConfig = require('./arkimeConfig');
const Auth = require('./auth');
const User = require('./user');

// ----------------------------------------------------------------------------
function help (msg) {
  if (msg) { console.log(`${msg}\n`); }
  console.log('changePasswordSecret.js [<config options>] [<oldSecret> <newSecret>]');
  console.log('');
  console.log('Re-encrypts every user\'s stored password and cont3xt keys in the users');
  console.log('index after you change the passwordSecret. Stop all Arkime processes,');
  console.log('update passwordSecret in the config, then run this once against the users index.');
  console.log('');
  console.log('Give the old and new passwordSecret as arguments, or omit them to be');
  console.log('prompted (prompting keeps the secrets out of the process list).');
  console.log('');
  console.log('Config Options:');
  console.log('  -c, --config <file|url>     Where to fetch the config file from');
  console.log('  -o <section>.<key>=<value>  Override the config file');
  console.log('  --insecure                  Disable certificate verification for https calls');
  process.exit(msg ? 1 : 0);
}

// ----------------------------------------------------------------------------
// Prompt for a secret without echoing the typed characters
function promptHidden (label) {
  return new Promise((resolve) => {
    const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
    rl._writeToOutput = (s) => { if (s === label) { process.stdout.write(s); } };
    rl.question(label, (answer) => {
      rl.close();
      console.log();
      resolve(answer);
    });
  });
}

// ----------------------------------------------------------------------------
// passStore crypto, matching Auth.store2ha1 / Auth.ha12store:
//   aes-256-cbc, key = sha256(secret), stored as "<iv hex>.<encrypted hex>"
function decryptPassStore (passStore, key256) {
  const parts = passStore.split('.');
  if (parts.length !== 2) { throw new Error('unsupported passStore format'); }
  const c = cryptoLib.createDecipheriv('aes-256-cbc', key256, Buffer.from(parts[0], 'hex'));
  let d = c.update(parts[1], 'hex', 'binary');
  d += c.final('binary');
  return d;
}

function encryptPassStore (ha1, key256) {
  const iv = cryptoLib.randomBytes(16);
  const c = cryptoLib.createCipheriv('aes-256-cbc', key256, iv);
  let e = c.update(ha1, 'binary', 'hex');
  e += c.final('hex');
  return iv.toString('hex') + '.' + e;
}

// ----------------------------------------------------------------------------
async function run (oldArg, newArg) {
  const oldSecret = oldArg ?? await promptHidden('Old passwordSecret: ');
  const newSecret = newArg ?? await promptHidden('New passwordSecret: ');

  if (oldSecret === '') { help('Old passwordSecret must not be empty'); }
  if (newSecret === '') { help('New passwordSecret must not be empty'); }
  if (oldSecret === newSecret) { help('Old and new passwordSecret are the same, nothing to do'); }

  // sha256(secret) is both the aes-256-cbc key for passStore and the pbkdf2
  // secret used by Auth.obj2auth/auth2obj for cont3xt keys.
  const oldKey = cryptoLib.createHash('sha256').update(oldSecret).digest();
  const newKey = cryptoLib.createHash('sha256').update(newSecret).digest();

  const users = await User.getAllUsers();

  let scanned = 0;
  let updated = 0;
  let skipped = 0;

  for (const user of users) {
    scanned++;
    const userId = user.userId;
    const changed = [];

    // passStore is the gate: if present but it won't decrypt with the old
    // secret, don't touch this user at all.
    if (typeof user.passStore === 'string' && user.passStore.length > 0) {
      let ha1;
      try {
        ha1 = decryptPassStore(user.passStore, oldKey);
      } catch (e) {
        console.log(`SKIP   - ${userId}: passStore could not be decrypted with the old passwordSecret, leaving unchanged`);
        skipped++;
        continue;
      }
      user.passStore = encryptPassStore(ha1, newKey);
      changed.push('passStore');
    }

    // cont3xt keys, encrypted with Auth.obj2auth(secret = sha256(passwordSecret))
    if (typeof user.cont3xt?.keys === 'string' && user.cont3xt.keys.length > 0) {
      try {
        const obj = Auth.auth2obj(user.cont3xt.keys, oldKey);
        user.cont3xt.keys = Auth.obj2auth(obj, newKey);
        changed.push('cont3xt keys');
      } catch (e) {
        if (changed.length > 0) {
          console.log(`WARN   - ${userId}: passStore re-encrypted but cont3xt keys could not be decrypted, leaving cont3xt keys unchanged`);
        } else {
          console.log(`SKIP   - ${userId}: cont3xt keys could not be decrypted with the old passwordSecret, leaving unchanged`);
          skipped++;
          continue;
        }
      }
    }

    // Nothing encrypted to migrate (e.g. a role or header-only user)
    if (changed.length === 0) { continue; }

    await User.setUser(userId, user);
    console.log(`UPDATE - ${userId}: re-encrypted ${changed.join(', ')}`);
    updated++;
  }

  await User.flush();

  console.log(`\nDone. Scanned ${scanned} users, updated ${updated}, skipped ${skipped}.`);
  process.exit(0);
}

// ----------------------------------------------------------------------------
// Parse -o overrides and --help. ArkimeConfig has already stripped -c/--insecure
// from process.argv, so what remains here is our -o pairs and the optional
// positional <oldSecret> <newSecret>.
function processArgs () {
  const positional = [];
  for (let i = 2; i < process.argv.length; i++) {
    if (process.argv[i] === '-o' || process.argv[i] === '--option') {
      i++;
      const kv = process.argv[i] ?? '';
      const equal = kv.indexOf('=');
      if (equal === -1) { help(`Missing equal sign in ${kv}`); }
      const key = kv.slice(0, equal);
      const value = kv.slice(equal + 1);
      // dotless keys go into the default section, matching viewer/config.js
      ArkimeConfig.setOverride(key.includes('.') ? key : `default.${key}`, value);
    } else if (process.argv[i] === '--help' || process.argv[i] === '-h') {
      help();
    } else {
      positional.push(process.argv[i]);
    }
  }

  if (positional.length === 1 || positional.length > 2) {
    help('Provide both <oldSecret> and <newSecret>, or neither to be prompted');
  }

  return positional;
}

// ----------------------------------------------------------------------------
async function premain () {
  const [oldSecret, newSecret] = processArgs();

  await ArkimeConfig.initialize({
    defaultConfigFile: `${version.config_prefix}/etc/config.ini`,
    defaultSections: 'default'
  });

  const es = ArkimeConfig.getArray('elasticsearch', 'http://localhost:9200');
  const usersUrl = ArkimeConfig.get('usersUrl');
  const usersEs = ArkimeConfig.getArray('usersElasticsearch') ?? es;

  // Match viewer/db.js: users prefix is usersPrefix falling back to the main prefix.
  const usersPrefix = ArkimeConfig.get('usersPrefix') ?? ArkimeConfig.get('prefix', 'arkime');

  await Auth.initialize({ passwordSecretSection: 'default' });

  User.initialize({
    insecure: ArkimeConfig.isInsecure([usersUrl, usersEs]),
    requestTimeout: ArkimeConfig.get('elasticsearchTimeout', 300),
    url: usersUrl,
    node: usersEs,
    caTrustFile: ArkimeConfig.get('caTrustFile'),
    clientKey: ArkimeConfig.get('esClientKey'),
    clientCert: ArkimeConfig.get('esClientCert'),
    clientKeyPass: ArkimeConfig.get('esClientKeyPass'),
    prefix: usersPrefix,
    apiKey: ArkimeConfig.get('usersElasticsearchAPIKey'),
    basicAuth: ArkimeConfig.get('usersElasticsearchBasicAuth', ArkimeConfig.get('elasticsearchBasicAuth')),
    noUsersCheck: true
  });

  await run(oldSecret, newSecret);
}

premain();
