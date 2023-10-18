/******************************************************************************/
/* overview.js  -- Cont3xt overviews
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');

const iTypes = ['domain', 'ip', 'url', 'email', 'phone', 'hash', 'text'];

class Overview {
  constructor (data) {
    Object.assign(this, data);
  }

  /**
   * A Cont3xt Overview Field Object
   *
   * Cont3xt Overview Fields configure the display for an entry in an overview
   * @typedef Cont3xtOverviewField
   * @param {string} from - The name of the integration to use a field from
   * @param {string} field - The label of the field to use
   * @param {string | undefined} alias - Optional replacement label to display for this field in the overview
   */

  /**
   * A Cont3xt Overview Object
   *
   * Cont3xt Overviews are used to configure the default display for an itype
   * @typedef Cont3xtOverview
   * @param {string} _id - The id of the overview
   * @param {string} creator - The creator of the overview
   * @param {string} name - The name of the overview
   * @param {string} title - The title of the overview, filled for display on the integration card panel
   * @param {string} iType - The itype this overview can be displayed for
   * @param {Cont3xtOverviewField[]} fields - The array of fields to be displayed by this overview
   * @param {array} editRoles - The Arkime roles that can edit this overview
   * @param {array} viewRoles - The Arkime roles that can view this overview
   * @param {boolean} _editable - Whether the logged in user is allowed to edit this overview
   * @param {boolean} _viewable - Whether the logged in user is allowed to view this overview
   */

  /**
   * Initialization ensures default overviews exist
   */
  static initialize () {
    const createDefaultForIType = (iType) => {
      const { name: defaultName, title, fields } = defaultOverviewPropertiesForIType[iType];

      return {
        iType,
        name: defaultName,
        title,
        fields,
        viewRoles: ['cont3xtUser'],
        editRoles: ['superAdmin'],
        creator: '!__cont3xt__!'
      };
    };

    for (const iType of iTypes) {
      Db.getOverview(iType).then((defaultOverviewForItype) => {
        if (defaultOverviewForItype) { return; } // skip existing

        Db.putOverview(iType, createDefaultForIType(iType)).then(() => {

        }).catch((err) => {
          console.log(`WARNING - failed to create default overview for ${iType} iType... skipping`, err);
        });
      }).catch((err) => {
        console.log(`WARNING - failed to check existence of ${iType} iType default overview... skipping:`, err);
      });
    }
  }

  // Verify the given overview, returns error { msg: string } on failure, { custom } otherwise
  static verifyOverviewCustomField (custom) {
    // non-empty strings are always valid (they're applied as both label and field)
    if (ArkimeUtil.isString(custom)) { return { custom }; }

    if (custom == null || typeof custom !== 'object') {
      return { msg: 'Custom field must be a string or object' };
    }
    custom = (
      ({ // only allow these properties in custom fields
        // eslint-disable-next-line no-shadow
        label, field, type, path, fields, defang, pivot, join, fieldRoot, fieldRootPath, filterEmpty, defaultSortField, defaultSortDirection, altText, noSearch, postProcess
      }) => ({
        label, field, type, path, fields, defang, pivot, join, fieldRoot, fieldRootPath, filterEmpty, defaultSortField, defaultSortDirection, altText, noSearch, postProcess
      })
    )(custom);

    if (custom.label != null && !ArkimeUtil.isString(custom.label)) {
      return { msg: 'Custom label must be a string when present' };
    }
    if (custom.field != null && !ArkimeUtil.isString(custom.field)) {
      return { msg: 'Custom field must be a string when present' };
    }
    if (custom.fieldRoot != null && !ArkimeUtil.isString(custom.fieldRoot)) {
      return { msg: 'Custom fieldRoot must be a string when present' };
    }
    if (custom.type != null && !ArkimeUtil.isString(custom.type)) {
      return { msg: 'Custom type must be a string when present' };
    }
    if (custom.defaultSortField != null && !ArkimeUtil.isString(custom.defaultSortField)) {
      return { msg: 'Custom defaultSortField must be a string when present' };
    }
    if (custom.defaultSortDirection != null && !ArkimeUtil.isString(custom.defaultSortDirection)) {
      return { msg: 'Custom defaultSortDirection must be a string when present' };
    }
    if (custom.altText != null && !ArkimeUtil.isString(custom.altText)) {
      return { msg: 'Custom altText must be a string when present' };
    }
    if (custom.path != null && !ArkimeUtil.isStringArray(custom.path)) {
      return { msg: 'Custom path must be a string array when present' };
    }
    if (custom.fieldRootPath != null && !ArkimeUtil.isStringArray(custom.fieldRootPath)) {
      return { msg: 'Custom fieldRootPath must be a string array when present' };
    }
    if (custom.defang != null && typeof custom.defang !== 'boolean') {
      return { msg: 'Custom defang must be a boolean when present' };
    }
    if (custom.pivot != null && typeof custom.pivot !== 'boolean') {
      return { msg: 'Custom pivot must be a boolean when present' };
    }
    if (custom.join != null && typeof custom.join !== 'boolean') {
      return { msg: 'Custom join must be a boolean when present when present' };
    }
    if (custom.filterEmpty != null && typeof custom.filterEmpty !== 'boolean') {
      return { msg: 'Custom filterEmpty must be a boolean when present' };
    }
    if (custom.noSearch != null && typeof custom.noSearch !== 'boolean') {
      return { msg: 'Custom noSearch must be a boolean when present' };
    }
    if (custom.postProcess != null && !(typeof custom.postProcess === 'string' || Array.isArray(custom.postProcess) || typeof custom.postProcess === 'object')) {
      return { msg: 'Custom postProcess must be a string, object, or array when present' };
    }

    if (custom.fields != null) {
      for (let i = 0; i < custom.fields.length; i++) {
        const { custom: customSubField, msg } = this.verifyOverviewCustomField(custom.fields[i]);
        if (msg) { return { msg }; }

        custom.fields[i] = customSubField;
      }
    }

    return { custom };
  }

  // Verify the given overview, returns error { msg: string } on failure, { overview } otherwise
  static verifyOverview (overview) {
    overview = (
      ({ // only allow these properties in overviews
        // eslint-disable-next-line no-shadow
        creator, name, title, iType, fields, editRoles, viewRoles
      }) => ({ creator, name, title, iType, fields, editRoles, viewRoles })
    )(overview);

    if (!ArkimeUtil.isString(overview.creator)) {
      return { msg: 'Missing creator' };
    }

    if (!ArkimeUtil.isString(overview.name)) {
      return { msg: 'Missing name' };
    }

    if (!ArkimeUtil.isString(overview.title)) {
      return { msg: 'Missing title' };
    }

    if (!ArkimeUtil.isString(overview.iType)) {
      return { msg: 'Missing iType' };
    }

    if (!iTypes.includes(overview.iType)) {
      return { msg: 'Invalid iType' };
    }

    if (overview.fields === undefined) {
      return { msg: 'Missing fields array' };
    }

    if (overview.viewRoles !== undefined && !ArkimeUtil.isStringArray(overview.viewRoles)) {
      return { msg: 'viewRoles must be an array of strings' };
    }

    if (overview.editRoles !== undefined && !ArkimeUtil.isStringArray(overview.editRoles)) {
      return { msg: 'editRoles must be an array of strings' };
    }

    if (!Array.isArray(overview.fields)) {
      return { msg: 'fields array must be an array' };
    }

    for (let i = 0; i < overview.fields.length; i++) {
      if (typeof overview.fields[i] !== 'object') {
        return { msg: 'Field must be object' };
      }

      overview.fields[i] = (
        ({ // only allow these properties in fields
          // eslint-disable-next-line no-shadow
          from, type, field, alias, custom
        }) => ({ from, type, field, alias, custom })
      )(overview.fields[i]);
      const field = overview.fields[i];

      if (!ArkimeUtil.isString(field.from)) {
        return { msg: 'Field missing from' };
      }

      switch (field.type) {
      case 'linked': // enforce shape { type: 'linked', from, field, alias? }
        if (!ArkimeUtil.isString(field.field)) {
          return { msg: 'Linked field missing field' };
        }
        if (field.alias === '') { field.alias = undefined; }
        if (field.alias !== undefined && !ArkimeUtil.isString(field.alias)) {
          return { msg: 'Linked field alias must be a string or undefined' };
        }
        break;
      case 'custom': { // enforce shape { type: 'custom', from, custom }
        if (field.field !== undefined) {
          return { msg: 'Custom field must not have field' };
        }
        if (field.alias !== undefined) {
          return { msg: 'Custom field must not have alias' };
        }

        // fail if the custom field is invalid
        const { msg, custom } = this.verifyOverviewCustomField(field.custom);
        if (msg) { return { msg }; }

        // we store custom as a json string for storage in db
        field.custom = JSON.stringify(custom);
        break;
      }
      default:
        return { msg: 'Field type must be either "linked" or "custom"' };
      }
    }

    return { overview };
  }

  /**
   * PUT - /api/overview
   *
   * Creates a new overview
   * @name /overview
   * @param {Cont3xtOverview} req.body - The overview to create
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiCreate (req, res, next) {
    const reqOverview = req.body;
    reqOverview.creator = req.user.userId;

    const { overview, msg } = Overview.verifyOverview(reqOverview);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    const results = await Db.putOverview(null, overview);
    if (!results) {
      return res.send({ success: false, text: 'ES Error' });
    }
    return res.send({ success: true, text: 'Success' });
  }

  /**
   * PUT - /api/overview/:id
   *
   * Updates an overview
   * @name /overview/:id
   * @param {string} req.params.id - The id of the overview to update
   * @param {Cont3xtOverview} req.body - The new value of the overview to update
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiUpdate (req, res, next) {
    const oOverview = await Db.getOverview(req.params.id);
    if (!oOverview) {
      return res.send({ success: false, text: 'Overview not found' });
    }

    const reqOverview = req.body;

    // sets the owner if it has changed
    if (!await User.setOwner(req, res, reqOverview, oOverview, 'creator')) {
      return;
    }

    const { overview, msg } = Overview.verifyOverview(reqOverview);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    if (iTypes.includes(req.params.id)) { // iType default overviews
      if (overview.iType !== req.params.id) {
        return res.send({ success: false, text: 'Can not change iType of a default overview' });
      }

      if (overview.viewRoles?.length !== 1 || reqOverview.viewRoles[0] !== 'cont3xtUser') {
        return res.send({ success: false, text: 'Can not change viewRoles of a default overview' });
      }
    }

    try {
      const results = await Db.putOverview(req.params.id, overview);
      if (!results) {
        return res.send({ success: false, text: 'ES Error' });
      }
      return res.send({ success: true, text: 'Success' });
    } catch (err) {
      return res.send({ success: false, text: err.toString() });
    }
  }

  /**
   * GET - /api/overview
   *
   * Returns overviews that the requesting user is allowed to view.
   * @name /overviews
   * @returns {Cont3xtOverview[]} overviews - An array of overviews that the logged in user can view
   * @returns {boolean} success - True if the request was successful, false otherwise
   */
  static async apiGet (req, res, next) {
    const all = req.query.all && req.user.hasRole('cont3xtAdmin');
    const roles = await req.user.getRoles();
    const overviews = await Db.getMatchingOverviews(req.user.userId, [...roles], all);

    // Set editable on any overviews that the user is allowed to edit
    for (const overview of overviews) {
      overview._editable = overview.creator === req.user.userId || req.user.hasRole(overview.editRoles);
      overview._viewable = overview.creator === req.user.userId || req.user.hasRole(overview.viewRoles);
      for (const field of overview.fields) {
        // take best guess at type in case it is missing
        field.type ??= field.custom == null ? 'linked' : 'custom';

        // custom is stored as a json string, so we parse it before sending back
        if (field.type === 'custom') {
          try {
            field.custom = JSON.parse(field.custom);
          } catch {
            // we'd rather not crash the service if the custom field is somehow invalid
            field.custom = '_cont3xt_INVALID_CUSTOM_FIELD';
          }
        }
      }
    }

    res.send({ success: true, overviews });
  }

  /**
   * DELETE - /api/overview/:id
   *
   * Deletes an overview
   * @name /overview/:id
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiDelete (req, res, next) {
    if (iTypes.includes(req.params.id)) { // no deleting default overview for iType
      return res.send({ success: false, text: 'Can not delete iType default' });
    }

    const overview = await Db.getOverview(req.params.id);
    if (!overview) {
      return res.send({ success: false, text: 'Overview not found' });
    }

    const results = await Db.deleteOverview(req.params.id, req.body);
    if (!results) {
      return res.send({ success: false, text: 'ES Error' });
    }
    return res.send({ success: true, text: 'Success' });
  }
}

const defaultOverviewPropertiesForIType = {
  domain: {
    name: 'Default Domain Overview',
    title: 'Domain Overview for %{query}',
    fields: [
      {
        type: 'linked',
        from: 'VT Domain',
        field: 'Forcepoint ThreatSeeker category',
        alias: 'VT Forcepoint'
      },
      {
        type: 'linked',
        from: 'VT Domain',
        field: 'Webutation domain info',
        alias: 'VT Webutation'
      },
      {
        type: 'linked',
        from: 'VT Domain',
        field: 'Sophos category',
        alias: 'VT Sophos'
      },
      {
        type: 'linked',
        from: 'PT DNS',
        field: 'firstSeen',
        alias: 'PT PDNS First Seen'
      },
      {
        type: 'linked',
        from: 'PT Whois',
        field: 'expiresAt',
        alias: 'Domain Expiry date'
      },
      {
        type: 'linked',
        from: 'PT Whois',
        field: 'contactEmail',
        alias: 'PT contact email'
      },
      {
        type: 'linked',
        from: 'PT Whois',
        field: 'organization',
        alias: 'PT organization'
      },
      {
        type: 'linked',
        from: 'Threatstream',
        field: 'Objects',
        alias: 'ThreatStream events'
      },
      {
        type: 'linked',
        from: 'AlienVaultOTX',
        field: 'Pulses:',
        alias: 'OTX Pulses'
      },
      {
        type: 'linked',
        from: 'URLScan',
        field: 'results',
        alias: 'URLScan results'
      },
      {
        type: 'linked',
        from: 'VT Domain',
        field: 'detected_urls',
        alias: 'VT detected urls'
      },
      {
        type: 'linked',
        from: 'PT DNS',
        field: 'results',
        alias: 'PT PDNS'
      }
    ]
  },
  ip: {
    name: 'Default IP Overview',
    title: 'IP Overview for %{query}',
    fields: [
      {
        type: 'linked',
        from: 'AbuseIPDB',
        field: 'usageType',
        alias: 'AbuseIPDB classification'
      },
      {
        type: 'linked',
        from: 'Spur',
        field: 'infrastructure',
        alias: 'Spur Infra:'
      },
      {
        type: 'linked',
        from: 'GreyNoise',
        field: 'classification',
        alias: 'GreyNoise classification'
      },
      {
        type: 'linked',
        from: 'BGPView',
        field: 'Prefixes',
        alias: 'BGPView prefixes'
      },
      {
        type: 'linked',
        from: 'Censys',
        field: 'Services',
        alias: 'Censys Open Port info'
      },
      {
        type: 'linked',
        from: 'Censys',
        field: 'Certificates',
        alias: 'Censys Certificates'
      },
      {
        type: 'linked',
        from: 'Shodan',
        field: 'Service/Port info',
        alias: 'Shodan Open Port info'
      },
      {
        type: 'linked',
        from: 'Shodan',
        field: 'Certificates',
        alias: 'Shodan Certificates'
      },
      {
        type: 'linked',
        from: 'AbuseIPDB',
        field: 'Reports:',
        alias: 'AbuseIPDB reports'
      },
      {
        type: 'linked',
        from: 'URLScan',
        field: 'results',
        alias: 'URLScan results'
      },
      {
        type: 'linked',
        from: 'VT IP',
        field: 'detected_urls',
        alias: 'VT detected urls'
      },
      {
        type: 'linked',
        from: 'Threatstream',
        field: 'Objects',
        alias: 'Threatstream tags'
      }
    ]
  },
  url: {
    name: 'Default URL Overview',
    title: 'URL Overview for %{query}',
    fields: [
      {
        type: 'linked',
        from: 'Threatstream',
        field: 'Objects',
        alias: 'ThreatStream imports'
      }
    ]
  },
  email: {
    name: 'Default Email Overview',
    title: 'Email Overview for %{query}',
    fields: [
      {
        type: 'linked',
        from: 'Threatstream',
        field: 'Objects',
        alias: 'Threatstream imports'
      }
    ]
  },
  phone: {
    name: 'Default Phone Overview',
    title: 'Phone Overview for %{query}',
    fields: [
      {
        type: 'linked',
        from: 'Twilio',
        field: 'caller_name',
        alias: 'Caller Name'
      },
      {
        type: 'linked',
        from: 'Twilio',
        field: 'carrier',
        alias: 'Carrier'
      }
    ]
  },
  hash: {
    name: 'Default Hash Overview',
    title: 'Hash Overview for %{query}',
    fields: [
      {
        type: 'linked',
        from: 'VT Hash',
        field: 'positives',
        alias: 'VT Hash malicious scan count'
      },
      {
        type: 'linked',
        from: 'VT Hash',
        field: 'scan_date',
        alias: 'VT scan date'
      },
      {
        type: 'linked',
        from: 'VT Hash',
        field: 'permalink',
        alias: 'VT link'
      },
      {
        type: 'linked',
        from: 'VT Hash',
        field: 'scans',
        alias: 'VT scan results'
      },
      {
        type: 'linked',
        from: 'Threatstream',
        field: 'Objects',
        alias: 'Threatstream tags'
      }
    ]
  },
  text: {
    name: 'Default Text Overview',
    title: 'Text Overview for %{query}',
    fields: []
  }
};

module.exports = Overview;

// Must be at bottom to avoid circular dependency
const Db = require('./db.js');
