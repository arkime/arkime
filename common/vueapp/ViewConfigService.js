/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0
*/
import { fetchWrapper } from './fetchWrapper.js';

// Every app serves these under its own base href, so relative urls work for all

// What a verified totp code bought this page. Held in memory only, so it
// belongs to this session and nothing else: another browser, another tab and
// a reload all ask for a code again, and nothing is left behind on the
// machine. The server idles it out after 10 minutes anyway.
const GRANT_HEADER = 'x-arkime-viewconfig';

let grant = '';

export function clearGrant () {
  grant = '';
}

function grantHeaders () {
  return grant ? { [GRANT_HEADER]: grant } : undefined;
}

export async function getConfig () {
  return fetchWrapper({ url: 'api/viewconfig', headers: grantHeaders() });
}

// viewer only, asks another node for its config over s2s
export async function getNodeConfig (node) {
  return fetchWrapper({ url: `api/viewconfig/node/${encodeURIComponent(node)}`, headers: grantHeaders() });
}

export async function verifyTotp (code) {
  const response = await fetchWrapper({ url: 'api/viewconfig/totp', method: 'POST', data: { code } });
  grant = response?.grant ?? '';
  return response;
}

// viewer only, the nodes that can be asked for a config
export async function getNodes () {
  const response = await fetchWrapper({ url: 'api/stats' });
  return (response?.data ?? []).map(stat => stat.nodeName).filter(n => n).sort();
}
