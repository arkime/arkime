/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
*/

// Packet count choices shared by the session detail Packets and Shark tabs.
// Both open on the numPackets setting, or for 'last' the count last picked in
// either tab.
export const PACKET_COUNTS = [50, 200, 500, 1000, 2000];

const LAST_KEY = 'moloch-packets';
const FALLBACK = 200;

export function defaultPacketCount (settings) {
  let n = settings?.numPackets;
  if (n === 'last') {
    try { n = localStorage[LAST_KEY]; } catch (e) { n = undefined; }
  }
  // the setting and localStorage both hold strings
  n = parseInt(n);
  return Number.isFinite(n) && n > 0 ? n : FALLBACK;
}

export function rememberPacketCount (settings, n) {
  if (settings?.numPackets !== 'last') { return; }
  try { localStorage[LAST_KEY] = n; } catch (e) { /* private mode */ }
}
