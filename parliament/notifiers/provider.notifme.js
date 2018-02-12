'use strict';

const Notifme = require('notifme-sdk');


// TODO add option to alert to different services (slack only now)
exports.init = function (api) {
  api.register('slack', {
    fields: [{ name: 'slackWebhookUrl' }],
    sendAlert: exports.sendSlackAlert
  });
};

// Slack
exports.sendSlackAlert = function (config, message) {
  if (!config.slackWebhookUrl) {
    // TODO better error?
    console.error('Please add a Slack webhook URL in the Settings page to enable Slack notifications');
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
