'use strict';

// eslint-disable-next-line camelcase
const child_process = require('child_process');

function git (command) {
  if (command === 'describe --tags' && process.env.ARKIME_BUILD_FULL_VERSION) {
    return process.env.ARKIME_BUILD_FULL_VERSION;
  } else if (command === 'log -1 --format=%aI' && process.env.ARKIME_BUILD_DATE) {
    return process.env.ARKIME_BUILD_DATE;
  }

  // eslint-disable-next-line camelcase
  return child_process.execSync(`git ${command}`, { encoding: 'utf8' }).trim();
}

module.exports = { git };
