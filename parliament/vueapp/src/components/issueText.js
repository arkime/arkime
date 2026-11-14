/*
 * Copyright Andy Wick
 * SPDX-License-Identifier: Apache-2.0
 */
import moment from 'moment-timezone';
import { commaString } from '@common/vueFilters.js';

// Issues are saved as their type and values, the words come from the locale files.
// An unknown type falls back to the English strings the server sends.
const TYPES = new Set(['esRed', 'esDown', 'esDropped', 'outOfDate', 'noPackets', 'lowDiskSpace', 'lowDiskSpaceES']);

export function issueTitle (issue, t) {
  return TYPES.has(issue.type) ? t(`parliament.issue.title-${issue.type}`) : issue.title;
}

export function issueMessage (issue, t) {
  // esRed is the only type without a value, the server message covers one that is missing
  if (!TYPES.has(issue.type) || (issue.value === undefined && issue.type !== 'esRed')) { return issue.message; }

  let key = issue.type;
  const params = { node: issue.node ?? '', value: issue.value };
  if (issue.type === 'esDropped') {
    params.value = commaString(issue.value);
  } else if (issue.type === 'outOfDate') {
    const date = moment(issue.value);
    if (!date.isValid()) { return issue.message; }
    params.value = date.format('YYYY/MM/DD HH:mm:ss');
  } else if (issue.type === 'lowDiskSpace' || issue.type === 'lowDiskSpaceES') {
    params.percent = Number.isFinite(issue.value) ? issue.value.toFixed(1) : 'N/A';
    params.gb = ((issue.freeSpaceM || 0) / 1000).toFixed(2);
    if (issue.thresholdType === 'gb') { key += 'GB'; }
  }

  return t(`parliament.issue.message-${key}`, params);
}
