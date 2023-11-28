/******************************************************************************/
/* linkGroup.js  -- Deal with a group of links
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const User = require('../common/user');
const ArkimeUtil = require('../common/arkimeUtil');

class LinkGroup {
  constructor (data) {
    Object.assign(this, data);
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
    lg = (
      ({ // only allow these properties in link groups
        // eslint-disable-next-line no-shadow
        name, links, viewRoles, editRoles, creator
      }) => ({ name, links, viewRoles, editRoles, creator })
    )(lg);

    if (!ArkimeUtil.isString(lg.name)) {
      return { msg: 'Missing name' };
    }

    if (!Array.isArray(lg.links)) {
      return { msg: 'Missing list of links' };
    }

    if (lg.viewRoles !== undefined && !ArkimeUtil.isStringArray(lg.viewRoles)) {
      return { msg: 'viewRoles must be an array of strings' };
    }

    if (lg.editRoles !== undefined && !ArkimeUtil.isStringArray(lg.editRoles)) {
      return { msg: 'editRoles must be an array of strings' };
    }

    if (lg.editRoles !== undefined) {
      if (!Array.isArray(lg.editRoles)) {
        return { msg: 'editRoles must be array' };
      }

      for (const editRole of lg.editRoles) {
        if (!ArkimeUtil.isString(editRole)) {
          return { msg: 'editRoles must contain strings' };
        }
      }
    }

    for (let i = 0; i < lg.links.length; i++) {
      const link = lg.links[i];
      lg.links[i] = (
        ({ // only allow these properties in links
          // eslint-disable-next-line no-shadow
          name, url, itypes, color, infoField, externalDocName, externalDocUrl
        }) => ({ name, url, itypes, color, infoField, externalDocName, externalDocUrl })
      )(link);

      if (typeof link !== 'object') {
        return { msg: 'Link must be object' };
      }
      if (!ArkimeUtil.isString(link.name)) {
        return { msg: 'Link missing name' };
      }
      if (!ArkimeUtil.isString(link.url)) {
        return { msg: 'Link missing url' };
      }
      if (!Array.isArray(link.itypes)) {
        return { msg: 'Link missing itypes' };
      }
      for (const itype of link.itypes) {
        if (!ArkimeUtil.isString(itype)) {
          return { msg: 'Link itypes must be strings' };
        }
      }
      if (link.color !== undefined && !ArkimeUtil.isString(link.color)) {
        return { msg: 'Link color must be a string' };
      }
      if (link.infoField !== undefined && !ArkimeUtil.isString(link.infoField)) {
        return { msg: 'Link infoField must be a string' };
      }
      if (link.externalDocName !== undefined && !ArkimeUtil.isString(link.externalDocName)) {
        return { msg: 'Link externalDocName must be a string' };
      }
      if (link.externalDocUrl !== undefined && !ArkimeUtil.isString(link.externalDocUrl)) {
        return { msg: 'Link externalDocUrl must be a string' };
      }
    }

    return { lg };
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

    const { lg, msg } = LinkGroup.verifyLinkGroup(linkGroup);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    const results = await Db.putLinkGroup(null, lg);
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

    const linkGroup = req.body;

    const { lg, msg } = LinkGroup.verifyLinkGroup(linkGroup);
    if (msg) {
      return res.send({ success: false, text: msg });
    }

    // sets the owner if it has changed
    if (!await User.setOwner(req, res, lg, olinkGroup, 'creator')) {
      return;
    }

    try {
      const results = await Db.putLinkGroup(req.params.id, lg);
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
