/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0

Unit tests for the notifme notifiers (email, slack, twilio) and the Teams card
builder. Run with `npm run test:unit`.

The alert message can carry strings the sender doesn't control (hunt and
periodic query names, what monitored clusters report to Parliament), so the
channels that render markup must not let them inject any. The notifme sdk is
stubbed so nothing is sent.
*/
import { test, describe, beforeEach } from 'node:test';
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const cjsRequire = createRequire(import.meta.url);
const notifme = cjsRequire('../common/notifier.notifme.js');
const teams = cjsRequire('../common/notifier.teams.js');
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
