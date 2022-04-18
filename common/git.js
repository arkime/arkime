'use strict';

// eslint-disable-next-line camelcase
const child_process = require('child_process');

function git (command) {
  // eslint-disable-next-line camelcase
  return child_process.execSync(`git ${command}`, { encoding: 'utf8' }).trim();
}

module.exports = { git };
