'use strict';

const util = require('util');

module.exports = (Db, internals, ViewerUtils) => {
  const shortcutAPIs = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  // https://stackoverflow.com/a/48569020
  class Mutex {
    constructor () {
      this.queue = [];
      this.locked = false;
    }

    lock () {
      return new Promise((resolve, reject) => {
        if (this.locked) {
          this.queue.push(resolve);
        } else {
          this.locked = true;
          resolve();
        }
      });
    }

    unlock () {
      if (this.queue.length > 0) {
        const resolve = this.queue.shift();
        resolve();
      } else {
        this.locked = false;
      }
    }
  }

  const shortcutMutex = new Mutex();

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * The shortcut object to store lists of values that can be used in search queries.
   *
   * @typedef Shortcut
   * @type {object}
   * @param {string} userId - The ID of the user that created the shortcut.
   * @param {string} name - The name of the shortcut.
   * @param {boolean} shared=false - Whether the shortcut is shared with the other users in the cluster.
   * @param {string} description - The description of the shortcut to display to users.
   * @param {number[]} number - A list of number values to use as the shortcut value. A shortcut must contain a list of numbers, strings, or ips.
   * @param {string[]} ip - A list of ip values to use as the shortcut value. A shortcut must contain a list of numbers, strings, or ips.
   * @param {string[]} string - A list of string values to use as the shortcut value. A shortcut must contain a list of numbers, strings, or ips.
   * @param {boolean} locked=false - Whether the shortcut is locked and must be updated using the db.pl script (can't be updated in the web application user interface).
   */

  /**
   * GET - /api/shortcuts
   *
   * Retrieves a list of shortcuts.
   * @name /shortcuts
   * @param {string} map=false - Whether to return a list or a map. Default is list.
   * @param {string} sort=name - The field to sort the results by.
   * @param {string} desc=true - Whether to sort the results descending or ascending. Default is descending.
   * @param {string} searchTerm - The search text to filter the shortcut list by.
   * @param {number} length=50 - The number of items to return. Defaults to 50.
   * @param {number} start=0 - The entry to start at. Defaults to 0.
   * @param {string} fieldType - Filter the results by type (number, ip, or string).
   * @param {string} fieldFormat=false - Sends a help field with the shortcut with the description + the values of the shortcut.
   * @returns {Shortcut[]} data - The list of shortcut results.
   * @returns {number} recordsTotal - The total number of shortcut results stored.
   * @returns {number} recordsFiltered - The number of shortcut items returned in this result.
   */
  shortcutAPIs.getShortcuts = (req, res) => {
    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const map = req.query.map && req.query.map === 'true';

    // only get shortcuts for setting user or shared
    const query = {
      query: {
        bool: {
          filter: [
            {
              bool: {
                should: [
                  { term: { shared: true } },
                  { term: { userId: req.settingUser.userId } }
                ]
              }
            }
          ]
        }
      },
      sort: {},
      size: req.query.length || 50,
      from: req.query.start || 0
    };

    query.sort[req.query.sort || 'name'] = {
      order: req.query.desc === 'true' ? 'desc' : 'asc'
    };

    if (req.query.searchTerm) {
      query.query.bool.filter.push({
        wildcard: { name: '*' + req.query.searchTerm + '*' }
      });
    }

    // if fieldType exists, filter it
    if (req.query.fieldType) {
      const fieldType = internals.shortcutTypeMap[req.query.fieldType];

      if (fieldType) {
        query.query.bool.filter.push({
          exists: { field: fieldType }
        });
      }
    }

    Promise.all([
      Db.searchShortcuts(query),
      Db.numberOfShortcuts()
    ]).then(([{ body: { hits: shortcuts } }, { body: { count: total } }]) => {
      const results = { list: [], map: {} };
      for (const hit of shortcuts.hits) {
        const shortcut = hit._source;
        shortcut.id = hit._id;

        if (shortcut.number) {
          shortcut.type = 'number';
        } else if (shortcut.ip) {
          shortcut.type = 'ip';
        } else {
          shortcut.type = 'string';
        }

        const values = shortcut[shortcut.type];

        if (req.query.fieldFormat && req.query.fieldFormat === 'true') {
          const shortcutName = `$${shortcut.name}`;
          shortcut.exp = shortcutName;
          shortcut.dbField = shortcutName;
          shortcut.help = shortcut.description
            ? `${shortcut.description}: ${values.join(', ')}`
            : `${values.join(',')}`;
        }

        shortcut.value = values.join('\n');
        delete shortcut[shortcut.type];

        if (map) {
          results.map[shortcut.id] = shortcut;
        } else {
          results.list.push(shortcut);
        }
      }

      const sendResults = map
        ? results.map
        : {
          data: results.list,
          recordsTotal: total,
          recordsFiltered: shortcuts.total
        };

      res.send(sendResults);
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/shortcuts`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error retrieving shortcuts - ' + err);
    });
  };

  /**
   * POST - /api/shortcut
   *
   * Creates a new shortcut.
   * @name /shortcut
   * @param {string} name - The name of the new shortcut.
   * @param {string} type - The type of the shortcut (number, ip, or string).
   * @param {string} value - The shortcut value.
   * @returns {Shortcut} shortcut - The new shortcut object.
   * @returns {boolean} success - Whether the create shortcut operation was successful.
   */
  shortcutAPIs.createShortcut = (req, res) => {
    // make sure all the necessary data is included in the post body
    if (!req.body.name) {
      return res.serverError(403, 'Missing shortcut name');
    }
    if (!req.body.type) {
      return res.serverError(403, 'Missing shortcut type');
    }
    if (!req.body.value) {
      return res.serverError(403, 'Missing shortcut value');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_]/g, '');

    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const query = {
      query: {
        bool: {
          filter: [
            { term: { name: req.body.name } }
          ]
        }
      }
    };

    shortcutMutex.lock().then(async () => {
      try {
        const { body: { hits: shortcuts } } = await Db.searchShortcuts(query);

        // search for shortcut name collision
        for (const hit of shortcuts.hits) {
          const shortcut = hit._source;
          if (shortcut.name === req.body.name) {
            shortcutMutex.unlock();
            return res.serverError(403, `A shortcut with the name, ${req.body.name}, already exists`);
          }
        }

        const newShortcut = req.body;
        newShortcut.userId = user.userId;

        // comma/newline separated value -> array of values
        const values = ViewerUtils.commaStringToArray(newShortcut.value);
        newShortcut[newShortcut.type] = values;

        const type = newShortcut.type;
        delete newShortcut.type;
        delete newShortcut.value;

        try {
          const { body: result } = await Db.createShortcut(newShortcut);
          newShortcut.id = result._id;
          newShortcut.type = type;
          newShortcut.value = values.join('\n');
          delete newShortcut.ip;
          delete newShortcut.string;
          delete newShortcut.number;
          shortcutMutex.unlock();

          return res.send(JSON.stringify({
            success: true,
            shortcut: newShortcut
          }));
        } catch (err) {
          shortcutMutex.unlock();
          console.log(`ERROR - ${req.method} /api/shortcut (createShortcut)`, util.inspect(err, false, 50));
          return res.serverError(500, 'Error creating shortcut');
        }
      } catch (err) {
        shortcutMutex.unlock();
        console.log(`ERROR - ${req.method} /api/shortcut (searchShortcuts)`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error creating shortcut');
      }
    });
  };

  /**
   * PUT - /api/shortcut/:id
   *
   * Updates a shortcut.
   * @name /shortcut/:id
   * @param {string} name - The name of the shortcut.
   * @param {string} type - The type of the shortcut (number, ip, or string).
   * @param {string} value - The shortcut value.
   * @returns {Shortcut} shortcut - The updated shortcut object.
   * @returns {boolean} success - Whether the upate shortcut operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  shortcutAPIs.updateShortcut = async (req, res) => {
    // make sure all the necessary data is included in the post body
    if (!req.body.name) {
      return res.serverError(403, 'Missing shortcut name');
    }
    if (!req.body.type) {
      return res.serverError(403, 'Missing shortcut type');
    }
    if (!req.body.value) {
      return res.serverError(403, 'Missing shortcut value');
    }

    const sentShortcut = req.body;

    try {
      const { body: fetchedShortcut } = await Db.getShortcut(req.params.id);

      if (fetchedShortcut._source.locked) {
        return res.serverError(403, 'Locked Shortcut. Use db.pl script to update this shortcut.');
      }

      // only allow admins or shortcut creator to update shortcut item
      if (!req.user.createEnabled && req.settingUser.userId !== fetchedShortcut._source.userId) {
        return res.serverError(403, 'Permission denied');
      }

      const query = {
        query: {
          bool: {
            filter: [
              { term: { name: req.body.name } } // same name
            ],
            must_not: [
              { ids: { values: [req.params.id] } } // but different ID
            ]
          }
        }
      };

      shortcutMutex.lock().then(async () => {
        try {
          const { body: { hits: shortcuts } } = await Db.searchShortcuts(query);

          // search for shortcut name collision
          for (const hit of shortcuts.hits) {
            const shortcut = hit._source;
            if (shortcut.name === req.body.name) {
              shortcutMutex.unlock();
              return res.serverError(403, `A shortcut with the name, ${req.body.name}, already exists`);
            }
          }

          // comma/newline separated value -> array of values
          const values = ViewerUtils.commaStringToArray(sentShortcut.value);
          sentShortcut[sentShortcut.type] = values;
          sentShortcut.userId = fetchedShortcut._source.userId;

          delete sentShortcut.type;
          delete sentShortcut.value;

          try {
            await Db.setShortcut(req.params.id, sentShortcut);
            shortcutMutex.unlock();
            sentShortcut.value = values.join('\n');

            return res.send(JSON.stringify({
              success: true,
              shortcut: sentShortcut,
              text: 'Successfully updated shortcut'
            }));
          } catch (err) {
            shortcutMutex.unlock();
            console.log(`ERROR - ${req.method} /api/shortcut/${req.params.id} (setShortcut)`, util.inspect(err, false, 50));
            return res.serverError(500, 'Error updating shortcut');
          }
        } catch (err) {
          shortcutMutex.unlock();
          console.log(`ERROR - ${req.method} /api/shortcut/${req.params.id} (searchShortcuts)`, util.inspect(err, false, 50));
          return res.serverError(500, 'Error updating shortcut');
        }
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcut/${req.params.id} (getShortcut)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching shortcut to update failed');
    }
  };

  /**
   * DELETE - /api/shortcut/:id
   *
   * Deletes a shortcut.
   * @name /shortcut/:id
   * @returns {boolean} success - Whether the delete shortcut operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  shortcutAPIs.deleteShortcut = async (req, res) => {
    try {
      const { data: shortcut } = await Db.getShortcut(req.params.id);

      // only allow admins or shortcut creator to delete shortcut item

      if (!req.user.createEnabled && req.settingUser.userId !== shortcut?._source.userId) {
        return res.serverError(403, 'Permission denied');
      }

      try {
        await Db.deleteShortcut(req.params.id);
        res.send(JSON.stringify({
          success: true,
          text: 'Deleted shortcut successfully'
        }));
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/shortcut/${req.params.id} (deleteShortcut)`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error deleting shortcut');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/shortcut/${req.params.id} (getShortcut)`, util.inspect(err, false, 50));
      return res.serverError(500, 'Fetching shortcut to delete failed');
    }
  };

  /**
   * GET - /api/syncshortcuts
   *
   * @name /syncshortcuts
   * Updates the shortcuts in the local db if they are out of sync with the
   * remote db (remote db = user's es)
   * This happens periodically (every minute) but can be triggered with this endpoint
   * @ignore
   * @returns {boolean} success - Always true.
   */
  shortcutAPIs.syncShortcuts = (req, res) => {
    Db.updateLocalShortcuts();
    return res.send(JSON.stringify({ success: true }));
  };

  return shortcutAPIs;
};
