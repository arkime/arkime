'use strict';

const dns = require('dns');
const fs = require('fs');

module.exports = (Config, Db, internals) => {
  const module = {};

  // field apis ---------------------------------------------------------------
  /**
   * GET - /api/fields
   *
   * Gets available database field objects pertaining to sessions.
   * @name /fields
   * @param {boolean} array=false Whether to return an array of fields, otherwise returns a map
   * @returns {array/map} The map or list of database fields
   */
  module.getFields = (req, res) => {
    if (!internals.fieldsMap) {
      res.status(404);
      res.send('Cannot locate fields');
    }

    if (req.query && req.query.array) {
      res.send(internals.fieldsArr);
    } else {
      res.send(internals.fieldsMap);
    }
  };

  // file apis ----------------------------------------------------------------
  /**
   * GET - /api/files
   *
   * Gets a list of PCAP files that Arkime knows about.
   * @name /files
   * @param {number} length=100 - The number of items to return. Defaults to 500, Max is 10,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @returns {Array} data - The list of files
   * @returns {number} recordsTotal - The total number of files Arkime knows about
   * @returns {number} recordsFiltered - The number of files returned in this result
   */
  module.getFiles = (req, res) => {
    const columns = ['num', 'node', 'name', 'locked', 'first', 'filesize', 'encoding', 'packetPosEncoding'];

    const query = {
      _source: columns,
      from: +req.query.start || 0,
      size: +req.query.length || 10,
      sort: {}
    };

    query.sort[req.query.sortField || 'num'] = {
      order: req.query.desc === 'true' ? 'desc' : 'asc'
    };

    if (req.query.filter) {
      query.query = { wildcard: { name: `*${req.query.filter}*` } };
    }

    Promise.all([
      Db.search('files', 'file', query),
      Db.numberOfDocuments('files')
    ]).then(([files, total]) => {
      if (files.error) { throw files.error; }

      const results = { total: files.hits.total, results: [] };
      for (const file of files.hits.hits) {
        const fields = file._source || files.fields;
        if (fields.locked === undefined) {
          fields.locked = 0;
        }
        fields.id = fields._id;
        results.results.push(fields);
      }

      const r = {
        recordsTotal: total.count,
        recordsFiltered: results.total,
        data: results.results
      };

      res.logCounts(r.data.length, r.recordsFiltered, r.total);
      res.send(r);
    }).catch((err) => {
      console.log('ERROR - /file/list', err);
      return res.send({ recordsTotal: 0, recordsFiltered: 0, data: [] });
    });
  };

  /**
   * GET - /api/:nodeName/:fileNum/filesize
   *
   * Retrieves the filesize of a PCAP file.
   * @name /:nodeName/:fileNum/filesize
   * @returns {number} filesize - The size of the file (-1 if the file cannot be found).
   */
  module.getFileSize = (req, res) => {
    Db.fileIdToFile(req.params.nodeName, req.params.fileNum, (file) => {
      if (!file) {
        return res.send({ filesize: -1 });
      }

      fs.stat(file.name, (err, stats) => {
        if (err || !stats) {
          return res.send({ filesize: -1 });
        } else {
          return res.send({ filesize: stats.size });
        }
      });
    });
  };

  /**
   * GET - /api/title
   *
   * Retrieves the browser page title for the Arkime app.
   * Configure it using <a href="https://arkime.com/settings#titletemplate">the titleTemplate setting</a>
   * @name /title
   * @returns {string} title - The title of the app based on the configured setting.
   */
  module.getPageTitle = (req, res) => {
    let titleConfig = Config.get('titleTemplate', '_cluster_ - _page_ _-view_ _-expression_');

    titleConfig = titleConfig.replace(/_cluster_/g, internals.clusterName)
      .replace(/_userId_/g, req.user ? req.user.userId : '-')
      .replace(/_userName_/g, req.user ? req.user.userName : '-');

    res.send(titleConfig);
  };

  /**
   * GET - /api/valueactions
   *
   * Retrives the actions that can be preformed on meta data values.
   * @name /valueactions
   * @returns {object} - The list of actions that can be preformed on data values.
   */
  module.getValueActions = (req, res) => {
    if (!req.user || !req.user.userId) {
      return res.send({});
    }

    const actions = {};

    actions.httpAuthorizationDecode = { fields: 'http.authorization', func: `{
      if (value.substring(0,5) === "Basic")
        return {name: "Decoded:", value: atob(value.substring(6))};
      return undefined;
    }` };
    actions.reverseDNS = { category: 'ip', name: 'Get Reverse DNS', url: 'reverseDNS.txt?ip=%TEXT%', actionType: 'fetch' };
    actions.bodyHashMd5 = { category: 'md5', url: '%NODE%/%ID%/bodyHash/%TEXT%', name: 'Download File' };
    actions.bodyHashSha256 = { category: 'sha256', url: '%NODE%/%ID%/bodyHash/%TEXT%', name: 'Download File' };

    for (const key in internals.rightClicks) {
      const rc = internals.rightClicks[key];
      if (!rc.users || rc.users[req.user.userId]) {
        actions[key] = rc;
      }
    }

    return res.send(actions);
  };

  /**
   * GET - /api/reversedns
   *
   * Retrives the domain names associated with an IP address.
   * @name /reversedns
   * @param {string} ip - The IP to search domain names for.
   * @returns {string} domains - A comma separated string list of all the matching domain names.
   */
  module.getReverseDNS = (req, res) => {
    dns.reverse(req.query.ip, (err, data) => {
      if (err) {
        return res.send('reverse error');
      }
      return res.send(data.join(', '));
    });
  };

  return module;
};
