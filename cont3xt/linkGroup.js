/******************************************************************************/
/* linkGroup.js  -- Deal with a group of links
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

class LinkGroup {
  constructor (data) {
    Object.assign(this, data);
  }

  save () {
  }

  /**
   * A Link object
   *
   * Links are used to navigate to external sources.
   * @typedef Link
   * @type {object}
   * @param {string} name - The name of the link
   * @param {string} color - The color of the link
   * @param {string[]} itypes - The type of cont3xt results that pertain to this link
   * @param {string} url - The url of the link. Links can include placeholder values that will be filled in with the data from the Cont3xt results
   * @param {string} infoField - An optional text field to display as an informative tooltip.
   * @param {string} externalDocUrl - An optional URL to link out to external documentation.
   * @param {string} externalDocName - An optional name to label the external documentation.
   */

  /**
   * A Link Group object
   *
   * Link Groups are used to list links to external sources.
   * @typedef LinkGroup
   * @type {object}
   * @param {string} _id - The id of the link group
   * @param {string} name - The name of the link group
   * @param {string} creator - The creator of the link group
   * @param {Links[]} links - The array of links in this link group
   * @param {array} editRoles - The Arkime roles that can edit this link group
   * @param {array} viewRoles - The Arkime roles that can view this link group
   * @param {boolean} _editable - Whether the logged in user is allowed to edit this link group
   * @param {boolean} _viewable - Whether the logged in user is allowed to view this link group
   */

  /**
   * GET - /api/linkGroup
   *
   * Returns link groups that the requesting user is allowed to view.
   * @name /linkGroup
   * @returns {LinkGroup[]} linkGroups - An array of link groups that the logged in user can view
   * @returns {boolean} success - True if the request was successful, false otherwise
   */
  static async apiGet (req, res, next) {
    const all = req.query.all && req.user.hasRole('cont3xtAdmin');
    const roles = await req.user.getRoles();
    let linkGroups = await Db.getMatchingLinkGroups(req.user.userId, [...roles], all);

    // Set editable on any linkGroups that the user is allowed to edit
    for (const lg of linkGroups) {
      lg._editable = lg.creator === req.user.userId || req.user.hasRole(lg.editRoles);
      lg._viewable = lg.creator === req.user.userId || req.user.hasRole(lg.viewRoles);
    }

    const reordered = [];
    const unordered = [];
    const settings = req.user.cont3xt ?? {};
    if (settings.linkGroup && settings.linkGroup.order) {
      for (const group of linkGroups) {
        if (settings.linkGroup.order.indexOf(group._id) === -1) {
          unordered.push(group);
        }
      }

      for (const id of settings.linkGroup.order) {
        for (const group of linkGroups) {
          if (group._id === id) {
            reordered.push(group);
          }
        }
      }
    }

    if (reordered.length) {
      linkGroups = reordered.concat(unordered);
    }

    res.send({ success: true, linkGroups });
  }

  // Verify the link group, returns error msg on failure
  static verifyLinkGroup (lg) {
    // TODO: Check roles

    if (typeof (lg.name) !== 'string') {
      return 'Missing name';
    }

    if (!Array.isArray(lg.links)) {
      return 'Missing list of links';
    }

    if (lg.viewRoles !== undefined && !Array.isArray(lg.viewRoles)) {
      return 'viewRoles must be array';
    }

    for (const viewRole of lg.viewRoles) {
      if (typeof viewRole !== 'string') {
        return 'viewRoles must contain strings';
      }
    }

    if (lg.editRoles !== undefined && !Array.isArray(lg.editRoles)) {
      return 'editRoles must be array';
    }

    for (const editRole of lg.editRoles) {
      if (typeof editRole !== 'string') {
        return 'editRoles must contain strings';
      }
    }

    for (const link of lg.links) {
      if (typeof link !== 'object') {
        return 'Link must be object';
      }
      if (typeof (link.name) !== 'string') {
        return 'Link missing name';
      }
      if (typeof (link.url) !== 'string') {
        return 'Link missing url';
      }
      if (!Array.isArray(link.itypes)) {
        return 'Link missing itypes';
      }
      for (const itype of link.itypes) {
        if (typeof itype !== 'string') {
          return 'Link itypes must be strings';
        }
      }
      if (link.infoField !== undefined && typeof link.infoField !== 'string') {
        return 'Link infoField must be a string';
      }
      if (link.externalDocName !== undefined && typeof link.externalDocName !== 'string') {
        return 'Link externalDocName must be a string';
      }
      if (link.externalDocUrl !== undefined && typeof link.externalDocUrl !== 'string') {
        return 'Link externalDocUrl must be a string';
      }
    }

    return null;
  }

  /**
   * PUT - /api/linkGroup
   *
   * Creates a new link group
   * @name /linkGroup
   * @param {LinkGroup} linkGroup - The link group to create
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiCreate (req, res, next) {
    const linkGroup = req.body;
    linkGroup.creator = req.user.userId;

    const msg = LinkGroup.verifyLinkGroup(linkGroup);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    const results = await Db.putLinkGroup(null, linkGroup);
    if (!results) {
      return res.send({ success: false, text: 'ES Error' });
    }
    return res.send({ success: true, text: 'Success' });
  }

  /**
   * PUT - /api/linkGroup/:id
   *
   * Updates a link group
   * @name /linkGroup/:id
   * @param {LinkGroup} linkGroup - The link group to update
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiUpdate (req, res, next) {
    const olinkGroup = await Db.getLinkGroup(req.params.id);
    if (!olinkGroup) {
      return res.send({ success: false, text: 'LinkGroup not found' });
    }

    if (olinkGroup.creator !== req.user.userId && !(req.user.hasRole(olinkGroup.editRoles)) && !req.user.hasRole('cont3xtAdmin')) {
      return res.send({ success: false, text: 'Permission denied' });
    }

    const linkGroup = req.body;
    linkGroup.creator = olinkGroup.creator; // Make sure the creator doesn't get changed

    const msg = LinkGroup.verifyLinkGroup(linkGroup);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    try {
      const results = await Db.putLinkGroup(req.params.id, linkGroup);
      if (!results) {
        return res.send({ success: false, text: 'ES Error' });
      }
      return res.send({ success: true, text: 'Success' });
    } catch (err) {
      return res.send({ success: false, text: err.toString() });
    }
  }

  /**
   * DELETE - /api/linkGroup/:id
   *
   * Deletes a link group
   * @name /linkGroup/:id
   * @returns {boolean} success - True if the request was successful, false otherwise
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  static async apiDelete (req, res, next) {
    const linkGroup = await Db.getLinkGroup(req.params.id);
    if (!linkGroup) {
      return res.send({ success: false, text: 'LinkGroup not found' });
    }

    if (linkGroup.creator !== req.user.userId && !(req.user.hasRole(linkGroup.editRoles)) && !req.user.hasRole('cont3xtAdmin')) {
      return res.send({ success: false, text: 'Permission denied' });
    }

    const results = await Db.deleteLinkGroup(req.params.id, req.body);
    if (!results) {
      return res.send({ success: false, text: 'ES Error' });
    }
    return res.send({ success: true, text: 'Success' });
  }
}

module.exports = LinkGroup;

// Must be at bottom to avoid circular dependency
const Db = require('./db.js');
