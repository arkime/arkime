/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the notifme notifiers (email, slack, twilio), the Teams card
builder, and the Pushover notifier. Run with `npm run test:unit`.

The alert message can carry strings the sender doesn't control (hunt and
periodic query names, what monitored clusters report to Parliament), so the
channels that render markup must not let them inject any. The notifme sdk is
stubbed so nothing is sent.
*/
import { test, describe, beforeEach, afterEach } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import nodeCrypto from 'node:crypto';
import zlib from 'node:zlib';

const cjsRequire = createRequire(import.meta.url);
const notifme = cjsRequire('../common/notifier.notifme.js');
const teams = cjsRequire('../common/notifier.teams.js');
const pushover = cjsRequire('../common/notifier.pushover.js');
// resolve the sdk the same way notifier.notifme.js does, so we stub that copy
const Notifme = createRequire(cjsRequire.resolve('../common/notifier.notifme.js'))('arkime-notifme-sdk');

let sent;
Notifme.default = class {
  send (payload) {
    sent.push(payload);
    return Promise.resolve({ status: 'success' });
  }
};

// send one alert and wait for the callback
function send (fn, config, message, links) {
  return new Promise((resolve) => fn(config, message, links, resolve));
}

const EMAIL = { host: 'localhost', port: 25, to: 'admin@example.com', from: 'arkime@example.com' };
const SLACK = { slackWebhookUrl: 'https://hooks.slack.com/services/x' };
const TWILIO = { accountSid: 'x', authToken: 'x', toNumber: '+15550000000', fromNumber: '+15550000001' };

// a message from a sender that didn't escape a value of the attacker's choosing
const UNESCAPED = 'cluster1 - node <a href="http://evil.example/">ack</a><img src=http://evil.example/p.gif> is down';

describe('notifier.notifme', () => {
  beforeEach(() => { sent = []; });

  test('email escapes markup in the message', async () => {
    await send(notifme.sendEmailAlert, EMAIL, UNESCAPED);
    assert.equal(sent[0].email.html, 'cluster1 - node &lt;a href="http://evil.example/"&gt;ack&lt;/a&gt;&lt;img src=http://evil.example/p.gif&gt; is down');
  });

  test('email still renders the links it adds', async () => {
    await send(notifme.sendEmailAlert, EMAIL, 'cluster <b> - esDown', [{ text: 'Parliament Dashboard', url: 'https://parliament.example/?searchTerm=c1' }]);
    assert.equal(sent[0].email.html, 'cluster &lt;b&gt; - esDown<br><a href="https://parliament.example/?searchTerm=c1">Parliament Dashboard</a>');
  });

  test('email escapes the link text and quotes in the link url', async () => {
    await send(notifme.sendEmailAlert, EMAIL, 'cluster - esDown', [{ text: 'Dashboard <b>', url: 'https://parliament.example/?q=" onclick="x' }]);
    assert.equal(sent[0].email.html, 'cluster - esDown<br><a href="https://parliament.example/?q=&quot; onclick=&quot;x">Dashboard &lt;b&gt;</a>');
  });

  test('email does not double escape an already safeStr\'d message', async () => {
    const message = 'A&amp;B &lt;x&gt; hunt job finished: https://arkime.example/sessions?expression=huntId==1&stopTime=2&startTime=1';
    await send(notifme.sendEmailAlert, EMAIL, message);
    assert.equal(sent[0].email.html, message);
  });

  test('slack escapes links and @mentions in the message', async () => {
    await send(notifme.sendSlackAlert, SLACK, 'cluster1 - node <!channel> <http://evil.example/|click to ack> is down');
    assert.equal(sent[0].slack.text, 'cluster1 - node &lt;!channel&gt; &lt;http://evil.example/|click to ack&gt; is down');
  });

  test('twilio sms is plain text and left as is', async () => {
    await send(notifme.sendTwilioAlert, TWILIO, UNESCAPED);
    assert.equal(sent[0].sms.text, UNESCAPED);
  });
});

describe('notifier.teams', () => {
  // the TextBlock text of the card Teams is sent
  function teamsText (message) {
    return teams.buildTeamsMessage(message).attachments[0].content.body[0].text;
  }

  test('replaces Markdown link brackets in the message', () => {
    assert.equal(
      teamsText('cluster1 - node [Click to ack](http://evil.example/) is down'),
      'cluster1 - node ［Click to ack］(http://evil.example/) is down'
    );
    assert.equal(teamsText('![x](http://evil.example/p.gif)'), '!［x］(http://evil.example/p.gif)');
  });

  test('a backslash in the message cannot bring the brackets back', () => {
    assert.equal(teamsText('\\[x\\](http://evil.example/)'), '\\［x\\］(http://evil.example/)');
  });

  test('hunt and cron *emphasis* and urls are left as is', () => {
    const message = `
*A&amp;B* hunt job finished:
*5* matched sessions out of *10* searched sessions.
https://arkime.example/sessions?expression=huntId==1&stopTime=2&startTime=1
`;
    assert.equal(teamsText(message), message);
  });

  test('links become card actions unchanged', () => {
    const card = teams.buildTeamsMessage('cluster - esDown', [{ text: 'Parliament Dashboard', url: 'https://parliament.example/?searchTerm=c1' }]).attachments[0].content;
    assert.equal(card.body[0].text, 'cluster - esDown');
    assert.deepEqual(card.actions, [{ type: 'Action.OpenUrl', title: 'Parliament Dashboard', url: 'https://parliament.example/?searchTerm=c1' }]);
  });
});

// Stub fetch for the notifiers that POST to a web API, so nothing is sent.
// reply() is called with each request and returns the Response, or throws.
function stubFetch () {
  const stub = { requests: [], realFetch: globalThis.fetch };
  stub.reply = () => new Response(JSON.stringify({ status: 1, request: 'r1' }), { status: 200 });
  globalThis.fetch = async (url, options) => {
    stub.requests.push({ url, options });
    return stub.reply(url, options);
  };
  stub.restore = () => { globalThis.fetch = stub.realFetch; };
  return stub;
}

function timeoutError () {
  return new DOMException('The operation was aborted due to timeout', 'TimeoutError');
}

describe('notifier.teams sendTeamsAlert', () => {
  const TEAMS = { teamsWebhookUrl: 'https://example.webhook.office.com/x' };
  let stub;
  beforeEach(() => { stub = stubFetch(); });
  afterEach(() => { stub.restore(); });

  test('posts the card as json to the webhook', async () => {
    const response = await send(teams.sendTeamsAlert, TEAMS, 'cluster - esDown');
    assert.deepEqual(response, {});
    assert.equal(stub.requests[0].url, TEAMS.teamsWebhookUrl);
    assert.equal(stub.requests[0].options.method, 'POST');
    assert.equal(stub.requests[0].options.headers['Content-Type'], 'application/json');
    assert.equal(JSON.parse(stub.requests[0].options.body).attachments[0].content.body[0].text, 'cluster - esDown');
  });

  test('reports a rejected post with the response text', async () => {
    stub.reply = () => new Response('bad card', { status: 400 });
    const response = await send(teams.sendTeamsAlert, TEAMS, 'hi');
    assert.deepEqual(response, { errors: { teams: 'Teams webhook returned 400: bad card' } });
  });

  test('reports a timeout', async () => {
    stub.reply = () => { throw timeoutError(); };
    const response = await send(teams.sendTeamsAlert, TEAMS, 'hi');
    assert.deepEqual(response, { errors: { teams: 'Teams webhook request timed out' } });
  });
});

describe('notifier.pushover', () => {
  const PUSHOVER = { appToken: 'azGDORePK8gMaC0QOYAMyEEuzJnyUi', userKey: 'uQiRzpo4DXghDmr9QzzfQu27cmVRsG' };
  const KEY = '4f3c2a1b0e9d8c7b6a5f4e3d2c1b0a9f8e7d6c5b4a39281706f5e4d3c2b1a090';

  // undo pushover.encryptField the way the Pushover apps do, checking the HMAC first
  function decryptField (keyHex, field) {
    const key = Buffer.from(keyHex, 'hex');
    const buf = Buffer.from(field, 'base64');
    const iv = buf.subarray(0, 16);
    const ciphertext = buf.subarray(16, buf.length - 32);
    const hmac = buf.subarray(buf.length - 32);
    const expected = nodeCrypto.createHmac('sha256', key).update(iv).update(ciphertext).digest();
    assert.ok(nodeCrypto.timingSafeEqual(hmac, expected), 'hmac matches');
    const decipher = nodeCrypto.createDecipheriv('aes-256-cbc', key, iv);
    return zlib.gunzipSync(Buffer.concat([decipher.update(ciphertext), decipher.final()])).toString('utf8');
  }

  // poorly compressible text, so encrypting it doesn't shrink it much
  function noise (len) {
    return nodeCrypto.randomBytes(len).toString('base64').slice(0, len);
  }

  test('sends the message as plain text with only the required params by default', () => {
    const params = pushover.buildPushoverParams(PUSHOVER, 'cluster1 - node <b>x</b> is down', null);
    assert.deepEqual(params, {
      token: PUSHOVER.appToken,
      user: PUSHOVER.userKey,
      message: 'cluster1 - node <b>x</b> is down'
    });
  });

  test('undoes the safeStr escaping parliament applies to the whole message', () => {
    const params = pushover.buildPushoverParams(PUSHOVER, 'Bob&#39;s lab&#47;east &amp; &lt;b&gt; &quot;x&quot; - esDown');
    assert.equal(params.message, 'Bob\'s lab/east & <b> "x" - esDown');
  });

  test('decodes in one pass so double escaped text stays escaped once', () => {
    assert.equal(pushover.plainText('&amp;lt;b&amp;gt;'), '&lt;b&gt;');
  });

  test('hunt and cron messages lose their *emphasis* and surrounding blank lines', () => {
    const hunt = `
*A&amp;B* hunt job finished:
*5* matched sessions out of *10* searched sessions.
https://arkime.example/sessions?expression=huntId==1&stopTime=2&startTime=1
          `;
    assert.equal(pushover.plainText(hunt), `A&B hunt job finished:
5 matched sessions out of 10 searched sessions.
https://arkime.example/sessions?expression=huntId==1&stopTime=2&startTime=1`);

    const paused = '\n*h1* hunt job paused with error: *ES down*\n';
    assert.equal(pushover.plainText(paused), 'h1 hunt job paused with error: ES down');

    const cron = '\n*q1* periodic query match alert:\n*3 new* matches\n*7 total* matches\n';
    assert.equal(pushover.plainText(cron), 'q1 periodic query match alert:\n3 new matches\n7 total matches');
  });

  test('asterisks that are not emphasis are left alone', () => {
    const url = 'https://arkime.example/sessions?expression=host.dns%20%3D%3D%20*.foo.*';
    assert.equal(pushover.plainText(url), url);
    assert.equal(pushover.plainText('*a * b* hunt job finished'), '*a * b* hunt job finished');
    assert.equal(pushover.plainText('*** hunt'), '*** hunt');
  });

  test('optional fields are trimmed and blank ones left out', () => {
    const params = pushover.buildPushoverParams({ ...PUSHOVER, userKey: ` ${PUSHOVER.userKey} `, device: 'iphone, pixel', sound: ' siren ', title: 'Arkime', priority: '1', retry: '', expire: '  ' }, 'hi');
    assert.equal(params.user, PUSHOVER.userKey);
    assert.equal(params.device, 'iphone,pixel');
    assert.equal(params.sound, 'siren');
    assert.equal(params.title, 'Arkime');
    assert.equal(params.priority, '1');
    assert.equal(params.retry, undefined);
    assert.equal(params.expire, undefined);
  });

  test('emergency priority gets retry, and expire defaults to as long as Pushover allows', () => {
    let params = pushover.buildPushoverParams({ ...PUSHOVER, priority: '2' }, 'hi');
    assert.equal(params.retry, '60');
    assert.equal(params.expire, '3000');
    params = pushover.buildPushoverParams({ ...PUSHOVER, priority: '2', retry: '120' }, 'hi');
    assert.equal(params.expire, '6000');
    params = pushover.buildPushoverParams({ ...PUSHOVER, priority: '2', retry: '300' }, 'hi');
    assert.equal(params.expire, '10800');
    params = pushover.buildPushoverParams({ ...PUSHOVER, priority: '2', retry: '120', expire: '600' }, 'hi');
    assert.equal(params.retry, '120');
    assert.equal(params.expire, '600');
  });

  test('the first link becomes the url and the rest go in the message', () => {
    const params = pushover.buildPushoverParams(PUSHOVER, 'cluster - esDown', [
      { text: 'Parliament Dashboard', url: 'https://parliament.example/?searchTerm=c1' },
      { text: 'Other', url: 'https://other.example/' }
    ]);
    assert.equal(params.url, 'https://parliament.example/?searchTerm=c1');
    assert.equal(params.url_title, 'Parliament Dashboard');
    assert.equal(params.message, 'cluster - esDown\nOther: https://other.example/');
  });

  test('a link too long for the url param goes in the message', () => {
    const long = 'https://parliament.example/?q=' + 'a'.repeat(600);
    const params = pushover.buildPushoverParams(PUSHOVER, 'msg', [{ text: 'Dashboard', url: long }]);
    assert.equal(params.url, undefined);
    assert.equal(params.message, `msg\nDashboard: ${long}`);
  });

  test('long messages and titles are cut to the Pushover limits without splitting a character', () => {
    const params = pushover.buildPushoverParams({ ...PUSHOVER, title: 't'.repeat(300) }, '😀'.repeat(2000));
    assert.equal(Array.from(params.message).length, 1024);
    assert.ok(params.message.endsWith('😀…'));
    assert.equal(params.title, 't'.repeat(249) + '…');
  });

  test('a long message is cut before the links in it, not through them', () => {
    const long = 'https://parliament.example/?q=' + 'a'.repeat(600);
    const params = pushover.buildPushoverParams(PUSHOVER, 'x'.repeat(2000), [{ text: 'Dashboard', url: long }]);
    const suffix = `\nDashboard: ${long}`;
    assert.equal(Array.from(params.message).length, 1024);
    assert.ok(params.message.endsWith('x…' + suffix));
  });

  test('a link longer than the whole message limit is cut too', () => {
    const huge = 'https://parliament.example/?q=' + 'a'.repeat(1100);
    const params = pushover.buildPushoverParams(PUSHOVER, 'msg', [{ text: 'Dashboard', url: huge }]);
    assert.equal(Array.from(params.message).length, 1024);
    assert.ok(params.message.startsWith('msg\nDashboard: https://parliament.example/'));
    assert.ok(params.message.endsWith('a…'));
  });

  test('end-to-end encryption encrypts the message and title, and links go in the message', () => {
    const params = pushover.buildPushoverParams({ ...PUSHOVER, title: 'Arkime', priority: '1', encryptionKey: KEY }, 'cluster1&#39;s node is down', [{ text: 'Dashboard', url: 'https://parliament.example/' }]);
    assert.equal(params.encrypted, '1');
    assert.equal(params.token, PUSHOVER.appToken);
    assert.equal(params.user, PUSHOVER.userKey);
    assert.equal(params.priority, '1');
    assert.equal(params.url, undefined);
    assert.equal(params.url_title, undefined);
    assert.equal(decryptField(KEY, params.message), 'cluster1\'s node is down\nDashboard: https://parliament.example/');
    assert.equal(decryptField(KEY, params.title), 'Arkime');
  });

  test('encrypted message and title fit the Pushover limits, keeping the links whole', () => {
    const suffix = '\nDashboard: https://parliament.example/';
    const params = pushover.buildPushoverParams({ ...PUSHOVER, title: noise(250), encryptionKey: KEY }, noise(1024), [{ text: 'Dashboard', url: 'https://parliament.example/' }]);
    assert.ok(params.message.length <= 1024, `encrypted message is ${params.message.length}`);
    assert.ok(params.title.length <= 250, `encrypted title is ${params.title.length}`);
    const message = decryptField(KEY, params.message);
    assert.ok(message.endsWith('…' + suffix));
    assert.ok(message.length > 500, `kept ${message.length} characters`);
    assert.ok(decryptField(KEY, params.title).endsWith('…'));
  });

  test('a short encrypted message is not cut', () => {
    const params = pushover.buildPushoverParams({ ...PUSHOVER, encryptionKey: KEY }, 'cluster1 is down');
    assert.equal(decryptField(KEY, params.message), 'cluster1 is down');
  });

  test('each encryption uses a new IV', () => {
    assert.notEqual(pushover.encryptField(KEY, 'same'), pushover.encryptField(KEY, 'same'));
  });

  test('validate refuses settings Pushover would reject', () => {
    assert.equal(pushover.validate(PUSHOVER), undefined);
    assert.equal(pushover.validate({ ...PUSHOVER, priority: '2', retry: '30', expire: '1500', encryptionKey: KEY.toUpperCase() }), undefined);
    assert.equal(pushover.validate({ ...PUSHOVER, priority: '3' }), 'Priority must be -2, -1, 0, 1, or 2');
    assert.equal(pushover.validate({ ...PUSHOVER, priority: 'high' }), 'Priority must be -2, -1, 0, 1, or 2');
    assert.equal(pushover.validate({ ...PUSHOVER, priority: true }), 'priority must be text');
    assert.match(pushover.validate({ ...PUSHOVER, retry: '29' }), /^Retry must be/);
    assert.match(pushover.validate({ ...PUSHOVER, retry: '30s' }), /^Retry must be/);
    assert.match(pushover.validate({ ...PUSHOVER, expire: '10801' }), /^Expire must be/);
    assert.match(pushover.validate({ ...PUSHOVER, expire: '0' }), /^Expire must be/);
    assert.equal(pushover.validate({ ...PUSHOVER, encryptionKey: KEY.slice(1) }), 'Encryption key must be 64 hex characters');
    assert.equal(pushover.validate({ ...PUSHOVER, encryptionKey: 'g' + KEY.slice(1) }), 'Encryption key must be 64 hex characters');
  });

  test('validate refuses blank or non text tokens', () => {
    assert.equal(pushover.validate({ userKey: PUSHOVER.userKey }), 'Missing a value for appToken');
    assert.equal(pushover.validate({ ...PUSHOVER, appToken: '   ' }), 'Missing a value for appToken');
    assert.equal(pushover.validate({ ...PUSHOVER, userKey: '\t' }), 'Missing a value for userKey');
    assert.equal(pushover.validate({ ...PUSHOVER, appToken: true }), 'appToken must be text');
  });

  test('validate refuses an emergency expire past the 50 repeats Pushover sends', () => {
    assert.equal(pushover.validate({ ...PUSHOVER, priority: '2', expire: '3600' }), 'Pushover stops an emergency alert after 50 repeats, so expire can be at most 50 times retry (3000)');
    assert.equal(pushover.validate({ ...PUSHOVER, priority: '2', retry: '72', expire: '3600' }), undefined);
    assert.equal(pushover.validate({ ...PUSHOVER, priority: '1', expire: '3600' }), undefined);
  });

  describe('sendPushoverAlert', () => {
    let stub;
    beforeEach(() => { stub = stubFetch(); });
    afterEach(() => { stub.restore(); });

    test('posts the params form encoded to the Pushover API', async () => {
      const response = await send(pushover.sendPushoverAlert, { ...PUSHOVER, priority: '1' }, 'Test alert from admin');
      assert.deepEqual(response, {});
      assert.equal(stub.requests.length, 1);
      assert.equal(stub.requests[0].url, 'https://api.pushover.net/1/messages.json');
      assert.equal(stub.requests[0].options.method, 'POST');
      const body = stub.requests[0].options.body;
      assert.ok(body instanceof URLSearchParams);
      assert.equal(body.get('token'), PUSHOVER.appToken);
      assert.equal(body.get('user'), PUSHOVER.userKey);
      assert.equal(body.get('message'), 'Test alert from admin');
      assert.equal(body.get('priority'), '1');
    });

    test('reports the errors Pushover returns', async () => {
      stub.reply = () => new Response(JSON.stringify({ user: 'invalid', errors: ['user identifier is invalid'], status: 0, request: 'r2' }), { status: 400 });
      const response = await send(pushover.sendPushoverAlert, PUSHOVER, 'hi');
      assert.deepEqual(response, { errors: { pushover: 'Pushover returned 400: user identifier is invalid' } });
    });

    test('reports a reply that is not json', async () => {
      stub.reply = () => new Response('<html>Bad Gateway</html>', { status: 502 });
      const response = await send(pushover.sendPushoverAlert, PUSHOVER, 'hi');
      assert.deepEqual(response, { errors: { pushover: 'Pushover returned 502' } });
    });

    test('reports a timeout', async () => {
      stub.reply = () => { throw timeoutError(); };
      const response = await send(pushover.sendPushoverAlert, PUSHOVER, 'hi');
      assert.deepEqual(response, { errors: { pushover: 'Pushover request timed out' } });
    });

    test('reports a network error', async () => {
      stub.reply = () => { throw new TypeError('fetch failed'); };
      const response = await send(pushover.sendPushoverAlert, PUSHOVER, 'hi');
      assert.deepEqual(response, { errors: { pushover: 'fetch failed' } });
    });

    test('calls a callback that throws only once', async () => {
      const calls = [];
      await pushover.sendPushoverAlert(PUSHOVER, 'hi', null, (response) => {
        calls.push(response);
        throw new Error('callback failed');
      });
      assert.deepEqual(calls, [{}]);
    });

    test('refuses missing or invalid settings without sending', async () => {
      let response = await send(pushover.sendPushoverAlert, { appToken: PUSHOVER.appToken }, 'hi');
      assert.deepEqual(response, { errors: { pushover: 'Missing a value for userKey' } });
      response = await send(pushover.sendPushoverAlert, { ...PUSHOVER, priority: '5' }, 'hi');
      assert.deepEqual(response, { errors: { pushover: 'Priority must be -2, -1, 0, 1, or 2' } });
      assert.equal(stub.requests.length, 0);
    });
  });
});
