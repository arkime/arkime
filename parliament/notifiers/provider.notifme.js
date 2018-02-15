'use strict';

const Notifme = require('notifme-sdk');


exports.init = function (api) {
  api.register('slack', {
    fields: [{
      name: 'slackWebhookUrl',
      required: true,
      description: 'Incoming Webhooks are a simple way to post messages from external sources into Slack.'
    }],
    sendAlert: exports.sendSlackAlert
  });

  api.register('twilio', {
    fields: [{
      name: 'accountSid',
      required: true,
      secret: true,
      description: 'Twilio account ID'
    }, {
      name: 'authToken',
      required: true,
      secret: true,
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
};

// Slack
exports.sendSlackAlert = function (config, message) {
  if (!config.slackWebhookUrl) {
    console.error('Please add a Slack webhook URL on the Settings page to enable Slack notifications');
    return;
  }

  const slackNotifier = new Notifme.default({
    channels: {
      slack: {
        providers: [{
          type: 'webhook',
          webhookUrl: config.slackWebhookUrl
        }]
      }
    }
  });

  slackNotifier.send({ slack: { text: message } });
};

// Twilio
exports.sendTwilioAlert = function (config, message) {
  if (!config.accountSid || !config.authToken || !config.toNumber || !config.fromNumber) {
    console.error('Please fill out the required fields for Twilio notifications on the Settings page.');
    return;
  }

  const twilioNotifier = new Notifme.default({
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

  twilioNotifier.send({
    sms: {
      from: config.fromNumber,
      to: config.toNumber,
      text: message
    }
  });
};
