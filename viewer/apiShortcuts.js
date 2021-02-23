'use strict';

module.exports = (Db, internals, ViewerUtils) => {
  const module = {};

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
  module.getShortcuts = (req, res) => {
    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const map = req.query.map && req.query.map === 'true';

    // only get shortcuts for setting user or shared
    const query = {
      query: {
        bool: {
          must: [
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
      query.query.bool.must.push({
        wildcard: { name: '*' + req.query.searchTerm + '*' }
      });
    }

    // if fieldType exists, filter it
    if (req.query.fieldType) {
      const fieldType = internals.shortcutTypeMap[req.query.fieldType];

      if (fieldType) {
        query.query.bool.must.push({
          exists: { field: fieldType }
        });
      }
    }

    Promise.all([
      Db.searchShortcuts(query),
      Db.numberOfDocuments('lookups')
    ]).then(([shortcuts, total]) => {
      if (shortcuts.error) { throw shortcuts.error; }

      const results = { list: [], map: {} };
      for (const hit of shortcuts.hits.hits) {
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
          const name = `$${shortcut.name}`;
          shortcut.exp = name;
          shortcut.dbField = name;
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
          recordsTotal: total.count,
          recordsFiltered: shortcuts.hits.total
        };

      res.send(sendResults);
    }).catch((err) => {
      console.log('ERROR - /api/shortcuts', err);
      return res.molochError(500, 'Error retrieving shortcuts - ' + err);
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
  module.createShortcut = (req, res) => {
    // make sure all the necessary data is included in the post body
    if (!req.body.name) {
      return res.molochError(403, 'Missing shortcut name');
    }
    if (!req.body.type) {
      return res.molochError(403, 'Missing shortcut type');
    }
    if (!req.body.value) {
      return res.molochError(403, 'Missing shortcut value');
    }

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_]/g, '');

    // return nothing if we can't find the user
    const user = req.settingUser;
    if (!user) { return res.send({}); }

    const query = {
      query: {
        bool: {
          must: [
            { term: { name: req.body.name } }
          ]
        }
      }
    };

    shortcutMutex.lock().then(() => {
      Db.searchShortcuts(query).then((shortcuts) => {
        // search for shortcut name collision
        for (const hit of shortcuts.hits.hits) {
          const shortcut = hit._source;
          if (shortcut.name === req.body.name) {
            shortcutMutex.unlock();
            return res.molochError(403, `A shortcut with the name, ${req.body.name}, already exists`);
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

        Db.createShortcut(newShortcut, user.userId, (err, result) => {
          if (err) {
            console.log('shortcut create failed', err, result);
            shortcutMutex.unlock();
            return res.molochError(500, 'Creating shortcut failed');
          }
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
        });
      }).catch((err) => {
        console.log('ERROR - /api/shortcut', err);
        shortcutMutex.unlock();
        return res.molochError(500, 'Error creating shortcut - ' + err);
      });
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
  module.updateShortcut = (req, res) => {
    // make sure all the necessary data is included in the post body
    if (!req.body.name) {
      return res.molochError(403, 'Missing shortcut name');
    }
    if (!req.body.type) {
      return res.molochError(403, 'Missing shortcut type');
    }
    if (!req.body.value) {
      return res.molochError(403, 'Missing shortcut value');
    }

    const sentShortcut = req.body;

    Db.getShortcut(req.params.id, (err, fetchedShortcut) => { // fetch shortcut
      if (err) {
        console.log('fetching shortcut to update failed', err, fetchedShortcut);
        return res.molochError(500, 'Fetching shortcut to update failed');
      }

      if (fetchedShortcut._source.locked) {
        return res.molochError(403, 'Locked Shortcut. Use db.pl script to update this shortcut.');
      }

      // only allow admins or shortcut creator to update shortcut item
      if (!req.user.createEnabled && req.settingUser.userId !== fetchedShortcut._source.userId) {
        return res.molochError(403, 'Permission denied');
      }

      const query = {
        query: {
          bool: {
            must: [
              { term: { name: req.body.name } } // same name
            ],
            must_not: [
              { ids: { values: [req.params.id] } } // but different ID
            ]
          }
        }
      };

      shortcutMutex.lock().then(() => {
        Db.searchShortcuts(query).then((shortcuts) => {
          // search for shortcut name collision
          for (const hit of shortcuts.hits.hits) {
            const shortcut = hit._source;
            if (shortcut.name === req.body.name) {
              shortcutMutex.unlock();
              return res.molochError(403, `A shortcut with the name, ${req.body.name}, already exists`);
            }
          }

          // comma/newline separated value -> array of values
          const values = ViewerUtils.commaStringToArray(sentShortcut.value);
          sentShortcut[sentShortcut.type] = values;
          sentShortcut.userId = fetchedShortcut._source.userId;

          delete sentShortcut.type;
          delete sentShortcut.value;

          Db.setShortcut(req.params.id, fetchedShortcut.userId, sentShortcut, (err, info) => {
            shortcutMutex.unlock();

            if (err) {
              console.log('shortcut update failed', err, info);
              return res.molochError(500, 'Updating shortcut failed');
            }

            sentShortcut.value = values.join('\n');

            return res.send(JSON.stringify({
              success: true,
              shortcut: sentShortcut,
              text: 'Successfully updated shortcut'
            }));
          });
        }).catch((err) => {
          console.log('ERROR - /api/shortcut', err);
          shortcutMutex.unlock();
          return res.molochError(500, 'Error updating shortcut - ' + err);
        });
      });
    });
  };

  /**
   * DELETE - /api/shortcut/:id
   *
   * Deletes a shortcut.
   * @name /shortcut/:id
   * @returns {boolean} success - Whether the delete shortcut operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  module.deleteShortcut = (req, res) => {
    Db.getShortcut(req.params.id, (err, shortcut) => { // fetch shortcut
      if (err) {
        console.log('fetching shortcut to delete failed', err, shortcut);
        return res.molochError(500, 'Fetching shortcut to delete failed');
      }

      // only allow admins or shortcut creator to delete shortcut item
      if (!req.user.createEnabled && req.settingUser.userId !== shortcut._source.userId) {
        return res.molochError(403, 'Permission denied');
      }

      Db.deleteShortcut(req.params.id, shortcut.userId, (err, result) => {
        if (err || result.error) {
          console.log('ERROR - deleting shortcut', err || result.error);
          return res.molochError(500, 'Error deleting shortcut');
        } else {
          res.send(JSON.stringify({
            success: true,
            text: 'Deleted shortcut successfully'
          }));
        }
      });
    });
  };

  return module;
};
