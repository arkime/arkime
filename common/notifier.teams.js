/******************************************************************************/
/* notifier.teams.js  -- Microsoft Teams notifier implementation
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

exports.init = function (api) {
  api.register('teams', {
    name: 'Microsoft Teams',
    type: 'teams',
    fields: [{
      name: 'teamsWebhookUrl',
      required: true,
      type: 'secret',
      description: 'Microsoft Teams webhook URL, created with a Power Automate Workflow using the "Post to a channel when a webhook request is received" template.'
    }],
    sendAlert: exports.sendTeamsAlert
  });
};

/**
 * Build an Adaptive Card payload for a Teams Workflows webhook
 */
exports.buildTeamsMessage = function (message, links) {
  const card = {
    $schema: 'http://adaptivecards.io/schemas/adaptive-card.json',
    type: 'AdaptiveCard',
    version: '1.4',
    body: [{
      type: 'TextBlock',
      text: String(message),
      wrap: true
    }]
  };

  if (links && links.length) {
    card.actions = links.map((link) => ({
      type: 'Action.OpenUrl',
      title: link.text,
      url: link.url
    }));
  }

  return {
    type: 'message',
    attachments: [{
      contentType: 'application/vnd.microsoft.card.adaptive',
      content: card
    }]
  };
};

exports.sendTeamsAlert = function (config, message, links, cb) {
  if (!config.teamsWebhookUrl) {
    console.error('Please add a Microsoft Teams webhook URL on the Settings page to enable Teams notifications');
    if (cb) { cb({ errors: { teams: 'Missing Teams webhook URL' } }); }
    return;
  }

  if (!config.teamsWebhookUrl.startsWith('https://')) {
    if (cb) { cb({ errors: { teams: 'Teams webhook URL must start with https://' } }); }
    return;
  }

  fetch(config.teamsWebhookUrl, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(exports.buildTeamsMessage(message, links)),
    signal: AbortSignal.timeout(10000)
  })
    .then(async (response) => {
      if (!response.ok) {
        const text = await response.text().catch(() => '');
        const error = `Teams webhook returned ${response.status}${text ? `: ${text.slice(0, 200)}` : ''}`;
        console.error(error);
        if (cb) { cb({ errors: { teams: error } }); }
        return;
      }
      if (cb) { cb({}); }
    })
    .catch((err) => {
      const error = err.name === 'TimeoutError' ? 'Teams webhook request timed out' : (err.message || String(err));
      console.error('Teams webhook error:', error);
      if (cb) { cb({ errors: { teams: error } }); }
    });
};
