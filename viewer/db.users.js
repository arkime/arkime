/******************************************************************************/
/* db.users.js -- views, shareables and shortcuts in the users db
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

// Names match the layouts each backend used before collections
const COLLECTIONS = {
  views: {},
  shareables: {},
  shortcuts: { version: true, es: { index: 'lookups' }, sqlite: { legacyMeta: { version: 'shortcutsVersion' } } }
};

// The api layers allow the same, anything else sorts by name
const SORT_FIELDS = {
  views: ['name', 'expression', 'created'],
  shareables: ['name', 'type', 'description', 'creator', 'created', 'updated'],
  shortcuts: ['name', 'description', 'created']
};

class DbUsersImpl {
  #views;
  #shareables;
  #shortcuts;

  constructor (conn) {
    const options = (collName) => {
      const opts = COLLECTIONS[collName];
      // es index names are relative to the users prefix
      return opts.es ? { ...opts, es: { index: `${conn.prefix ?? ''}${opts.es.index}` } } : opts;
    };
    this.#views = conn.collection('views', options('views'));
    this.#shareables = conn.collection('shareables', options('shareables'));
    this.#shortcuts = conn.collection('shortcuts', options('shortcuts'));
  }

  async init () {
    await Promise.all([this.#views.init(), this.#shareables.init(), this.#shortcuts.init()]);
  }

  // --------------------------------------------------------------------------
  // VIEWS
  // --------------------------------------------------------------------------
  async searchViews (params) {
    return this.#search(this.#views, 'views', viewsWhere(params), params);
  }

  async numberOfViews (params) {
    return this.#views.count(viewsWhere(params));
  }

  async getView (id) {
    return this.#getOrThrow(this.#views, id, 'View');
  }

  // idOrName comes from the request, an object would be read as where operators.
  // An id, then the user's own view by that name, then the one view shared with them by that name.
  // Anyone can share a view with anyone, so a name more than one shared view has is refused.
  async getViewByIdOrName (idOrName, user, roles) {
    if (typeof idOrName !== 'string') { return null; }
    const size = 100;
    const { total, data } = await this.#views.find({
      where: {
        $and: [
          { $or: [{ id: idOrName }, { name: idOrName }] },
          { $or: accessClauses({ roles, users: user, user }) }
        ]
      },
      size
    });

    // Past one page the id or own view may not be on it, and the shared name is ambiguous anyway
    if (total > size) {
      const { data: exact } = await this.#views.find({
        where: { $or: [{ $and: [{ id: idOrName }, { $or: accessClauses({ roles, users: user, user }) }] }, { $and: [{ name: idOrName }, { user }] }] },
        size
      });
      return (exact.find(h => h.id === idOrName) ?? exact[0])?.doc ?? null;
    }

    const named = data.filter(h => h.doc.name === idOrName);
    const hit = data.find(h => h.id === idOrName) ??
      named.find(h => h.doc.user === user) ??
      (named.length === 1 ? named[0] : undefined);
    return hit?.doc ?? null;
  }

  async createView (doc) {
    return this.#views.create(doc);
  }

  async deleteView (id) {
    await this.#views.delete(id);
  }

  async deleteAllViews () {
    await this.#views.deleteAll();
  }

  async setView (id, doc) {
    await this.#views.put(id, doc);
  }

  // --------------------------------------------------------------------------
  // SHAREABLES
  // --------------------------------------------------------------------------
  async searchShareables (params) {
    return this.#search(this.#shareables, 'shareables', shareablesWhere(params), params);
  }

  async numberOfShareables (params) {
    return this.#shareables.count(shareablesWhere(params));
  }

  async getShareable (id) {
    return this.#getOrThrow(this.#shareables, id, 'Shareable');
  }

  async createShareable (doc) {
    return this.#shareables.create(doc);
  }

  async deleteShareable (id) {
    await this.#shareables.delete(id);
  }

  async deleteAllShareables () {
    await this.#shareables.deleteAll();
  }

  async getShareableVersioned (id) {
    const hit = await this.#shareables.getVersioned(id);
    if (!hit) { throw new Error('Shareable not found'); }
    return { shareable: hit.doc, version: hit.version };
  }

  async setShareable (id, doc, version) {
    if (version === undefined) {
      await this.#shareables.put(id, doc);
    } else {
      await this.#shareables.putIfVersion(id, doc, version);
    }
  }

  // --------------------------------------------------------------------------
  // SHORTCUTS
  // --------------------------------------------------------------------------
  async searchShortcuts (params) {
    return this.#search(this.#shortcuts, 'shortcuts', shortcutsWhere(params), params);
  }

  async numberOfShortcuts (params) {
    return this.#shortcuts.count(shortcutsWhere(params));
  }

  async getShortcut (id) {
    return this.#getOrThrow(this.#shortcuts, id, 'Shortcut');
  }

  async createShortcut (doc) {
    return this.#shortcuts.create(doc);
  }

  async deleteShortcut (id) {
    if (!await this.#shortcuts.delete(id)) { throw new Error('Shortcut not found'); }
  }

  async setShortcut (id, doc) {
    await this.#shortcuts.put(id, doc);
  }

  async deleteAllShortcuts () {
    await this.#shortcuts.deleteAll();
  }

  // Returns all shortcuts with id, source, and version for sync to local ES
  async getAllShortcuts () {
    const { data } = await this.#shortcuts.find({ version: true });
    return data.map(h => ({ id: h.id, source: h.doc, version: h.version }));
  }

  async getShortcutsVersion () {
    return (await this.#shortcuts.getMeta('version')) || 0;
  }

  async setShortcutsVersion () {
    const version = await this.getShortcutsVersion();
    await this.#shortcuts.setMeta({ version: version + 1, initSync: true });
  }

  // --------------------------------------------------------------------------
  // Flush/Refresh, index is the ES index name without the prefix
  // --------------------------------------------------------------------------
  #byIndex (index) {
    return { views: this.#views, shareables: this.#shareables, lookups: this.#shortcuts }[index];
  }

  async flush (index) {
    await this.#byIndex(index)?.flush?.();
  }

  async refresh (index) {
    await this.#byIndex(index)?.refresh();
  }

  // --------------------------------------------------------------------------
  async #search (coll, collName, where, params) {
    const sortField = SORT_FIELDS[collName].includes(params.sortField) ? params.sortField : 'name';
    const { total, data } = await coll.find({
      where,
      sort: { [sortField]: params.sortOrder === 'desc' ? 'desc' : 'asc' },
      from: params.from || 0,
      size: Math.min(Math.max(+params.size || 50, 0), 10000)
    });
    return { total, data: data.map(h => ({ id: h.id, source: h.doc })) };
  }

  async #getOrThrow (coll, id, what) {
    const doc = await coll.get(id);
    if (!doc) { throw new Error(`${what} not found`); }
    return doc;
  }
}

// $or clauses for the fields that grant access, skipping missing values
function accessClauses (fields) {
  const or = [];
  for (const [field, value] of Object.entries(fields)) {
    if (value === undefined || value === null) { continue; }
    or.push(Array.isArray(value) ? { [field]: { $in: value } } : { [field]: value });
  }
  return or;
}

function viewsWhere (params) {
  const and = [];
  if (!params.all) {
    and.push({ $or: accessClauses({ roles: params.roles ?? [], editRoles: params.roles ?? [], users: params.user, user: params.user }) });
  }
  if (params.searchTerm) {
    and.push({ name: { $icontains: params.searchTerm } });
  }
  return { $and: and };
}

function shareablesWhere (params) {
  const and = [];
  // no type means list every type the user can see
  if (params.type) { and.push({ type: params.type }); }

  const access = params.viewOnly
    ? { creator: params.user, viewUsers: params.user, viewRoles: params.roles }
    : { creator: params.user, editUsers: params.user, editRoles: params.roles, viewUsers: params.user, viewRoles: params.roles };
  and.push({ $or: accessClauses(access) });

  if (params.searchTerm) {
    and.push({ $or: ['name', 'description', 'type', 'creator'].map(f => ({ [f]: { $icontains: params.searchTerm } })) });
  }
  return { $and: and };
}

function shortcutsWhere (params) {
  const and = [];
  if (!params.all) {
    and.push({ $or: accessClauses({ roles: params.roles ?? [], editRoles: params.roles ?? [], users: params.user, userId: params.user }) });
  }
  if (params.searchTerm) {
    and.push({ name: { $icontains: params.searchTerm } });
  }
  if (params.fieldType) {
    and.push({ [params.fieldType]: { $exists: true } });
  }
  // Name collision check: find shortcuts with this name but NOT this id
  if (params.nameCheck) {
    and.push({ name: params.nameCheck });
    if (params.excludeId) { and.push({ id: { $ne: params.excludeId } }); }
  }
  return { $and: and };
}

module.exports = DbUsersImpl;
