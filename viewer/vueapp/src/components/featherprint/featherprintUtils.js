/******************************************************************************/
/* featherprintUtils.js -- formatting shared by the featherprint page, the
 *                         device detail pane and the admin page.
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
import { timezoneDateString } from '@common/vueFilters.js';

/**
 * Render a ms timestamp in the user's configured timezone, like every other
 * viewer page. `settings` is the user's settings object (may be undefined).
 */
export function fmtTs (ts, settings) {
  if (!ts) { return '—'; }
  return timezoneDateString(ts, settings?.timezone ?? 'local', settings?.ms ?? false);
}

/**
 * One-line human summary of a history entry or alert, keyed off its `kind`.
 * Falls back to the raw before/after payload for kinds we don't special-case.
 */
export function describeHistory (h) {
  const b = h.before;
  const a = h.after;
  switch (h.kind) {
  case 'newIp':
    return a?.ip ? `${a.ip}${a.classification ? ` (${a.classification})` : ''}` : '';
  case 'newMac':
    return a?.value ? `${a.value}${a.source ? ` via ${a.source}` : ''}` : '';
  case 'changeMac':
    return `${b?.value ?? '?'} → ${a?.value ?? '?'}`;
  case 'changeIp':
    return `${b?.mac ?? '?'}: ${b?.ip ?? '?'} → ${a?.ip ?? '?'}`;
  case 'newName':
  case 'changeName':
    if (b && a) { return `${b.name ?? '?'} → ${a.name ?? '?'}`; }
    return a?.name ? `${a.name}${a.source ? ` (${a.source})` : ''}` : '';
  case 'newService':
    if (!a) { return ''; }
    return `${a.type ?? '?'} (${a.proto ?? '?'}${a.port !== undefined ? '/' + a.port : ''})`;
  case 'changeDevice':
    return `${b?.classification ?? '?'} → ${a?.classification ?? '?'}`;
  default:
    if (b && a) { return `${JSON.stringify(b)} → ${JSON.stringify(a)}`; }
    if (a) { return JSON.stringify(a); }
    return '';
  }
}

/* Grouped-expression field defs for arkime-session-field pivot menus. Frozen
 * module constants rather than per-component data() so every row shares one
 * object identity and Vue doesn't wrap each copy in a reactive proxy.
 * `category` drives which value actions SessionField offers. */
export const IP_FIELD = Object.freeze({
  dbField: 'ip', exp: 'ip', type: 'ip', group: 'general', category: 'ip', friendlyName: 'All IP fields'
});
export const MAC_FIELD = Object.freeze({
  dbField: 'mac', exp: 'mac', type: 'lotermfield', group: 'general', friendlyName: 'All MAC fields'
});
export const HOST_FIELD = Object.freeze({
  dbField: 'host', exp: 'host', type: 'lotermfield', group: 'general', category: 'host', friendlyName: 'All host fields'
});

/* Tertiary-coloured action button, the viewer's convention for a refresh
 * affordance. Inline style because the theme defines no `on-tertiary`, so
 * Vuetify's `color="tertiary"` can't pick a readable foreground. */
export const TERTIARY_BTN_STYLE = Object.freeze({
  backgroundColor: 'rgb(var(--v-theme-tertiary))',
  color: 'rgb(var(--v-theme-button-fg))'
});
