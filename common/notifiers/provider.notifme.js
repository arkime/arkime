'use strict';

const Notifme = require('mjlescano-notifme-sdk');

exports.init = function (api) {
  api.register('slack', {
    name: 'Slack',
    type: 'slack',
    fields: [{
      name: 'slackWebhookUrl',
      required: true,
      type: 'secret',
      description: 'Incoming Webhooks are a simple way to post messages from external sources into Slack.'
    }],
    sendAlert: exports.sendSlackAlert
  });

  api.register('twilio', {
    name: 'Twilio',
    type: 'twilio',
    fields: [{
      name: 'accountSid',
      required: true,
      type: 'secret',
      description: 'Twilio account ID'
    }, {
      name: 'authToken',
      required: true,
      type: 'secret',
      description: 'Twilio authentication token'
    }, {
      name: 'toNumber',
      required: true,
      description: 'The number to send the alert to'
    }, {
      name: 'fromNumber',
      required: true,
      description: 'The number to send the alert from'
    }],
    sendAlert: exports.sendTwilioAlert
  });

  api.register('email', {
    name: 'Email',
    type: 'email',
    fields: [{
      name: 'secure',
      type: 'checkbox',
      description: 'Send the email securely'
    }, {
      name: 'host',
      required: true,
      description: 'Email host'
    }, {
      name: 'port',
      required: true,
      description: 'Email port'
    }, {
      name: 'user',
      description: 'The username of the user sending the email'
    }, {
      name: 'password',
      type: 'secret',
      description: 'Password of the user sending the email'
    }, {
      name: 'from',
      required: true,
      description: 'Send the email from this address'
    }, {
      name: 'to',
      required: true,
      description: 'Send the email to this address'
    }, {
      name: 'subject',
      description: 'The subject of the email (defaults to "Parliament Alert")'
    }],
    sendAlert: exports.sendEmailAlert
  });
};

// Slack
exports.sendSlackAlert = function (config, message, links, cb) {
  if (!config.slackWebhookUrl) {
    console.error('Please add a Slack webhook URL on the Settings page to enable Slack notifications');
    return;
  }

  /* eslint new-cap: ["error", { "properties": false }] */
  const SlackNotifier = new Notifme.default({
    channels: {
      slack: {
        providers: [{
          type: 'webhook',
          webhookUrl: config.slackWebhookUrl
        }]
      }
    }
  });

  // add links to the slack alert
  const slackMsgObj = { slack: { text: message } };
  if (links && links.length) {
    slackMsgObj.slack.attachments = [];
    for (const link of links) {
      slackMsgObj.slack.attachments.push({
        fallback: `${link.text}: <${link.link}>`,
        actions: [{
          type: 'button',
          text: link.text,
          url: link.url
        }]
      });
    }
  }

  SlackNotifier.send(slackMsgObj)
    .then((response) => { if (cb) { cb(response); } });
};

// Twilio
exports.sendTwilioAlert = function (config, message, links, cb) {
  if (!config.accountSid || !config.authToken || !config.toNumber || !config.fromNumber) {
    console.error('Please fill out the required fields for Twilio notifications on the Settings page.');
    return;
  }

  /* eslint new-cap: ["error", { "properties": false }] */
  const TwilioNotifier = new Notifme.default({
    channels: {
      sms: {
        providers: [{
          type: 'twilio',
          accountSid: config.accountSid,
          authToken: config.authToken
        }]
      }
    }
  });

  if (links && links.length) {
    for (const link of links) {
      message += `\n${link.text}: ${link.url}`;
    }
  }

  TwilioNotifier.send({
    sms: {
      from: config.fromNumber,
      to: config.toNumber,
      text: message
    }
  }).then((response) => { if (cb) { cb(response); } });
};

// Email
exports.sendEmailAlert = function (config, message, links, cb) {
  if (!config.host || !config.port || !config.to || !config.from) {
    console.error('Please fill out the required fields for Email notifications on the Settings page.');
    return;
  }

  if (!config.secure) {
    config.secure = false;
  }

  /* eslint new-cap: ["error", { "properties": false }] */
  const EmailNotifier = new Notifme.default({
    channels: {
      email: {
        providers: [{
          type: 'smtp',
          host: config.host,
          port: config.port,
          secure: config.secure,
          auth: {
            user: config.user,
            pass: config.password
          }
        }]
      }
    }
  });

  if (links && links.length) {
    for (const link of links) {
      message += `<br><a href="${link.url}">${link.text}</a>`;
    }
  }

  EmailNotifier.send({
    email: {
      html: message,
      to: config.to,
      from: config.from,
      subject: config.subject || 'Parliament Alert'
    }
  }).then((response) => { if (cb) { cb(response); } });
};
