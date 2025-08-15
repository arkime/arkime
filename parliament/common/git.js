'use strict';
// NOTE: modified from 'cjs -> ejs' for use with Vite

 
import { execSync } from 'child_process';

export function git (command) {
  if (command === 'describe --tags' && process.env.ARKIME_BUILD_FULL_VERSION) {
    return process.env.ARKIME_BUILD_FULL_VERSION;
  } else if (command === 'log -1 --format=%aI' && process.env.ARKIME_BUILD_DATE) {
    return process.env.ARKIME_BUILD_DATE;
  }

  return execSync(`git ${command}`, { encoding: 'utf8' }).trim();
}
