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
   * Return LinkGroups user can see
   */
  static async apiGet (req, res, next) {
    const linkGroups = await Db.getMatchingLinkGroups(req.user.userId, [...req.user.getRoles()]);

    // Set editable on any linkGroups that the user is allowed to edit
    for (const lg of linkGroups) {
      lg._editable = lg.creator === req.user.userId || req.user.hasRole(lg.editRoles);
      lg._viewable = lg.creator === req.user.userId || req.user.hasRole(lg.viewRoles);
    }

    res.send({ success: true, linkGroups: linkGroups });
  }

  /**
   * Verify the link group, returns error msg on failure
   */
  static verifyLinkGroup (lg) {
    // TODO: Check roles

    if (typeof (lg.name) !== 'string') {
      return 'Missing name';
    }

    if (!Array.isArray(lg.links)) {
      return 'Missing list of links';
    }

    for (const link of lg.links) {
      if (typeof (link.name) !== 'string') {
        return 'Link missing name';
      }
      if (typeof (link.url) !== 'string') {
        return 'Link missing url';
      }
      if (!Array.isArray(link.itypes)) {
        return 'Link missing itypes';
      }
    }

    return null;
  }

  /**
   * Create new link group
   */
  static async apiCreate (req, res, next) {
    /* if (!req.user.hasRole('cont3xtUser')) {
      return res.send({ success: false, text: 'Permission denied' });
    } */

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
   * Update new link group
   */
  static async apiUpdate (req, res, next) {
    const olinkGroup = await Db.getLinkGroup(req.params.id);
    if (!olinkGroup) {
      return res.send({ success: false, text: 'LinkGroup not found' });
    }

    if (olinkGroup.creator !== req.user.userId && !req.user.hasRole(olinkGroup.editRoles)) {
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
   * Delete a link group
   */
  static async apiDelete (req, res, next) {
    const linkGroup = await Db.getLinkGroup(req.params.id);
    if (!linkGroup) {
      return res.send({ success: false, text: 'LinkGroup not found' });
    }

    if (linkGroup.creator !== req.user.userId && !req.user.hasRole(linkGroup.editRoles)) {
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
