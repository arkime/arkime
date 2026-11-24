/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for writing only some fields of a user, against a SQLite users db.
Run with `npm run test:unit`.
*/
import { test, describe, before } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const cjsRequire = createRequire(import.meta.url);
const User = cjsRequire('../common/user.js');
const Auth = cjsRequire('../common/auth.js');
const ArkimeDb = cjsRequire('../common/arkimeDb.js');
const ArkimeConfig = cjsRequire('../common/arkimeConfig.js');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'user-unit-'));

const getUser = (userId) => new Promise((resolve, reject) => {
  User.getUser(userId, (err, user) => err ? reject(err) : resolve(user));
});
const stored = async (userId) => (await User.getAllUsers()).find(u => u.userId === userId);

describe('user field updates', () => {
  before(async () => {
    const file = path.join(tmp, 'config.ini');
    fs.writeFileSync(file, '[default]\n');
    await ArkimeConfig.initialize({ defaultConfigFile: file, defaultSections: 'default' });
    User.initialize({ url: `sqlite://${tmp}/users.db`, noUsersCheck: true });
    await User.setUser('bob', { userId: 'bob', userName: 'Bob', enabled: true, expression: 'port == 80', emailSearch: true, roles: ['arkimeUser'], settings: { theme: 'dark', logo: 'x' } });
  });

  test('UNSET_FIELD is the collection UNSET', () => {
    assert.equal(User.UNSET_FIELD, ArkimeDb.UNSET);
  });

  test('undefined leaves a field alone and UNSET_FIELD removes it', async () => {
    assert.equal(await User.updateUser('bob', { enabled: false, expression: undefined, emailSearch: User.UNSET_FIELD }), true);
    const doc = await stored('bob');
    assert.equal(doc.enabled, false);
    assert.equal(doc.expression, 'port == 80');
    assert.equal('emailSearch' in doc, false);
  });

  test('an object field is replaced whole', async () => {
    await User.updateUser('bob', { settings: { theme: 'light' } });
    assert.deepEqual((await stored('bob')).settings, { theme: 'light' });
  });

  test('roles keep createEnabled in step', async () => {
    await User.updateUser('bob', { roles: ['arkimeUser', 'usersAdmin'] });
    assert.equal((await stored('bob')).createEnabled, true);
    await User.updateUser('bob', { roles: ['arkimeUser'] });
    assert.equal((await stored('bob')).createEnabled, false);
    await User.updateUser('bob', { createEnabled: true });
    await User.updateUser('bob', { roles: User.UNSET_FIELD });
    const doc = await stored('bob');
    assert.equal('roles' in doc, false);
    assert.equal('createEnabled' in doc, false, 'a stale createEnabled would bring back usersAdmin');
    await User.updateUser('bob', { roles: ['arkimeUser'] });
  });

  test('saveFields sets the object and the db', async () => {
    const user = await getUser('bob');
    assert.equal(await User.saveFields(user, { userName: 'Robert', expression: User.UNSET_FIELD, hidePcap: undefined }), true);
    assert.equal(user.userName, 'Robert');
    assert.equal('expression' in user, false);
    const doc = await stored('bob');
    assert.equal(doc.userName, 'Robert');
    assert.equal('expression' in doc, false);
    assert.equal('hidePcap' in doc, false);
  });

  test('saveFields never recreates a deleted user', async () => {
    await User.setUser('gone', { userId: 'gone', userName: 'Gone', enabled: true });
    const user = await getUser('gone');
    await User.deleteUser('gone');
    assert.equal(await User.saveFields(user, { enabled: false }), false);
    assert.equal(await stored('gone'), undefined);

    const mode = Auth.mode;
    Auth.mode = 'anonymous';
    try {
      assert.equal(await User.saveFields(user, { enabled: false }), false, 'only the anonymous user is saved whole');
      assert.equal(await stored('gone'), undefined);

      const anon = Object.assign(new User(), { userId: 'anonymous', enabled: true, settings: {} });
      assert.equal(await User.saveFields(anon, { settings: { theme: 'dark' } }), true);
      assert.deepEqual((await stored('anonymous')).settings, { theme: 'dark' });
    } finally {
      Auth.mode = mode;
    }
  });
});
