/* apiShareables.js -- api calls for shareables
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Db = require('./db');
const util = require('util');
const ArkimeUtil = require('../common/arkimeUtil');

class ShareableAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------

  /**
   * Check if a user can view a shareable item
   */
  static async canView (user, shareable) {
    // Creator can always view
    if (user.userId === shareable.creator) {
      return true;
    }

    // Check viewUsers
    if (shareable.viewUsers && shareable.viewUsers.includes(user.userId)) {
      return true;
    }

    // Check viewRoles
    if (shareable.viewRoles && user.hasRole(shareable.viewRoles)) {
      return true;
    }

    return false;
  }

  /**
   * Check if a user can edit a shareable item
   */
  static async canEdit (user, shareable) {
    // Creator can always edit
    if (user.userId === shareable.creator) {
      return true;
    }

    // Check editUsers
    if (shareable.editUsers && shareable.editUsers.includes(user.userId)) {
      return true;
    }

    // Check editRoles
    if (shareable.editRoles && user.hasRole(shareable.editRoles)) {
      return true;
    }

    return false;
  }

  /**
   * Check if a user can delete a shareable item
   */
  static canDelete (user, shareable) {
    // Creator can delete
    if (user.userId === shareable.creator) {
      return true;
    }

    // arkimeAdmin can delete
    if (user.hasRole('arkimeAdmin')) {
      return true;
    }

    return false;
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------

  /**
   * POST - /api/shareable
   *
   * Creates a shareable item.
   * @name /shareable
   * @returns {boolean} success - Whether the create operation was successful.
   * @returns {string} text - The success/error message.
   * @returns {string} id - The ID of the new shareable item.
   * @returns {object} shareable - The new shareable item.
   */
  static async apiCreateShareable (req, res) {
    if (!ArkimeUtil.isString(req.body.name)) {
      return res.serverError(403, 'Missing shareable name');
    }

    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing shareable type');
    }

    const user = req.settingUser;

    const viewRoles = ArkimeUtil.isStringArray(req.body.viewRoles) ? req.body.viewRoles : [];
    const viewUsers = ArkimeUtil.isStringArray(req.body.viewUsers) ? req.body.viewUsers : [];
    const editRoles = ArkimeUtil.isStringArray(req.body.editRoles) ? req.body.editRoles : [];
    const editUsers = ArkimeUtil.isStringArray(req.body.editUsers) ? req.body.editUsers : [];

    const doc = {
      name: req.body.name,
      type: req.body.type,
      creator: user.userId,
      created: new Date(),
      updated: new Date(),
      viewRoles,
      viewUsers,
      editRoles,
      editUsers,
      data: req.body.data || {}
    };

    try {
      const { body: { _id: id } } = await Db.createShareable(doc);
      const { body: { _source: shareable } } = await Db.getShareable(id);

      shareable.id = id;

      return res.send(JSON.stringify({
        success: true,
        shareable,
        text: 'Created shareable item!',
        id
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable (createShareable)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error creating shareable item');
    }
  }

  /**
   * GET - /api/shareable/:id
   *
   * Gets a shareable item by ID. User must have view permission.
   * @name /shareable/:id
   * @returns {boolean} success - Whether the get operation was successful.
   * @returns {object} shareable - The shareable item.
   */
  static async apiGetShareable (req, res) {
    try {
      const { body: { _source: shareable } } = await Db.getShareable(req.params.id);
      const user = req.settingUser;

      if (!await ShareableAPIs.canView(user, shareable)) {
        return res.serverError(403, 'Not permitted to view this shareable item');
      }

      shareable.id = req.params.id;
      shareable.canEdit = await ShareableAPIs.canEdit(user, shareable);
      shareable.canDelete = ShareableAPIs.canDelete(user, shareable);

      return res.send({
        success: true,
        shareable
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable/%s (getShareable)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error retrieving shareable item');
    }
  }

  /**
   * PUT - /api/shareable/:id
   *
   * Updates a shareable item. Only creator or editUsers/editRoles can update.
   * Creator and created date are not changed. Updated date is set to now.
   * @name /shareable/:id
   * @returns {boolean} success - Whether the update operation was successful.
   * @returns {string} text - The success/error message.
   * @returns {object} shareable - The updated shareable item.
   */
  static async apiUpdateShareable (req, res) {
    try {
      const { body: dbItem } = await Db.getShareable(req.params.id);
      const user = req.settingUser;

      if (!await ShareableAPIs.canEdit(user, dbItem._source)) {
        return res.serverError(403, 'Not permitted to edit this shareable item');
      }

      if (req.body.type !== undefined && req.body.type !== dbItem._source.type) {
        return res.serverError(403, 'Cannot change shareable type');
      }

      const viewRoles = req.body.viewRoles !== undefined ? (ArkimeUtil.isStringArray(req.body.viewRoles) ? req.body.viewRoles : []) : (dbItem._source.viewRoles || []);
      const viewUsers = req.body.viewUsers !== undefined ? (ArkimeUtil.isStringArray(req.body.viewUsers) ? req.body.viewUsers : []) : (dbItem._source.viewUsers || []);
      const editRoles = req.body.editRoles !== undefined ? (ArkimeUtil.isStringArray(req.body.editRoles) ? req.body.editRoles : []) : (dbItem._source.editRoles || []);
      const editUsers = req.body.editUsers !== undefined ? (ArkimeUtil.isStringArray(req.body.editUsers) ? req.body.editUsers : []) : (dbItem._source.editUsers || []);

      const doc = {
        name: req.body.name !== undefined ? req.body.name : dbItem._source.name,
        type: dbItem._source.type,
        creator: dbItem._source.creator,
        created: dbItem._source.created,
        updated: new Date(),
        viewRoles,
        viewUsers,
        editRoles,
        editUsers,
        data: req.body.data !== undefined ? req.body.data : (dbItem._source.data || {})
      };

      await Db.setShareable(req.params.id, doc);
      const { body: { _source: shareable } } = await Db.getShareable(req.params.id);

      shareable.id = req.params.id;
      shareable.canEdit = await ShareableAPIs.canEdit(user, shareable);
      shareable.canDelete = ShareableAPIs.canDelete(user, shareable);

      return res.send(JSON.stringify({
        success: true,
        shareable,
        text: 'Updated shareable item!'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable/%s (updateShareable)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error updating shareable item');
    }
  }

  /**
   * DELETE - /api/shareable/:id
   *
   * Deletes a shareable item. Only creator or arkimeAdmin can delete.
   * @name /shareable/:id
   * @returns {boolean} success - Whether the delete operation was successful.
   * @returns {string} text - The success/error message.
   */
  static async apiDeleteShareable (req, res) {
    try {
      const { body: { _source: shareable } } = await Db.getShareable(req.params.id);
      const user = req.settingUser;

      if (!ShareableAPIs.canDelete(user, shareable)) {
        return res.serverError(403, 'Not permitted to delete this shareable item');
      }

      await Db.deleteShareable(req.params.id);
      res.send(JSON.stringify({
        success: true,
        text: 'Deleted shareable item successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable/%s (deleteShareable)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting shareable item');
    }
  }

  /**
   * GET - /api/shareables
   *
   * Lists shareable items. Returns items user has permission to view/edit.
   * @name /shareables
   * @returns {array} data - Array of shareable items with { id, type, name, data, canEdit, canDelete }
   * @returns {number} recordsTotal - Total records matching query
   * @returns {number} recordsFiltered - Total records after filtering
   */
  static async apiListShareables (req, res) {
    try {
      const user = req.settingUser;
      if (!user) {
        return res.send({ data: [], recordsTotal: 0, recordsFiltered: 0 });
      }

      if (!ArkimeUtil.isString(req.query.type)) {
        return res.serverError(403, 'Missing or invalid type parameter');
      }

      const type = req.query.type;
      const viewOnly = req.query.viewOnly !== 'false';
      const userRoles = [...await user.getRoles()];

      // Build query
      const query = {
        query: {
          bool: {
            must: [
              { term: { type } }
            ],
            filter: []
          }
        },
        sort: { name: { order: 'asc' } },
        from: req.query.start || 0,
        size: req.query.length || 50
      };

      // Build permission filter
      const permissionFilters = [];

      // Creator filter
      permissionFilters.push({ term: { creator: user.userId } });

      if (viewOnly) {
        // viewUsers filter
        permissionFilters.push({ term: { viewUsers: user.userId } });
        // viewRoles filter
        if (userRoles.length > 0) {
          permissionFilters.push({ terms: { viewRoles: userRoles } });
        }
      } else {
        // editUsers filter
        permissionFilters.push({ term: { editUsers: user.userId } });
        // editRoles filter
        if (userRoles.length > 0) {
          permissionFilters.push({ terms: { editRoles: userRoles } });
        }
        // viewUsers filter
        permissionFilters.push({ term: { viewUsers: user.userId } });
        // viewRoles filter
        if (userRoles.length > 0) {
          permissionFilters.push({ terms: { viewRoles: userRoles } });
        }
      }

      query.query.bool.filter.push({
        bool: {
          should: permissionFilters,
          minimum_should_match: 1
        }
      });

      const { body: { hits: items } } = await Db.searchShareables(query);

      delete query.sort;
      delete query.size;
      delete query.from;
      const { body: { count: total } } = await Db.numberOfShareables(query);

      const results = items.hits.map(async (item) => {
        const shareable = item._source;
        return {
          id: item._id,
          type: shareable.type,
          name: shareable.name,
          data: shareable.data,
          canEdit: await ShareableAPIs.canEdit(user, shareable),
          canDelete: ShareableAPIs.canDelete(user, shareable)
        };
      });

      const resolvedResults = await Promise.all(results);

      return res.send({
        data: resolvedResults,
        recordsTotal: items.total,
        recordsFiltered: total
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareables (listShareables)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error listing shareable items');
    }
  }
}

module.exports = ShareableAPIs;
