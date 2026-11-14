/* systemDashboards.js -- the dashboards Arkime ships, read from disk
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * A system dashboard is a *.dashboard.json file in one of the dashboardsDir
 * directories (default /opt/arkime/etc/dashboards;/opt/arkime/share/dashboards;
 * /opt/arkime/plugins). The install puts Arkime's own in share and replaces
 * them on upgrade; an admin adds or overrides one by dropping a file in
 * etc/dashboards, or a plugin ships its own. The file is the
 * dashboards menu's export format, { name, description, data }, where data is
 * a summaryConfig shareable's data (version, folder, colorScheme, widgets).
 *
 * The id is system-<file name without .dashboard.json>, so it stays the same
 * when the dashboard changes folder and the Arkime tab can open it from
 * ?dashboard=<id>. The first directory listing a name wins, so a file in an
 * earlier directory overrides a later one. The directories are re-read every
 * couple of seconds (SCAN_TTL_MS), each file is only parsed again when it
 * changes, so a new or edited file shows up without a restart. A file that
 * does not parse or validate is logged and left out.
 *
 * A file can list "requires": [field exps], eg a plugin's dashboard naming the
 * fields the plugin adds. It stays hidden until every one of them is a known
 * field, so a cluster without the plugin doesn't get a page of unknown field
 * errors.
 */
'use strict';

const fs = require('fs');
const path = require('path');
const util = require('util');
const Config = require('./config.js');
const ArkimeUtil = require('../common/arkimeUtil');
const ShareableAPIs = require('./apiShareables');
const version = require('../common/version');

const SUFFIX = '.dashboard.json';
const ID_PREFIX = 'system-';
// the part of the file name before the suffix that becomes the id
const KEY_RE = /^[a-z0-9][a-z0-9_-]*$/i;
const MAX_FILE_BYTES = 1024 * 1024;
// how long one directory scan (readdir + a stat per file) serves list()
// calls; a new or edited file is still seen within this
const SCAN_TTL_MS = 2000;

class SystemDashboards {
  // path -> { mtimeNs, size, dashboard | null (bad file, already logged) }
  static #cache = new Map();
  // { at, dashboards } the last directory scan (deduped, before requires)
  static #scan = null;
  // id -> the missing fields last logged for it, so a hidden dashboard is
  // logged once (and again only when what it's missing changes)
  static #hiddenLogged = new Map();

  // --------------------------------------------------------------------------
  /** The directories to scan, from dashboardsDir. */
  static dirs () {
    const prefix = version.config_prefix;
    return Config.getArray('dashboardsDir', `${prefix}/etc/dashboards;${prefix}/share/dashboards;${prefix}/plugins`);
  }

  // --------------------------------------------------------------------------
  /**
   * Turns a parsed file into a dashboard list entry, or returns an error
   * string. The name and data are held to the API's own rules for a saved
   * dashboard (ShareableAPIs.validateName / validateData).
   */
  static #fromFile (key, parsed) {
    if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) { return 'not a JSON object'; }
    if (!ArkimeUtil.isString(parsed.name)) { return 'missing name'; }
    // the same rules as a saved dashboard: the API's own name and data checks
    const named = ShareableAPIs.validateName(parsed.name);
    if (named.error) { return `name: ${named.error.text}`; }
    if (parsed.description !== undefined && !ArkimeUtil.isString(parsed.description)) { return 'description must be a string'; }
    const data = parsed.data;
    if (!data || typeof data !== 'object' || Array.isArray(data)) { return 'missing data'; }
    const invalid = ShareableAPIs.validateData('summaryConfig', data);
    if (invalid) { return `data: ${invalid.text}`; }
    if (parsed.requires !== undefined && !ArkimeUtil.isStringArray(parsed.requires)) { return 'requires must be an array of field names'; }
    return {
      id: `${ID_PREFIX}${key}`,
      type: 'summaryConfig',
      name: named.name,
      description: parsed.description ?? '',
      data,
      system: true,
      creator: '',
      shared: false,
      viewUsers: [],
      viewRoles: [],
      editUsers: [],
      editRoles: [],
      canEdit: false,
      canDelete: false,
      ...(parsed.requires?.length ? { requires: parsed.requires } : {})
    };
  }

  // --------------------------------------------------------------------------
  /**
   * The file's contents, checked and read through one descriptor so a file
   * swapped after the stat can't be a FIFO, a device or larger than
   * MAX_FILE_BYTES. Throws when it is.
   */
  static #readBounded (file) {
    // non blocking so a FIFO swapped in doesn't hang the open
    /* eslint-disable-next-line no-bitwise */
    const fd = fs.openSync(file, fs.constants.O_RDONLY | fs.constants.O_NONBLOCK);
    try {
      if (!fs.fstatSync(fd).isFile()) { throw new Error('not a regular file'); }
      const buf = Buffer.alloc(MAX_FILE_BYTES + 1);
      let len = 0;
      let n;
      while (len < buf.length && (n = fs.readSync(fd, buf, len, buf.length - len, null)) > 0) { len += n; }
      if (len > MAX_FILE_BYTES) { throw new Error(`larger than ${MAX_FILE_BYTES} bytes`); }
      return buf.toString('utf8', 0, len);
    } finally {
      fs.closeSync(fd);
    }
  }

  // --------------------------------------------------------------------------
  /** The dashboard in this file, parsed again only when the file changed. */
  static #load (file) {
    let st;
    try {
      // nanosecond mtime: a same size rewrite within the file system's mtimeMs
      // granularity is still seen
      st = fs.statSync(file, { bigint: true });
    } catch (err) {
      return null;
    }
    const size = Number(st.size);
    const cached = SystemDashboards.#cache.get(file);
    if (cached && cached.mtimeNs === st.mtimeNs && cached.size === size) { return cached.dashboard; }

    const key = path.basename(file, SUFFIX);
    let dashboard = null;
    if (!KEY_RE.test(key)) {
      console.log(`DASHBOARDS - skipping ${file}, the name before ${SUFFIX} must be letters, numbers, - or _`);
    } else if (!st.isFile()) {
      console.log(`DASHBOARDS - skipping ${file}, not a regular file`);
    } else if (size > MAX_FILE_BYTES) {
      console.log(`DASHBOARDS - skipping ${file}, larger than ${MAX_FILE_BYTES} bytes`);
    } else {
      try {
        const result = SystemDashboards.#fromFile(key, JSON.parse(SystemDashboards.#readBounded(file)));
        if (typeof result === 'string') {
          console.log(`DASHBOARDS - skipping ${file}, ${result}`);
        } else {
          dashboard = result;
          if (Config.debug > 0) { console.log(`DASHBOARDS - loaded ${dashboard.id} from ${file}`); }
        }
      } catch (err) {
        console.log(`DASHBOARDS - skipping ${file}, ${err.message}`);
      }
    }
    SystemDashboards.#cache.set(file, { mtimeNs: st.mtimeNs, size, dashboard });
    return dashboard;
  }

  // --------------------------------------------------------------------------
  /**
   * The dashboards in the dashboardsDir directories, by directory order then
   * file name, a later directory's file with the same name as an earlier one
   * ignored. One scan serves SCAN_TTL_MS of calls.
   * @returns {{ file: string, dashboard: object }[]}
   */
  static #scanDirs () {
    const now = Date.now();
    const scan = SystemDashboards.#scan;
    if (scan && now - scan.at < SCAN_TTL_MS) { return scan.dashboards; }

    const dashboards = [];
    const seen = new Set();
    for (const dir of SystemDashboards.dirs()) {
      let names;
      try {
        names = fs.readdirSync(dir);
      } catch (err) {
        if (Config.debug > 0) { console.log(`DASHBOARDS - can't read ${dir}: ${err.message}`); }
        continue;
      }
      for (const fileName of names.filter(n => !n.startsWith('.') && n.endsWith(SUFFIX)).sort()) {
        const file = path.join(dir, fileName);
        const dashboard = SystemDashboards.#load(file);
        if (!dashboard) { continue; }
        if (seen.has(dashboard.id)) {
          if (Config.debug > 0) { console.log(`DASHBOARDS - ignoring ${file}, ${dashboard.id} was already loaded from an earlier dashboardsDir`); }
          continue;
        }
        seen.add(dashboard.id);
        dashboards.push({ file, dashboard });
      }
    }
    SystemDashboards.#scan = { at: now, dashboards };
    return dashboards;
  }

  // --------------------------------------------------------------------------
  /**
   * Every system dashboard, by directory order then file name. A later
   * directory's file with the same name as an earlier one is ignored, and one
   * whose required fields aren't all known is left out.
   * @returns {object[]} list entries: { id, type, name, description, data, system: true, canEdit: false, ... }
   */
  static list () {
    const out = [];
    const fieldsMap = Config.getFieldsMap() ?? {};
    for (const { file, dashboard } of SystemDashboards.#scanDirs()) {
      const missing = (dashboard.requires || []).filter(exp => !Object.hasOwn(fieldsMap, exp));
      if (missing.length) {
        const text = missing.join(', ');
        if (SystemDashboards.#hiddenLogged.get(dashboard.id) !== text) {
          SystemDashboards.#hiddenLogged.set(dashboard.id, text);
          console.log(`DASHBOARDS - hiding ${dashboard.id} from ${file}, missing fields ${text}`);
        }
        continue;
      }
      SystemDashboards.#hiddenLogged.delete(dashboard.id);
      out.push(dashboard);
    }
    return out;
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------

  /**
   * GET - /api/dashboards/system
   *
   * The dashboards Arkime ships and the ones an admin added to a dashboardsDir
   * directory (*.dashboard.json files). They are read only and visible to
   * every user; the Arkime tab opens one from ?dashboard=<id>.
   * @name /dashboards/system
   * @returns {array} data - The dashboards, each shaped like a summaryConfig
   *   shareable list entry: { id, type, name, description, data, system: true, canEdit: false, canDelete: false }
   */
  static apiList (req, res) {
    try {
      return res.send({ data: SystemDashboards.list() });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/dashboards/system`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error listing system dashboards', 'api.dashboards.listFailed');
    }
  }
}

module.exports = SystemDashboards;
