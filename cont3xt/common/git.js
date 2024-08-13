'use strict';
// NOTE: modified from 'cjs -> ejs' for use in

// eslint-disable-next-line camelcase
import { execSync } from 'child_process';

export function git (command) {
  // eslint-disable-next-line camelcase
  return execSync(`git ${command}`, { encoding: 'utf8' }).trim();
}
