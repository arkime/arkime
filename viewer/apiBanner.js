/******************************************************************************/
/* apiBanner.js  -- api calls for the global app banner
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const util = require('util');
const Db = require('./db.js');
const ArkimeUtil = require('../common/arkimeUtil');

const BANNER_TYPES = ['info', 'warning', 'error'];
const BANNER_EFFECTS = ['marquee', 'blink', 'rainbow'];
const MAX_MESSAGE_LENGTH = 1000;
const EMPTY_BANNER = { enabled: false, message: '', type: 'info', effects: [], expires: 0, updated: 0 };

// single global banner doc in the shared (schemaless) configs index
const BANNER_INDEX = 'configs';
const BANNER_ID = 'banner';

class BannerAPIs {
  /**
   * A message shown across the top of every page to all users.
   * @typedef Banner
   * @type {object}
   * @property {boolean} enabled - Whether the banner is shown to users.
   * @property {string} message - The message to display.
   * @property {string} type - The severity/style: "info", "warning", or "error".
   * @property {string[]} effects - Optional combinable display effects: any of "marquee", "blink", "rainbow".
   * @property {number} expires - When the banner auto-hides (milliseconds since Unix EPOCH); 0 means never.
   * @property {number} updated - When the banner was last changed (milliseconds since Unix EPOCH); also used as the per-user dismissal key.
   * @property {string} user - The id of the user that last updated the banner.
   */

  /**
   * @ignore
   * Reads the banner doc, returning a disabled banner when none is set.
   * @returns {Banner}
   */
  static async getBanner () {
    try {
      const { body: { _source: doc } } = await Db.get(BANNER_INDEX, BANNER_ID);
      if (!doc) { return { ...EMPTY_BANNER }; }
      return {
        enabled: !!doc.enabled,
        message: doc.message ?? '',
        type: BANNER_TYPES.includes(doc.type) ? doc.type : 'info',
        effects: Array.isArray(doc.effects) ? doc.effects.filter(e => BANNER_EFFECTS.includes(e)) : [],
        expires: doc.expires ?? 0,
        updated: doc.updated ?? 0,
        user: doc.user
      };
    } catch (err) {
      if (err.meta?.statusCode === 404) { return { ...EMPTY_BANNER }; }
      console.log('ERROR - fetching banner', util.inspect(err, false, 50));
      return { ...EMPTY_BANNER };
    }
  }

  /**
   * GET - /api/banner
   *
   * Retrieves the global app banner (disabled if none has been set).
   * @name /banner
   * @returns {Banner} The configured banner.
   */
  static async apiGetBanner (req, res) {
    return res.send(await BannerAPIs.getBanner());
  }

  /**
   * PUT - /api/banner
   *
   * Updates the global app banner (admin only). The message is rendered as
   * plain text by the client, never as HTML.
   * @name /banner
   * @param {boolean} enabled - Whether the banner is shown to users.
   * @param {string} message - The message to display.
   * @param {string} type - The severity/style: "info", "warning", or "error".
   * @param {string[]} effects - Optional combinable display effects: any of "marquee", "blink", "rainbow".
   * @param {number} expires - Optional auto-hide time (ms since EPOCH); 0/omitted means never.
   * @returns {boolean} success - Whether the update operation was successful.
   * @returns {string} text - The success/error message to (optionally) display.
   * @returns {Banner} banner - If successful, the saved banner.
   */
  static async apiUpdateBanner (req, res) {
    const message = req.body.message ?? '';
    if (!ArkimeUtil.isString(message, 0)) {
      return res.serverError(403, 'Banner message must be a string');
    }
    if (message.length > MAX_MESSAGE_LENGTH) {
      return res.serverError(403, `Banner message must be ${MAX_MESSAGE_LENGTH} characters or less`);
    }

    const type = req.body.type ?? 'info';
    if (!BANNER_TYPES.includes(type)) {
      return res.serverError(403, 'Unknown banner type');
    }

    let effects = req.body.effects ?? [];
    if (!Array.isArray(effects)) {
      return res.serverError(403, 'Banner effects must be an array');
    }
    for (const e of effects) {
      if (!BANNER_EFFECTS.includes(e)) {
        return res.serverError(403, 'Unknown banner effect');
      }
    }
    effects = [...new Set(effects)];

    let expires = req.body.expires;
    if (expires === undefined || expires === null || expires === '') { expires = 0; }
    expires = Number(expires);
    if (!Number.isFinite(expires) || expires < 0) {
      return res.serverError(403, 'Banner expires must be a number');
    }
    expires = Math.floor(expires);

    const doc = {
      enabled: !!req.body.enabled,
      message,
      type,
      effects,
      expires,
      updated: Date.now(),
      user: req.user.userId
    };

    try {
      await Db.indexNow(BANNER_INDEX, BANNER_ID, doc);
      return res.send({ success: true, banner: doc, text: 'Updated banner' });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/banner`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error updating banner');
    }
  }
}

module.exports = BannerAPIs;
