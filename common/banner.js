/******************************************************************************/
/* banner.js  -- shared per-app banner code
 *
 * Each Arkime app (viewer, cont3xt, parliament, wise) has its own banner,
 * stored as a doc `banner-<app>` in the `configs` collection on the shared
 * users db, whatever backend it is. A "sync" copies one app's banner to every
 * app's doc.
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const util = require('util');
const User = require('./user');
const ArkimeUtil = require('./arkimeUtil');

const BANNER_TYPES = ['info', 'warning', 'error'];
const BANNER_EFFECTS = ['marquee', 'blink', 'rainbow'];
const BANNER_APPS = ['viewer', 'cont3xt', 'parliament', 'wise'];
const MAX_MESSAGE_LENGTH = 1000;
const EMPTY_BANNER = { enabled: false, message: '', type: 'info', effects: [], expires: 0, updated: 0 };

class Banner {
  static #app; // this process's app name
  static #prefix; // the users prefix
  static #configs; // configs collection on the users db, made on first use

  /**
   * @ignore
   * @param {object} options
   * @param {string} options.app - one of viewer/cont3xt/parliament/wise
   * @param {string} options.prefix - the users ES prefix
   */
  static initialize (options) {
    Banner.#app = options.app;
    Banner.#prefix = options.prefix;
  }

  // undefined when there's no users db (e.g. wise in anonymous mode)
  static #collection () {
    if (!Banner.#configs) {
      // On ES db.pl creates the configs index
      Banner.#configs = User.dbConnect(Banner.#prefix)?.collection('configs', { es: { dbpl: true } });
    }
    return Banner.#configs;
  }

  static #docId (app) { return `banner-${app}`; }

  // NOTE: deliberately omits `user` -- it's stored for audit but the GET is
  // readable by any authenticated user and the client never needs it.
  static #normalize (doc) {
    if (!doc) { return { ...EMPTY_BANNER }; }
    return {
      enabled: !!doc.enabled,
      message: doc.message ?? '',
      type: BANNER_TYPES.includes(doc.type) ? doc.type : 'info',
      effects: Array.isArray(doc.effects) ? doc.effects.filter(e => BANNER_EFFECTS.includes(e)) : [],
      expires: doc.expires ?? 0,
      updated: doc.updated ?? 0
    };
  }

  /**
   * @ignore
   * Reads an app's banner doc; returns a disabled banner when unset or when
   * there's no users DB (e.g. wise in anonymous mode).
   * @returns {object} banner
   */
  static async getBanner (app = Banner.#app) {
    const configs = Banner.#collection();
    if (!configs) { return { ...EMPTY_BANNER }; }
    try {
      return Banner.#normalize(await configs.get(Banner.#docId(app)));
    } catch (err) {
      console.log('ERROR - fetching banner', util.inspect(err, false, 50));
      return { ...EMPTY_BANNER };
    }
  }

  static async #save (app, doc) {
    await Banner.#collection().put(Banner.#docId(app), doc);
  }

  // builds a validated banner doc from a request body, or returns { error }
  static #buildDoc (req) {
    const message = req.body.message ?? '';
    if (!ArkimeUtil.isString(message, 0)) {
      return { error: 'Banner message must be a string' };
    }
    if (message.length > MAX_MESSAGE_LENGTH) {
      return { error: `Banner message must be ${MAX_MESSAGE_LENGTH} characters or less` };
    }

    const type = req.body.type ?? 'info';
    if (!BANNER_TYPES.includes(type)) {
      return { error: 'Unknown banner type' };
    }

    let effects = req.body.effects ?? [];
    if (!Array.isArray(effects)) {
      return { error: 'Banner effects must be an array' };
    }
    for (const e of effects) {
      if (!BANNER_EFFECTS.includes(e)) { return { error: 'Unknown banner effect' }; }
    }
    effects = [...new Set(effects)];

    let expires = req.body.expires;
    if (expires === undefined || expires === null || expires === '') { expires = 0; }
    expires = Number(expires);
    if (!Number.isFinite(expires) || expires < 0) {
      return { error: 'Banner expires must be a number' };
    }

    return {
      doc: {
        enabled: !!req.body.enabled,
        message,
        type,
        effects,
        expires: Math.floor(expires),
        updated: Date.now(),
        user: req.user.userId
      }
    };
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /** GET - /api/banner -- this app's banner (disabled if none set) */
  static async apiGetBanner (req, res) {
    return res.json(await Banner.getBanner());
  }

  /** PUT - /api/banner -- update this app's banner (admin only) */
  static async apiUpdateBanner (req, res) {
    if (!Banner.#collection()) {
      return res.status(500).json({ success: false, text: 'Users DB not configured' });
    }
    const { error, doc } = Banner.#buildDoc(req);
    if (error) { return res.status(400).json({ success: false, text: error }); }

    try {
      await Banner.#save(Banner.#app, doc);
      return res.json({ success: true, banner: doc, text: 'Updated banner' });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/banner`, util.inspect(err, false, 50));
      return res.status(500).json({ success: false, text: 'Error updating banner' });
    }
  }

  /** POST - /api/banner/sync -- copy this app's banner to every app (admin only) */
  static async apiSyncBanner (req, res) {
    if (!Banner.#collection()) {
      return res.status(500).json({ success: false, text: 'Users DB not configured' });
    }
    try {
      const current = await Banner.getBanner(Banner.#app);
      const doc = {
        enabled: current.enabled,
        message: current.message,
        type: current.type,
        effects: current.effects,
        expires: current.expires,
        updated: Date.now(),
        user: req.user.userId
      };
      await Promise.all(BANNER_APPS.map(app => Banner.#save(app, doc)));
      return res.json({ success: true, banner: doc, text: 'Synced banner to all apps' });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/banner/sync`, util.inspect(err, false, 50));
      return res.status(500).json({ success: false, text: 'Error syncing banner' });
    }
  }
}

module.exports = Banner;
