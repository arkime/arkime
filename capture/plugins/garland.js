/******************************************************************************/
/* garland.js
 *
 * Copyright 2025 Arkime. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

let Pcap;

function garland (pcap, buffer, obj, pos) {
  return pcap.ethertyperun(0, buffer.slice(18), obj, pos + 18);
}

/// ///////////////////////////////////////////////////////////////////////////////
exports.init = function (config, emitter, api) {
  const Config = config;
  Pcap = api.getPcap();

  if (Pcap.setEtherCB === undefined) {
    console.error('ERROR - Garland plugin requires newer version of Arkime');
    return;
  }
  console.log('Garland plugin initialized');

  const ethertype = parseInt(Config.get('garlandEthertype', 0xff12));

  Pcap.setEtherCB(ethertype, garland);
};
