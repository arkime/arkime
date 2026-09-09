/******************************************************************************/
/* viewConfig.js  -- shared read only view of the running config
 *
 * Every app (viewer, cont3xt, parliament, wise) exposes the config it is
 * actually running with to its own admins. Values are redacted here, on the
 * server, so a secret never reaches the browser: any key that looks like a
 * credential, and the password half of any url.
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const cryptoLib = require('crypto');
const { LRUCache } = require('lru-cache');
const ArkimeConfig = require('./arkimeConfig');
const ArkimeUtil = require('./arkimeUtil');

const REDACTED = '[redacted]';

// A key holding a credential, matched anywhere in the name, case insensitive
const SECRET_KEY_RE = /(secret|password|token|apikey|basicauth)/i;

// Whole sections whose every value is key material, no matter what the keys
// are named - [keks] is `<kekId>=<key encryption key>`
const SECRET_SECTIONS = new Set(['keks']);

// scheme://userinfo@host. The userinfo can't span a path, query or fragment,
// which both keeps a `?x=a@b` query from looking like one and lets a `,` or `;`
// joined list of urls redact element by element
const URL_USERINFO_RE = /([a-z][a-z0-9+.-]*:\/\/)([^/@\s?#]+)@/gi;

// The user half is a password too here, so there is nothing safe to keep
const WHOLE_USERINFO_SCHEMES = new Set(['redis-sentinel://']);

// Some settings pack their own key:value pairs into one value, and those pairs
// hold credentials too, eg a remote cluster's
// `url:http://host:8124;passwordSecret:secret;name:Test`
const INNER_SECRET_RE = /([\w.-]*(?:secret|password|token|apikey|basicauth)[\w.-]*|\b(?:pass|passwd|pwd))(\s*[:=]\s*)([^;]*)/gi;

// what the browser must send back after verifying a totp code
const GRANT_HEADER = 'x-arkime-viewconfig';

class ViewConfig {
  // grantId => userId. A verified code hands the browser that asked an
  // unguessable grant, so the unlock belongs to that browser and not to
  // every session the user has open. Idles out after 10 minutes of no use.
  static #totpGrants = new LRUCache({ max: 1000, ttl: 10 * 60 * 1000, updateAgeOnGet: true });

  // Exact key names an app knows hold a credential, lowercased. Sources and
  // integrations name their api key settings whatever they like (`key`,
  // `appCode`, ...), so they register their own instead of hoping the name
  // happens to contain the word secret.
  static #secretKeys = new Set(['key', 'keys', 'appcode', 'credentials', 'pass', 'passwd', 'pwd']);

  // ----------------------------------------------------------------------------
  /**
   * @ignore
   * Register setting names that must never be shown, eg every field a wise
   * source or cont3xt integration marked as a password.
   * @param {string[]} names
   */
  static addSecretKeys (names) {
    for (const keyName of names ?? []) {
      if (typeof keyName === 'string' && keyName !== '') {
        ViewConfig.#secretKeys.add(keyName.toLowerCase());
      }
    }
  }

  // --------------------------------------------------------------------------
  /**
   * @ignore
   * Redact the password out of every url in a value.
   * `redis://user:pass@host` becomes `redis://user:[redacted]@host`, and a
   * userinfo with no `:` is a bare credential, so all of it goes.
   */
  static redactUrls (str) {
    return str.replace(URL_USERINFO_RE, (match, scheme, userinfo) => {
      if (WHOLE_USERINFO_SCHEMES.has(scheme.toLowerCase())) {
        return `${scheme}${REDACTED}@`;
      }
      const colon = userinfo.indexOf(':');
      if (colon === -1) { return `${scheme}${REDACTED}@`; }
      return `${scheme}${userinfo.slice(0, colon)}:${REDACTED}@`;
    });
  }

  // --------------------------------------------------------------------------
  /**
   * @ignore
   * The display value for one setting, and whether anything was hidden.
   * json/yaml configs hand us numbers, booleans, arrays and objects, so
   * everything that isn't a string is rendered as json for the ini view.
   */
  static redactValue (key, value, alwaysSecret) {
    // an empty setting has nothing to hide, and hiding it would make a set
    // and an unset value look the same in a diff
    if (value === '' || value === null || value === undefined) {
      return { value: '', redacted: false };
    }

    if (alwaysSecret || SECRET_KEY_RE.test(key) || ViewConfig.#secretKeys.has(key.toLowerCase())) {
      return { value: REDACTED, redacted: true };
    }

    if (Array.isArray(value)) {
      const parts = value.map(v => ViewConfig.redactValue(key, v));
      return {
        value: JSON.stringify(parts.map(p => p.value)),
        redacted: parts.some(p => p.redacted)
      };
    }

    if (value !== null && typeof value === 'object') {
      const out = {};
      let redacted = false;
      for (const [k, v] of Object.entries(value)) {
        const part = ViewConfig.redactValue(k, v);
        out[k] = part.value;
        redacted ||= part.redacted;
      }
      return { value: JSON.stringify(out), redacted };
    }

    if (typeof value !== 'string') { return { value: `${value}`, redacted: false }; }

    const redactedValue = ViewConfig.redactUrls(value)
      .replace(INNER_SECRET_RE, (match, innerKey, sep, innerValue) => {
        return innerValue === '' ? match : `${innerKey}${sep}${REDACTED}`;
      });

    return { value: redactedValue, redacted: redactedValue !== value };
  }

  // --------------------------------------------------------------------------
  /**
   * @ignore
   * The whole running config, redacted, in the shape the UI renders and diffs.
   */
  static getConfig () {
    const sections = {};
    const redacted = [];

    for (const sectionName of ArkimeConfig.getSections().sort()) {
      const section = ArkimeConfig.getSection(sectionName);
      if (section === null || typeof section !== 'object') { continue; }
      const secretSection = SECRET_SECTIONS.has(sectionName.toLowerCase());
      sections[sectionName] = {};
      for (const key of Object.keys(section).sort()) {
        const result = ViewConfig.redactValue(key, section[key], secretSection);
        sections[sectionName][key] = result.value;
        if (result.redacted) { redacted.push(`${sectionName}.${key}`); }
      }
    }

    const overrides = {};
    for (const [key, value] of Object.entries(ArkimeConfig.getOverrides())) {
      // an override key is already section.key, redact on the key half
      const result = ViewConfig.redactValue(key.slice(key.indexOf('.') + 1), value);
      overrides[key] = result.value;
      if (result.redacted) { redacted.push(`override.${key}`); }
    }

    return {
      sections,
      overrides,
      redacted,
      envKeys: ArkimeConfig.getEnvKeys(),
      defaultSections: ArkimeConfig.getDefaultSections(),
      configFile: ViewConfig.redactUrls(ArkimeConfig.configFile ?? '')
    };
  }

  /******************************************************************************/
  // APIs
  /******************************************************************************/

  // --------------------------------------------------------------------------
  /**
   * @ignore
   * A user with totp set must prove it before seeing any config. A user
   * without totp set has no second factor to ask for, so the admin role is
   * all there is.
   */
  static checkTotp (req, res, next) {
    if (!req.user?.totpSecret) { return next(); }

    const grant = req.headers[GRANT_HEADER];
    if (ArkimeUtil.isString(grant) && ViewConfig.#totpGrants.get(grant) === req.user.userId) {
      return next();
    }

    return res.status(403).json({ success: false, needTotp: true, text: 'TOTP code required' });
  }

  // --------------------------------------------------------------------------
  /**
   * POST - /api/viewconfig/totp
   *
   * Verify a TOTP code to unlock the config view for this browser.
   * @name /viewconfig/totp
   * @returns {boolean} success - Whether the code verified.
   * @returns {string} grant - Send back as the x-arkime-viewconfig header on every config request.
   */
  static apiVerifyTotp (req, res) {
    if (!req.user?.totpSecret) {
      return res.json({ success: true }); // nothing to verify
    }

    const result = req.user.verifyTotp(req.body?.code);
    if (result === 'rate-limited') {
      return res.status(429).json({ success: false, text: 'Too many attempts, try again later' });
    }
    if (!result) {
      return res.status(403).json({ success: false, needTotp: true, text: 'Invalid TOTP code' });
    }

    const grant = cryptoLib.randomBytes(32).toString('hex');
    ViewConfig.#totpGrants.set(grant, req.user.userId);
    return res.json({ success: true, grant });
  }

  // --------------------------------------------------------------------------
  /**
   * GET - /api/viewconfig
   *
   * Fetch the config this app is running with, redacted.
   * @name /viewconfig
   * @returns {boolean} success - Whether the request was successful.
   * @returns {object} sections - The config sections, key to redacted value.
   * @returns {object} overrides - The command line -o overrides in effect.
   * @returns {string[]} redacted - The section.keys whose value was redacted.
   * @returns {string[]} envKeys - The section.keys that came from the environment.
   * @returns {string[]} defaultSections - The sections this app itself reads.
   * @returns {string} configFile - Where the config was loaded from.
   */
  static apiGetConfig (req, res) {
    return res.json({ success: true, ...ViewConfig.getConfig() });
  }
}

module.exports = ViewConfig;
