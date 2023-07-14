/******************************************************************************/
/* overview.js  -- Cont3xt overviews
 *
 * Copyright Yahoo Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

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
          console.log(`ERROR - failed to create default overview for ${iType} iType... skipping`, err);
        });
      }).catch((err) => {
        console.log(`ERROR - failed to check existence of ${iType} iType default overview... skipping:`, err);
      });
    }
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
          from, field, alias
        }) => ({ from, field, alias })
      )(overview.fields[i]);
      const field = overview.fields[i];

      if (!ArkimeUtil.isString(field.from)) {
        return { msg: 'Field missing from' };
      }
      if (!ArkimeUtil.isString(field.field)) {
        return { msg: 'Field missing field' };
      }

      if (field.alias === '') {
        field.alias = undefined;
      }

      if (field.alias !== undefined && !ArkimeUtil.isString(field.alias)) {
        return { msg: 'Field alias must be a string or undefined' };
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

    if (oOverview.creator !== req.user.userId && !(req.user.hasRole(oOverview.editRoles)) && !req.user.hasRole('cont3xtAdmin')) {
      return res.send({ success: false, text: 'Permission denied' });
    }

    const reqOverview = req.body;
    reqOverview.creator = oOverview.creator; // Make sure the creator doesn't get changed

    if (iTypes.includes(req.params.id) && reqOverview.iType !== req.params.id) {
      return res.send({ success: false, text: 'Can not change iType of a default overview' });
    }

    const { overview, msg } = Overview.verifyOverview(reqOverview);
    if (msg) {
      return res.send({ success: false, text: msg });
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

    if (overview.creator !== req.user.userId && !(req.user.hasRole(overview.editRoles)) && !req.user.hasRole('cont3xtAdmin')) {
      return res.send({ success: false, text: 'Permission denied' });
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
    name: 'Domain_card',
    title: 'Domain info for %{query}',
    fields: [
      {
        from: 'VT Domain',
        field: 'Forcepoint ThreatSeeker category',
        alias: 'VT Forcepoint'
      },
      {
        from: 'VT Domain',
        field: 'Webutation domain info',
        alias: 'VT Webutation'
      },
      {
        from: 'VT Domain',
        field: 'Sophos category',
        alias: 'VT Sophos'
      },
      {
        from: 'PT DNS',
        field: 'firstSeen',
        alias: 'PT PDNS First Seen'
      },
      {
        from: 'PT Whois',
        field: 'expiresAt',
        alias: 'Domain Expiry date'
      },
      {
        from: 'PT Whois',
        field: 'contactEmail',
        alias: 'PT contact email'
      },
      {
        from: 'PT Whois',
        field: 'organization',
        alias: 'PT organization'
      },
      {
        from: 'Threatstream',
        field: 'Objects',
        alias: 'ThreatStream events'
      },
      {
        from: 'AlienVaultOTX',
        field: 'Pulses:',
        alias: 'OTX Pulses'
      },
      {
        from: 'URLScan',
        field: 'results',
        alias: 'URLScan results'
      },
      {
        from: 'VT Domain',
        field: 'detected_urls',
        alias: 'VT detected urls'
      },
      {
        from: 'PT DNS',
        field: 'results',
        alias: 'PT PDNS'
      }
    ]
  },
  ip: {
    name: 'IP_card',
    title: 'IP Overview for %{query}',
    fields: [
      {
        from: 'AbuseIPDB',
        field: 'usageType',
        alias: 'AbuseIPDB classification'
      },
      {
        from: 'Spur',
        field: 'infrastructure',
        alias: 'Spur Infra:'
      },
      {
        from: 'GreyNoise',
        field: 'classification',
        alias: 'GreyNoise classification'
      },
      {
        from: 'BGPView',
        field: 'Prefixes',
        alias: 'BGPView prefixes'
      },
      {
        from: 'Censys',
        field: 'Services',
        alias: 'Censys Open Port info'
      },
      {
        from: 'Censys',
        field: 'Certificates',
        alias: 'Censys Certificates'
      },
      {
        from: 'Shodan',
        field: 'Service/Port info',
        alias: 'Shodan Open Port info'
      },
      {
        from: 'Shodan',
        field: 'Certificates',
        alias: 'Shodan Certificates'
      },
      {
        from: 'AbuseIPDB',
        field: 'Reports:',
        alias: 'AbuseIPDB reports'
      },
      {
        from: 'URLScan',
        field: 'results',
        alias: 'URLScan results'
      },
      {
        from: 'VT IP',
        field: 'detected_urls',
        alias: 'VT detected urls'
      },
      {
        from: 'Threatstream',
        field: 'Objects',
        alias: 'Threatstream tags'
      }
    ]
  },
  url: {
    name: 'URL_card',
    title: 'URL info for:  %{query}',
    fields: [
      {
        from: 'Threatstream',
        field: 'Objects',
        alias: 'ThreatStream imports'
      }
    ]
  },
  email: {
    name: 'Email_card',
    title: 'Email info for  %{query}',
    fields: [
      {
        from: 'Threatstream',
        field: 'Objects',
        alias: 'Threatstream imports'
      }
    ]
  },
  phone: {
    name: 'Phone_card',
    title: 'Phone info for %{query}',
    fields: [
      {
        from: 'Twilio',
        field: 'caller_name',
        alias: 'Caller Name'
      },
      {
        from: 'Twilio',
        field: 'carrier',
        alias: 'Carrier'
      }
    ]
  },
  hash: {
    name: 'Hash_card',
    title: 'Hash Overview for %{query}',
    fields: [
      {
        from: 'VT Hash',
        field: 'positives',
        alias: 'VT Hash malicious scan count'
      },
      {
        from: 'VT Hash',
        field: 'scan_date',
        alias: 'VT scan date'
      },
      {
        from: 'VT Hash',
        field: 'permalink',
        alias: 'VT link'
      },
      {
        from: 'VT Hash',
        field: 'scans',
        alias: 'VT scan results'
      },
      {
        from: 'Threatstream',
        field: 'Objects',
        alias: 'Threatstream tags'
      }
    ]
  },
  text: {
    name: 'Text_card',
    title: 'Text Overview for %{query}',
    fields: []
  }
};

module.exports = Overview;

// Must be at bottom to avoid circular dependency
const Db = require('./db.js');
