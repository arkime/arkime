/******************************************************************************/
/* internals.js -- global vars shared between parts of viewer
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const EventEmitter = require('events').EventEmitter;
const fs = require('fs');
const http = require('http');
const https = require('https');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');
const RE2 = require('re2');
const ArkimeConfig = require('../common/arkimeConfig');

// build internals
const internals = {
  isProduction: false,
  multiES: false,
  CYBERCHEFVERSION: '11.3.0',
  httpAgent: new http.Agent({ keepAlive: true, keepAliveMsecs: 20000, maxSockets: 50, maxFreeSockets: 25 }),
  rightClicks: {},
  fieldActions: {},
  pluginEmitter: new EventEmitter(),
  writers: new Map(),
  uploadLimits: {},
  // Minimum supported tshark version (4.6.4), encoded as major*10000 + minor*100 + patch.
  //TSHARKMINVERSION: 40604,
  // For now support debian 13
  TSHARKMINVERSION: 40415,

  // http://garethrees.org/2007/11/14/pngcrush/
  emptyPNG: Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAACklEQVR4nGMAAQAABQABDQottAAAAABJRU5ErkJggg==', 'base64'),
  PNG_LINE_WIDTH: 256,
  shortcutTypeMap: {
    ip: 'ip',
    integer: 'number',
    termfield: 'string',
    uptermfield: 'string',
    lotermfield: 'string'
  },
  anonymousUser: {
    userId: 'anonymous',
    enabled: true,
    webEnabled: true,
    headerAuthEnabled: false,
    emailSearch: true,
    removeEnabled: true,
    packetSearch: true,
    settings: {},
    welcomeMsgNum: 1,
    found: true,
    roles: ['arkimeUser']
  },
  // default settings for users with no settings
  settingDefaults: {
    timezone: 'local',
    detailFormat: 'last',
    showTimestamps: 'last',
    sortColumn: 'firstPacket',
    sortDirection: 'desc',
    spiGraph: 'node',
    connSrcField: 'source.ip',
    connDstField: 'ip.dst:port',
    numPackets: 'last',
    theme: 'default-theme',
    manualQuery: false,
    timelineDataFilters: ['network.packets', 'network.bytes', 'totDataBytes'], // dbField2 values from fields
    hideTags: ''
  },
  usersMissing: {
    userId: '',
    userName: '',
    expression: '',
    enabled: 0,
    webEnabled: 0,
    headerAuthEnabled: 0,
    emailSearch: 0,
    removeEnabled: 0,
    lastUsed: 0
  }
};

internals.initialize = (app) => {
  internals.isProduction = app.get('env') === 'production';
};

ArkimeConfig.loaded(() => {
  // build internals
  internals.elasticBase = Config.getArray('elasticsearch', 'http://localhost:9200');
  internals.remoteClusters = Config.configMap('remote-clusters', 'moloch-clusters');
  internals.esQueryTimeout = Config.get('elasticsearchTimeout', 5 * 60) + 's';
  internals.esScrollTimeout = Config.get('elasticsearchScrollTimeout', 5 * 60) + 's';
  internals.userNameHeader = Config.get('userNameHeader');
  if (Config.get('esAdminUsers', false) !== false) {
    console.log('WARNING - the esAdminUsers setting has been removed, assign the dbAdmin role to those users instead');
  }
  internals.httpsAgent = new https.Agent({ keepAlive: true, keepAliveMsecs: 20000, maxSockets: 50, maxFreeSockets: 25, rejectUnauthorized: !ArkimeConfig.insecure });
  internals.isLocalViewRegExp = Config.get('isLocalViewRegExp') ? new RE2(Config.get('isLocalViewRegExp')) : undefined;
  internals.allowUploads = !!Config.get('uploadCommand');
  internals.uploadRoles = Config.getArray('uploadRoles', 'arkimeUser');
  internals.cronTimeout = +Config.get('dbFlushTimeout', 5) + // How long capture holds items
                           60 + // How long before ES reindexes
                           20; // Transmit and extra time
  internals.prefix = Config.get('prefix', 'arkime_');
  internals.multiES = Config.get('multiES', false);

  // make sure there's an _ after the prefix
  if (internals.prefix && !internals.prefix.endsWith('_')) {
    internals.prefix = `${internals.prefix}_`;
  }

  internals.uploadLimits.fileSize = parseInt(Config.get('uploadFileSizeLimit', 2147483648));

  if (!internals.elasticBase[0].startsWith('http')) {
    internals.elasticBase[0] = 'http://' + internals.elasticBase[0];
  }

  // update user settingDefaults with user-setting-defaults config option if set
  for (const key in internals.settingDefaults) {
    const userSettingDefault = ArkimeConfig.getFull('user-setting-defaults', key);
    if (userSettingDefault !== undefined && userSettingDefault !== null) {
      internals.settingDefaults[key] = key === 'timelineDataFilters' ? userSettingDefault.split(';') : userSettingDefault;
    }
  }

  // tshark integration -------------------------------------------------------
  internals.tsharkMaxPackets = parseInt(Config.get('tsharkMaxPackets', 10000));
  internals.tsharkTimeoutMs = parseInt(Config.get('tsharkTimeoutMs', 30000));
  internals.tsharkMaxConcurrent = parseInt(Config.get('tsharkMaxConcurrent', 3));
  internals.tsharkMemoryLimitMB = parseInt(Config.get('tsharkMemoryLimitMB', 1024));
  internals.tsharkWrapper = Config.getArray('tsharkWrapper', '');
  internals.tsharkRunning = 0;

  // Scratch dir for tshark. mkdtemp so another user can't pre-create it: the
  // fifos we hand tshark live here, and the empty/ subdir is where every
  // WIRESHARK_* var points so no personal profile, plugin or init.lua loads.
  try {
    internals.tsharkDir = fs.mkdtempSync(path.join(os.tmpdir(), 'arkime-tshark-'));
    internals.tsharkEmptyDir = path.join(internals.tsharkDir, 'empty');
    fs.mkdirSync(internals.tsharkEmptyDir, { mode: 0o500 });
    process.on('exit', () => {
      try { fs.rmSync(internals.tsharkDir, { recursive: true, force: true }); } catch (e) { /* ignore */ }
    });
  } catch (e) {
    console.log('tshark: could not create scratch dir, tshark disabled -', e.message);
    return;
  }

  // tshark inherits nothing from viewer -- ARKIME__* overrides can hold the ES
  // credentials and passwordSecret. Only the linker paths are carried over.
  internals.tsharkEnv = {
    PATH: '/usr/bin:/bin',
    HOME: internals.tsharkEmptyDir,
    TMPDIR: internals.tsharkDir,
    LC_ALL: 'C',
    XDG_CONFIG_HOME: internals.tsharkEmptyDir,
    XDG_DATA_HOME: internals.tsharkEmptyDir,
    WIRESHARK_CONFIG_DIR: internals.tsharkEmptyDir,
    WIRESHARK_DATA_DIR: internals.tsharkEmptyDir,
    WIRESHARK_PLUGIN_DIR: internals.tsharkEmptyDir
  };
  for (const v of ['LD_LIBRARY_PATH', 'DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH']) {
    if (process.env[v]) { internals.tsharkEnv[v] = process.env[v]; }
  }

  // argv for a tshark run: the configured sandbox wrapper, then tshark. A
  // dissector driven into a huge allocation can claim GBs long before
  // tsharkTimeoutMs fires, so cap the address space via a shell that execs --
  // exec keeps the pid we spawned, so kill() still reaches tshark. Nothing is
  // interpolated into the shell string, the command goes in as positional
  // args, and a shell that cannot set the limit (macOS) just carries on.
  internals.tsharkCommand = (args) => {
    const wrapper = internals.tsharkWrapper.map(a => a.replaceAll('{tsharkDir}', internals.tsharkDir));
    const argv = [...wrapper, internals.tsharkPath, ...args];
    if (!(internals.tsharkMemoryLimitMB > 0)) { return { cmd: argv[0], args: argv.slice(1) }; }
    return {
      cmd: '/bin/sh',
      args: ['-c', `ulimit -v ${internals.tsharkMemoryLimitMB * 1024} 2>/dev/null || true; exec "$@"`, 'arkime-tshark', ...argv]
    };
  };

  const tsharkCandidates = Config.getArray('tsharkPath', '/opt/arkime/bin/tshark;/usr/bin/tshark;/usr/local/bin/tshark;/opt/homebrew/bin/tshark;/Applications/Wireshark.app/Contents/MacOS/tshark');
  const probeNext = (i) => {
    if (i >= tsharkCandidates.length) {
      console.log('tshark: no tshark binary >= 4.6.4 found in', tsharkCandidates.join(', '));
      return;
    }
    const cand = tsharkCandidates[i].trim();
    if (!cand) { return probeNext(i + 1); }
    let stdout = '';
    let proc;
    let advanced = false;
    const advance = () => {
      if (advanced) { return; }
      advanced = true;
      probeNext(i + 1);
    };
    try {
      proc = spawn(cand, ['-v'], { stdio: ['ignore', 'pipe', 'ignore'], env: internals.tsharkEnv });
    } catch (e) {
      return advance();
    }
    proc.stdout.on('data', (b) => { stdout += b.toString(); });
    proc.on('error', advance);
    proc.on('close', (code) => {
      if (advanced) { return; }
      if (code !== 0 || !/^TShark/m.test(stdout)) { return advance(); }
      const m = stdout.match(/^TShark[^\n]*?((\d+)\.(\d+)(?:\.(\d+))?\S*)/m);
      if (!m) {
        console.log('tshark:', cand, 'unable to parse version, skipping');
        return advance();
      }
      const versionNum = parseInt(m[2]) * 10000 + parseInt(m[3]) * 100 + parseInt(m[4] || '0');
      if (versionNum < internals.TSHARKMINVERSION) {
        console.log('tshark:', cand, 'version', m[1], 'is older than required 4.6.4, skipping');
        return advance();
      }
      advanced = true;
      internals.tsharkPath = cand;
      internals.tsharkVersion = m[1];
      console.log('tshark:', cand, 'version', m[1]);
    });
  };
  probeNext(0);
});

module.exports = internals;
