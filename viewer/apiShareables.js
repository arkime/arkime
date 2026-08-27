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
const User = require('../common/user');

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

  /**
   * Per-type shape validation for shareable `data`. Types not listed here
   * (eg views, dashboards) have no shape requirement.
   * @returns {object|undefined} { text, i18n }, or undefined if valid.
   */
  static validateData (type, data) {
    const missingColumns = { text: 'Missing columns', i18n: 'api.shareables.missingColumns' };
    const missingSortOrder = { text: 'Missing sort order', i18n: 'api.shareables.missingSortOrder' };
    const missingFields = { text: 'Missing fields', i18n: 'api.shareables.missingFields' };

    switch (type) {
    case 'sessionsTableLayout':
      if (!Array.isArray(data.columns)) { return missingColumns; }
      if (!Array.isArray(data.order)) { return missingSortOrder; }
      break;
    case 'sessionsInfoLayout':
      if (!Array.isArray(data.fields)) { return missingFields; }
      break;
    case 'spiviewLayout':
      if (typeof data.fields !== 'string') { return missingFields; }
      break;
    }
    return undefined;
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
      return res.serverError(403, 'Missing shareable name', 'api.shareables.missingName');
    }

    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing shareable type', 'api.shareables.missingType');
    }

    // empty is allowed so a description can be cleared
    if (req.body.description !== undefined && typeof req.body.description !== 'string') {
      return res.serverError(403, 'Description must be a string', 'api.shareables.descriptionMustBeString');
    }

    const user = req.settingUser;

    const viewRoles = ArkimeUtil.isStringArray(req.body.viewRoles) ? req.body.viewRoles : [];
    let viewUsers = ArkimeUtil.isStringArray(req.body.viewUsers) ? req.body.viewUsers : [];
    const editRoles = ArkimeUtil.isStringArray(req.body.editRoles) ? req.body.editRoles : [];
    let editUsers = ArkimeUtil.isStringArray(req.body.editUsers) ? req.body.editUsers : [];

    if (viewUsers.length > 0) {
      const validatedViewUsers = await User.validateUserIds(viewUsers);
      viewUsers = validatedViewUsers.validUsers;
    }

    if (editUsers.length > 0) {
      const validatedEditUsers = await User.validateUserIds(editUsers);
      editUsers = validatedEditUsers.validUsers;
    }

    if (req.body.data !== undefined && (typeof req.body.data !== 'object' || Array.isArray(req.body.data) || req.body.data === null)) {
      return res.serverError(403, 'Data must be an object');
    }

    const data = req.body.data || {};
    const dataError = ShareableAPIs.validateData(req.body.type, data);
    if (dataError) {
      return res.serverError(403, dataError.text, dataError.i18n);
    }

    const doc = {
      name: req.body.name,
      description: req.body.description,
      type: req.body.type,
      creator: user.userId,
      created: new Date(),
      updated: new Date(),
      viewRoles,
      viewUsers,
      editRoles,
      editUsers,
      data
    };

    try {
      const id = await Db.createShareable(doc);
      const shareable = await Db.getShareable(id);

      shareable.id = id;

      return res.json({
        success: true,
        shareable,
        text: 'Created shareable item!',
        id,
        i18n: 'api.shareables.created'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable (createShareable)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error creating shareable item', 'api.shareables.errorCreating');
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
      const shareable = await Db.getShareable(req.params.id);
      const user = req.settingUser;

      if (!await ShareableAPIs.canView(user, shareable)) {
        return res.serverError(403, 'Not permitted to view this shareable item', 'api.shareables.notPermittedToView');
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
      return res.serverError(500, 'Error retrieving shareable item', 'api.shareables.retrieveFailed');
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
      const shareable = await Db.getShareable(req.params.id);
      const user = req.settingUser;

      if (!await ShareableAPIs.canEdit(user, shareable)) {
        return res.serverError(403, 'Not permitted to edit this shareable item', 'api.shareables.notPermittedToEdit');
      }

      if (req.body.type !== undefined && req.body.type !== shareable.type) {
        return res.serverError(403, 'Cannot change shareable type', 'api.shareables.cannotChangeType');
      }

      if (req.body.name !== undefined && !ArkimeUtil.isString(req.body.name)) {
        return res.serverError(403, 'Name must be a string', 'api.shareables.nameMustBeString');
      }

      // empty is allowed so a description can be cleared
      if (req.body.description !== undefined && typeof req.body.description !== 'string') {
        return res.serverError(403, 'Description must be a string', 'api.shareables.descriptionMustBeString');
      }

      let viewRoles = req.body.viewRoles !== undefined ? (ArkimeUtil.isStringArray(req.body.viewRoles) ? req.body.viewRoles : []) : (shareable.viewRoles || []);
      let viewUsers = req.body.viewUsers !== undefined ? (ArkimeUtil.isStringArray(req.body.viewUsers) ? req.body.viewUsers : []) : (shareable.viewUsers || []);
      let editRoles = req.body.editRoles !== undefined ? (ArkimeUtil.isStringArray(req.body.editRoles) ? req.body.editRoles : []) : (shareable.editRoles || []);
      let editUsers = req.body.editUsers !== undefined ? (ArkimeUtil.isStringArray(req.body.editUsers) ? req.body.editUsers : []) : (shareable.editUsers || []);

      if (viewUsers.length > 0) {
        const validatedViewUsers = await User.validateUserIds(viewUsers);
        viewUsers = validatedViewUsers.validUsers;
      }

      if (editUsers.length > 0) {
        const validatedEditUsers = await User.validateUserIds(editUsers);
        editUsers = validatedEditUsers.validUsers;
      }

      if (req.body.data !== undefined && (typeof req.body.data !== 'object' || Array.isArray(req.body.data) || req.body.data === null)) {
        return res.serverError(403, 'Data must be an object');
      }

      // only what the request actually sends is validated -- revalidating the
      // stored data would make a shareable whose shape predates these rules
      // impossible to rename or reshare
      if (req.body.data !== undefined) {
        const dataError = ShareableAPIs.validateData(shareable.type, req.body.data);
        if (dataError) {
          return res.serverError(403, dataError.text, dataError.i18n);
        }
      }

      const data = req.body.data !== undefined ? req.body.data : (shareable.data || {});

      const doc = {
        name: req.body.name !== undefined ? req.body.name : shareable.name,
        type: shareable.type,
        description: req.body.description !== undefined ? req.body.description : shareable.description,
        creator: shareable.creator,
        created: shareable.created,
        updated: new Date(),
        viewRoles,
        viewUsers,
        editRoles,
        editUsers,
        data
      };

      await Db.setShareable(req.params.id, doc);
      const updated = await Db.getShareable(req.params.id);

      updated.id = req.params.id;
      updated.canEdit = await ShareableAPIs.canEdit(user, updated);
      updated.canDelete = ShareableAPIs.canDelete(user, updated);

      return res.json({
        success: true,
        shareable: updated,
        text: 'Updated shareable item!',
        i18n: 'api.shareables.updated'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable/%s (updateShareable)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error updating shareable item', 'api.shareables.errorUpdating');
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
      const shareable = await Db.getShareable(req.params.id);
      const user = req.settingUser;

      if (!ShareableAPIs.canDelete(user, shareable)) {
        return res.serverError(403, 'Not permitted to delete this shareable item', 'api.shareables.notPermittedToDelete');
      }

      await Db.deleteShareable(req.params.id);
      res.json({
        success: true,
        text: 'Deleted shareable item successfully',
        i18n: 'api.shareables.deletedSuccessfully'
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareable/%s (deleteShareable)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting shareable item', 'api.shareables.errorDeleting');
    }
  }

  /**
   * GET - /api/shareables
   *
   * Lists shareable items. Returns items user has permission to view/edit.
   * @name /shareables
   * @param {string} type - Only list this type, omit to list every type
   * @param {string} searchTerm - Substring match against name, description, type and creator
   * @param {string} sort=name - Field to sort by: name, type, description, creator, created or updated
   * @param {string} desc=false - "true" to sort descending
   * @returns {array} data - Array of shareable items with { id, type, name, description,
   *   data, creator, shared, viewUsers, viewRoles, editUsers, editRoles, canEdit, canDelete }
   * @returns {number} recordsTotal - Total records matching query
   * @returns {number} recordsFiltered - Total records after filtering
   */
  static async apiListShareables (req, res) {
    try {
      const user = req.settingUser;
      if (!user) {
        return res.send({ data: [], recordsTotal: 0, recordsFiltered: 0 });
      }

      // type is optional, without it every type the user can see is listed
      if (req.query.type !== undefined && !ArkimeUtil.isString(req.query.type)) {
        return res.serverError(403, 'Invalid type parameter', 'api.shareables.invalidTypeParam');
      }

      if (req.query.searchTerm !== undefined && typeof req.query.searchTerm !== 'string') {
        return res.serverError(403, 'Invalid searchTerm parameter', 'api.shareables.invalidSearchTerm');
      }

      const userRoles = [...await user.getRoles()];

      const allowedShareablesSortFields = { name: 1, type: 1, description: 1, creator: 1, created: 1, updated: 1 };
      const params = {
        user: user.userId,
        roles: userRoles,
        type: req.query.type,
        viewOnly: req.query.viewOnly !== 'false',
        sortField: allowedShareablesSortFields[req.query.sort] ? req.query.sort : 'name',
        sortOrder: req.query.desc === 'true' ? 'desc' : 'asc',
        searchTerm: req.query.searchTerm,
        from: req.query.start || 0,
        size: req.query.length || 50
      };

      const { data: items, total: recordsFiltered } = await Db.searchShareables(params);
      // recordsTotal ignores the search so the UI can show "x of y"
      const recordsTotal = await Db.numberOfShareables({ ...params, searchTerm: undefined });

      const results = items.map(async (item) => {
        const shareable = item.source;
        return {
          id: item.id,
          type: shareable.type,
          name: shareable.name,
          description: shareable.description,
          data: shareable.data,
          creator: shareable.creator,
          shared: shareable.creator !== user.userId,
          viewUsers: shareable.viewUsers ?? [],
          viewRoles: shareable.viewRoles ?? [],
          editUsers: shareable.editUsers ?? [],
          editRoles: shareable.editRoles ?? [],
          canEdit: await ShareableAPIs.canEdit(user, shareable),
          canDelete: ShareableAPIs.canDelete(user, shareable)
        };
      });

      const resolvedResults = await Promise.all(results);

      return res.send({
        data: resolvedResults,
        recordsTotal,
        recordsFiltered
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shareables (listShareables)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error listing shareable items', 'api.shareables.listFailed');
    }
  }
}

module.exports = ShareableAPIs;
