'use strict';

const dns = require('dns');
const fs = require('fs');
const unzipper = require('unzipper');

module.exports = (Config, Db, internals, sessionAPIs, ViewerUtils) => {
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

  // title apis ---------------------------------------------------------------
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

  // value actions apis -------------------------------------------------------
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

    actions.httpAuthorizationDecode = {
      fields: 'http.authorization', func: `{
      if (value.substring(0,5) === "Basic")
        return {name: "Decoded:", value: atob(value.substring(6))};
      return undefined;
    }`
    };
    actions.reverseDNS = { category: 'ip', name: 'Get Reverse DNS', url: 'api/reversedns?ip=%TEXT%', actionType: 'fetch' };
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

  // reverse dns apis ---------------------------------------------------------
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

  // upload apis --------------------------------------------------------------
  /**
   * POST - /api/upload
   *
   * Uploads PCAP files.
   * @name /upload
   * @param {string} tags - A comma separated list of tags to add to each session created.
   */
  module.upload = (req, res) => {
    const exec = require('child_process').exec;

    let tags = '';
    if (req.body.tags) {
      const t = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, '').split(',');
      t.forEach((tag) => {
        if (tag.length > 0) {
          tags += ' --tag ' + tag;
        }
      });
    }

    const cmd = Config.get('uploadCommand')
      .replace('{TAGS}', tags)
      .replace('{NODE}', Config.nodeName())
      .replace('{TMPFILE}', req.file.path)
      .replace('{CONFIG}', Config.getConfigFile());

    console.log('upload command: ', cmd);
    exec(cmd, (error, stdout, stderr) => {
      if (error !== null) {
        console.log('<b>exec error: ' + error);
        res.status(500);
        res.write('<b>Upload command failed:</b><br>');
      }
      res.write(ViewerUtils.safeStr(cmd));
      res.write('<br>');
      res.write('<pre>');
      res.write(stdout);
      res.end('</pre>');
      fs.unlinkSync(req.file.path);
    });
  };

  // cluster apis -------------------------------------------------------------
  /**
   * GET - /api/clusters
   *
   * Retrieves a list of known configured Arkime clusters (if in
   * <a href="https://arkime.com/settings#multi-viewer-settings">Mulit Viewer mode</a>).
   * @name /clusters
   * @returns {Array} active - The active Arkime clusters.
   * @returns {Array} inactive - The inactive Arkime clusters.
   */
  module.getClusters = (req, res) => {
    const clusters = { active: [], inactive: [] };
    if (Config.get('multiES', false)) {
      Db.getClusterDetails((err, results) => {
        if (err) {
          console.log('Error: ' + err);
        } else if (results) {
          clusters.active = results.active;
          clusters.inactive = results.inactive;
        }
        res.send(clusters);
      });
    } else {
      res.send(clusters);
    }
  };

  // cyberchef apis -----------------------------------------------------------
  /**
   * @ignore
   *
   * Retrieves the source or destination packets for a session for CyberChef.
   * @name /cyberchef/:nodeName/session/:id
   * @param {string} type=src - Whether to send the source (src) or destination (dst) packets.
   */
  module.cyberChef = (req, res) => {
    sessionAPIs.processSessionIdAndDecode(req.params.id, 10000, (err, session, results) => {
      if (err) {
        console.log(`ERROR - /${req.params.nodeName}/session/${req.params.id}/cyberchef`, err);
        return res.end('Error - ' + err);
      }

      let data = '';
      for (let i = (req.query.type !== 'dst' ? 0 : 1), ilen = results.length; i < ilen; i += 2) {
        data += results[i].data.toString('hex');
      }

      res.send({ data: data });
    });
  };

  /**
   * @ignore
   *
   * Loads the CyberChef UI.
   * @name /cyberchef
   */
  module.getCyberChefUI = (req, res) => {
    let found = false;
    let path = req.path.substring(1);

    if (req.baseUrl === '/modules') {
      res.setHeader('Content-Type', 'application/javascript; charset=UTF-8');
      path = 'modules/' + path;
    }

    if (path === '') {
      path = `CyberChef_v${internals.CYBERCHEFVERSION}.html`;
    }

    if (path === 'assets/main.js') {
      res.setHeader('Content-Type', 'application/javascript; charset=UTF-8');
    } else if (path === 'assets/main.css') {
      res.setHeader('Content-Type', 'text/css');
    } else if (path.endsWith('.png')) {
      res.setHeader('Content-Type', 'image/png');
    }

    fs.createReadStream(
      `public/CyberChef_v${internals.CYBERCHEFVERSION}.zip`
    ).pipe(unzipper.Parse()).on('entry', (entry) => {
      if (entry.path === path) {
        entry.pipe(res);
        found = true;
      } else {
        entry.autodrain();
      }
    }).on('finish', () => {
      if (!found) {
        res.status(404).end('Page not found');
      }
    });
  };

  return module;
};
