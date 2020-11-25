'use strict';

const async = require('async');
const fs = require('fs');
const http = require('http');
const https = require('https');
const path = require('path');
const pug = require('pug');
const url = require('url');
const util = require('util');

module.exports = (Config, Db, decode, internals, molochparser, Pcap, ViewerUtils) => {
  let module = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  /**
   * Adds the sort options to the elasticsearch query
   * @ignore
   * @name addSortToQuery
   * @param {object} query - the elasticsearch query that has been partially built already by buildSessionQuery
   * @param {object} info - the query params from the client
   * @param {string} defaultSort - the default sort
   */
  function addSortToQuery (query, info, defaultSort) {
    function addSortDefault () {
      if (defaultSort) {
        if (!query.sort) {
          query.sort = [];
        }
        let obj = {};
        obj[defaultSort] = { order: 'asc' };
        obj[defaultSort].missing = '_last';
        query.sort.push(obj);
      }
    }

    if (!info) {
      addSortDefault();
      return;
    }

    // New Method
    if (info.order) {
      if (info.order.length === 0) {
        addSortDefault();
        return;
      }

      if (!query.sort) {
        query.sort = [];
      }

      info.order.split(',').forEach(function (item) {
        const parts = item.split(':');
        const field = parts[0];

        let obj = {};
        if (field === 'firstPacket') {
          obj.firstPacket = { order: parts[1] };
        } else if (field === 'lastPacket') {
          obj.lastPacket = { order: parts[1] };
        } else {
          obj[field] = { order: parts[1] };
        }

        obj[field].unmapped_type = 'string';
        const fieldInfo = Config.getDBFieldsMap()[field];
        if (fieldInfo) {
          if (fieldInfo.type === 'ip') {
            obj[field].unmapped_type = 'ip';
          } else if (fieldInfo.type === 'integer') {
            obj[field].unmapped_type = 'long';
          }
        }
        obj[field].missing = (parts[1] === 'asc' ? '_last' : '_first');
        query.sort.push(obj);
      });

      return;
    }

    // Old Method
    if (!info.iSortingCols || parseInt(info.iSortingCols, 10) === 0) {
      addSortDefault();
      return;
    }

    if (!query.sort) {
      query.sort = [];
    }

    for (let i = 0, ilen = parseInt(info.iSortingCols, 10); i < ilen; i++) {
      if (!info['iSortCol_' + i] || !info['sSortDir_' + i] || !info['mDataProp_' + info['iSortCol_' + i]]) {
        continue;
      }

      let obj = {};
      const field = info['mDataProp_' + info['iSortCol_' + i]];
      obj[field] = { order: info['sSortDir_' + i] };
      query.sort.push(obj);

      if (field === 'firstPacket') {
        query.sort.push({ firstPacket: { order: info['sSortDir_' + i] } });
      } else if (field === 'lastPacket') {
        query.sort.push({ lastPacket: { order: info['sSortDir_' + i] } });
      }
    }
  }

  /**
   * Adds the view search expression to the elasticsearch query
   * @ignore
   * @name addViewToQuery
   * @param {object} req - the client request
   * @param {object} query - the elasticsearch query that has been partially built already by buildSessionQuery
   * @param {function} continueBuildQueryCb - the callback to call when adding the view is complete
   * @param {function} finalCb - the callback to pass to continueBuildQueryCb that is called when building the sessions query is complete
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {function} - the callback to call once the session query is built or an error occurs
   */
  function addViewToQuery (req, query, continueBuildQueryCb, finalCb, queryOverride = null) {
    let err;
    let viewExpression;

    // queryOverride can supercede req.query if specified
    let reqQuery = queryOverride || req.query;

    if (req.user.views && req.user.views[reqQuery.view]) { // it's a user's view
      try {
        viewExpression = molochparser.parse(req.user.views[reqQuery.view].expression);
        query.query.bool.filter.push(viewExpression);
      } catch (e) {
        console.log(`ERROR - User expression (${reqQuery.view}) doesn't compile -`, e);
        err = e;
      }
      return continueBuildQueryCb(req, query, err, finalCb, queryOverride);
    } else { // it's a shared view
      Db.getUser('_moloch_shared', (err, sharedUser) => {
        if (sharedUser && sharedUser.found) {
          sharedUser = sharedUser._source;
          sharedUser.views = sharedUser.views || {};
          for (let viewName in sharedUser.views) {
            if (viewName === reqQuery.view) {
              viewExpression = sharedUser.views[viewName].expression;
              break;
            }
          }
          if (sharedUser.views[reqQuery.view]) {
            try {
              viewExpression = molochparser.parse(sharedUser.views[reqQuery.view].expression);
              query.query.bool.filter.push(viewExpression);
            } catch (e) {
              console.log(`ERROR - Shared user expression (${reqQuery.view}) doesn't compile -`, e);
              err = e;
            }
          }
          return continueBuildQueryCb(req, query, err, finalCb, queryOverride);
        }
      });
    }
  }

  function csvListWriter (req, res, list, fields, pcapWriter, extension) {
    if (list.length > 0 && list[0].fields) {
      list = list.sort(function (a, b) { return a.fields.lastPacket - b.fields.lastPacket; });
    } else if (list.length > 0 && list[0]._source) {
      list = list.sort(function (a, b) { return a._source.lastPacket - b._source.lastPacket; });
    }

    let fieldObjects = Config.getDBFieldsMap();

    if (fields) {
      let columnHeaders = [];
      for (let i = 0, ilen = fields.length; i < ilen; ++i) {
        if (fieldObjects[fields[i]] !== undefined) {
          columnHeaders.push(fieldObjects[fields[i]].friendlyName);
        }
      }
      res.write(columnHeaders.join(', '));
      res.write('\r\n');
    }

    for (let j = 0, jlen = list.length; j < jlen; j++) {
      let sessionData = ViewerUtils.flattenFields(list[j]._source || list[j].fields);
      sessionData._id = list[j]._id;

      if (!fields) { continue; }

      let values = [];
      for (let k = 0, klen = fields.length; k < klen; ++k) {
        let value = sessionData[fields[k]];
        if (fields[k] === 'ipProtocol' && value) {
          value = Pcap.protocol2Name(value);
        }

        if (Array.isArray(value)) {
          let singleValue = '"' + value.join(', ') + '"';
          values.push(singleValue);
        } else {
          if (value === undefined) {
            value = '';
          } else if (typeof (value) === 'string' && value.includes(',')) {
            if (value.includes('"')) {
              value = value.replace(/"/g, '""');
            }
            value = '"' + value + '"';
          }
          values.push(value);
        }
      }

      res.write(values.join(','));
      res.write('\r\n');
    }

    res.end();
  }

  function sessionsListAddSegments (req, indices, query, list, cb) {
    let processedRo = {};

    // Index all the ids we have, so we don't include them again
    let haveIds = {};
    list.forEach(function (item) {
      haveIds[item._id] = true;
    });

    delete query.aggregations;

    // Do a ro search on each item
    let writes = 0;
    async.eachLimit(list, 10, function (item, nextCb) {
      let fields = item._source || item.fields;
      if (!fields.rootId || processedRo[fields.rootId]) {
        if (writes++ > 100) {
          writes = 0;
          setImmediate(nextCb);
        } else {
          nextCb();
        }
        return;
      }
      processedRo[fields.rootId] = true;

      query.query.bool.filter.push({ term: { rootId: fields.rootId } });
      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err || result === undefined || result.hits === undefined || result.hits.hits === undefined) {
          console.log('ERROR fetching matching sessions', err, result);
          return nextCb(null);
        }
        result.hits.hits.forEach(function (item) {
          if (!haveIds[item._id]) {
            haveIds[item._id] = true;
            list.push(item);
          }
        });
        return nextCb(null);
      });
      query.query.bool.filter.pop();
    }, function (err) {
      cb(err, list);
    });
  }

  function sortFields (session) {
    if (session.tags) {
      session.tags = session.tags.sort();
    }
    if (session.http) {
      if (session.http.requestHeader) {
        session.http.requestHeader = session.http.requestHeader.sort();
      }
      if (session.http.responseHeader) {
        session.http.responseHeader = session.http.responseHeader.sort();
      }
    }
    if (session.email && session.email.headers) {
      session.email.headers = session.email.headers.sort();
    }
    if (session.ipProtocol) {
      session.ipProtocol = Pcap.protocol2Name(session.ipProtocol);
    }
  }

  function localSessionDetailReturnFull (req, res, session, incoming) {
    if (req.packetsOnly) { // only return packets
      res.render('sessionPackets.pug', {
        filename: 'sessionPackets',
        cache: internals.isProduction,
        compileDebug: !internals.isProduction,
        user: req.user,
        session: session,
        data: incoming,
        reqPackets: req.query.packets,
        query: req.query,
        basedir: '/',
        reqFields: Config.headers('headers-http-request'),
        resFields: Config.headers('headers-http-response'),
        emailFields: Config.headers('headers-email'),
        showFrames: req.query.showFrames
      }, function (err, data) {
        if (err) {
          console.trace('ERROR - localSession - ', err);
          return req.next(err);
        }
        res.send(data);
      });
    } else { // return SPI data and packets
      res.send('HOW DID I GET HERE?');
      console.trace('HOW DID I GET HERE');
    }
  }

  function localSessionDetailReturn (req, res, session, incoming) {
    // console.log("ALW", JSON.stringify(incoming));
    var numPackets = req.query.packets || 200;
    if (incoming.length > numPackets) {
      incoming.length = numPackets;
    }

    if (incoming.length === 0) {
      return localSessionDetailReturnFull(req, res, session, []);
    }

    var options = {
      id: session.id,
      nodeName: req.params.nodeName,
      order: [],
      'ITEM-HTTP': {
        order: []
      },
      'ITEM-SMTP': {
        order: []
      },
      'ITEM-CB': {
      }
    };

    if (req.query.needgzip) {
      options['ITEM-HTTP'].order.push('BODY-UNCOMPRESS');
      options['ITEM-SMTP'].order.push('BODY-UNBASE64');
      options['ITEM-SMTP'].order.push('BODY-UNCOMPRESS');
    }

    options.order.push('ITEM-HTTP');
    options.order.push('ITEM-SMTP');

    var decodeOptions = JSON.parse(req.query.decode || '{}');
    for (var key in decodeOptions) {
      if (key.match(/^ITEM/)) {
        options.order.push(key);
      } else {
        options['ITEM-HTTP'].order.push(key);
        options['ITEM-SMTP'].order.push(key);
      }
      options[key] = decodeOptions[key];
    }

    if (req.query.needgzip) {
      options['ITEM-HTTP'].order.push('BODY-UNCOMPRESS');
      options['ITEM-SMTP'].order.push('BODY-UNCOMPRESS');
    }

    options.order.push('ITEM-BYTES');
    options.order.push('ITEM-SORTER');
    if (req.query.needimage) {
      options.order.push('ITEM-LINKBODY');
    }
    if (req.query.base === 'hex') {
      options.order.push('ITEM-HEX');
      options['ITEM-HEX'] = { showOffsets: req.query.line === 'true' };
    } else if (req.query.base === 'ascii') {
      options.order.push('ITEM-ASCII');
    } else if (req.query.base === 'utf8') {
      options.order.push('ITEM-UTF8');
    } else {
      options.order.push('ITEM-NATURAL');
    }
    options.order.push('ITEM-CB');
    options['ITEM-CB'].cb = function (err, outgoing) {
      localSessionDetailReturnFull(req, res, session, outgoing);
    };

    if (Config.debug) {
      console.log('Pipeline options', options);
    }

    decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, incoming));
  }

  function localSessionDetail (req, res) {
    if (!req.query) {
      req.query = { gzip: false, line: false, base: 'natural', packets: 200 };
    }

    req.query.needgzip = req.query.gzip === 'true' || false;
    req.query.needimage = req.query.image === 'true' || false;
    req.query.line = req.query.line || false;
    req.query.base = req.query.base || 'ascii';
    req.query.showFrames = req.query.showFrames === 'true' || false;

    var packets = [];
    module.processSessionId(req.params.id, !req.packetsOnly, null, function (pcap, buffer, cb, i) {
      var obj = {};
      if (buffer.length > 16) {
        try {
          pcap.decode(buffer, obj);
        } catch (e) {
          obj = { ip: { p: 'Error decoding' + e } };
          console.trace('loadSessionDetail error', e.stack);
        }
      } else {
        obj = { ip: { p: 'Empty' } };
      }
      packets[i] = obj;
      cb(null);
    },
    function (err, session) {
      if (err) {
        return res.end('Problem loading packets for ' + ViewerUtils.safeStr(req.params.id) + ' Error: ' + err);
      }
      session.id = req.params.id;
      sortFields(session);

      if (req.query.showFrames && packets.length !== 0) {
        Pcap.packetFlow(session, packets, +req.query.packets || 200, function (err, results, sourceKey, destinationKey) {
          session._err = err;
          session.sourceKey = sourceKey;
          session.destinationKey = destinationKey;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets.length === 0) {
        session._err = 'No pcap data found';
        localSessionDetailReturn(req, res, session, []);
      } else if (packets[0].ether !== undefined && packets[0].ether.data !== undefined) {
        Pcap.reassemble_generic_ether(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip === undefined) {
        session._err = "Couldn't decode pcap file, check viewer log";
        localSessionDetailReturn(req, res, session, []);
      } else if (packets[0].ip.p === 1) {
        Pcap.reassemble_icmp(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 6) {
        var key = session.srcIp;
        Pcap.reassemble_tcp(packets, +req.query.packets || 200, key + ':' + session.srcPort, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 17) {
        Pcap.reassemble_udp(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 132) {
        Pcap.reassemble_sctp(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 50) {
        Pcap.reassemble_esp(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 58) {
        Pcap.reassemble_icmp(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.data !== undefined) {
        Pcap.reassemble_generic_ip(packets, +req.query.packets || 200, function (err, results) {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else {
        session._err = 'Unknown ip.p=' + packets[0].ip.p;
        localSessionDetailReturn(req, res, session, []);
      }
    },
    req.query.needimage ? 10000 : 400, 10);
  }

  function processSessionIdDisk (session, headerCb, packetCb, endCb, limit) {
    let fields;

    function processFile (pcap, pos, i, nextCb) {
      pcap.ref();
      pcap.readPacket(pos, function (packet) {
        switch (packet) {
        case null:
          let msg = util.format(session._id, 'in file', pcap.filename, "couldn't read packet at", pos, 'packet #', i, 'of', fields.packetPos.length);
          console.log('ERROR - processSessionIdDisk -', msg);
          endCb(msg, null);
          break;
        case undefined:
          break;
        default:
          packetCb(pcap, packet, nextCb, i);
          break;
        }
        pcap.unref();
      });
    }

    fields = session._source || session.fields;

    var fileNum;
    var itemPos = 0;
    async.eachLimit(fields.packetPos, limit || 1, function (pos, nextCb) {
      if (pos < 0) {
        fileNum = pos * -1;
        return nextCb(null);
      }

      // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
      var opcap = Pcap.get(fields.node + ':' + fileNum);
      if (!opcap.isOpen()) {
        Db.fileIdToFile(fields.node, fileNum, function (file) {
          if (!file) {
            console.log("WARNING - Only have SPI data, PCAP file no longer available.  Couldn't look up in file table", fields.node + '-' + fileNum);
            return nextCb('Only have SPI data, PCAP file no longer available for ' + fields.node + '-' + fileNum);
          }
          if (file.kekId) {
            file.kek = Config.sectionGet('keks', file.kekId, undefined);
            if (file.kek === undefined) {
              console.log("ERROR - Couldn't find kek", file.kekId, 'in keks section');
              return nextCb("Couldn't find kek " + file.kekId + ' in keks section');
            }
          }

          var ipcap = Pcap.get(fields.node + ':' + file.num);

          try {
            ipcap.open(file.name, file);
          } catch (err) {
            console.log("ERROR - Couldn't open file ", err);
            if (err.code === 'EACCES') {
              // Find all the directories to check
              let checks = [];
              let dir = path.resolve(file.name);
              while ((dir = path.dirname(dir)) !== '/') {
                checks.push(dir);
              }

              // Check them in reverse order, smallest to largest
              let i;
              for (i = checks.length - 1; i >= 0; i--) {
                try {
                  fs.accessSync(checks[i], fs.constants.X_OK);
                } catch (e) {
                  console.log(`NOTE - Directory permissions issue, possible fix "chmod a+x '${checks[i]}'"`);
                  break;
                }
              }

              // No directory issue, check the file itself
              if (i === -1) {
                try {
                  fs.accessSync(file.name, fs.constants.R_OK);
                } catch (e) {
                  console.log(`NOTE - File permissions issue, possible fix "chmod a+r '${file.name}'"`);
                }
              }
            }
            return nextCb("Couldn't open file " + err);
          }

          if (headerCb) {
            headerCb(ipcap, ipcap.readHeader());
            headerCb = null;
          }
          processFile(ipcap, pos, itemPos++, nextCb);
        });
      } else {
        if (headerCb) {
          headerCb(opcap, opcap.readHeader());
          headerCb = null;
        }
        processFile(opcap, pos, itemPos++, nextCb);
      }
    },
    function (pcapErr, results) {
      endCb(pcapErr, fields);
    });
  }

  module.processSessionId = (id, fullSession, headerCb, packetCb, endCb, maxPackets, limit) => {
    var options;
    if (!fullSession) {
      options = { _source: 'node,totPackets,packetPos,srcIp,srcPort,ipProtocol,packetLen' };
    }

    Db.getSession(id, options, (err, session) => {
      if (err || !session.found) {
        console.log('session get error', err, session);
        return endCb('Session not found', null);
      }

      var fields = session._source || session.fields;

      if (maxPackets && fields.packetPos.length > maxPackets) {
        fields.packetPos.length = maxPackets;
      }

      /* Go through the list of prefetch the id to file name if we are running in parallel to
       * reduce the number of elasticsearch queries and problems
       */
      let outstanding = 0; let i; let ilen;

      function fileReadyCb (fileInfo) {
        outstanding--;

        // All of the replies have been received
        if (i === ilen && outstanding === 0) {
          readyToProcess();
        }
      }

      for (i = 0, ilen = fields.packetPos.length; i < ilen; i++) {
        if (fields.packetPos[i] < 0) {
          outstanding++;
          Db.fileIdToFile(fields.node, -1 * fields.packetPos[i], fileReadyCb);
        }
      }

      function readyToProcess () {
        var pcapWriteMethod = Config.getFull(fields.node, 'pcapWriteMethod');
        var psid = processSessionIdDisk;
        var writer = internals.writers[pcapWriteMethod];
        if (writer && writer.processSessionId) {
          psid = writer.processSessionId;
        }

        psid(session, headerCb, packetCb, function (err, fields) {
          if (!fields) {
            return endCb(err, fields);
          }

          if (!fields.tags) {
            fields.tags = [];
          }

          ViewerUtils.fixFields(fields, endCb);
        }, limit);
      }
    });
  };

  module.sessionsListFromQuery = (req, res, fields, cb) => {
    if (req.query.segments && req.query.segments.match(/^(time|all)$/) && fields.indexOf('rootId') === -1) {
      fields.push('rootId');
    }

    module.buildSessionQuery(req, (err, query, indices) => {
      if (err) {
        return res.send('Could not build query.  Err: ' + err);
      }
      query._source = fields;
      if (Config.debug) {
        console.log('sessionsListFromQuery query', JSON.stringify(query, null, 1));
      }
      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err || result.error) {
          console.log('ERROR - Could not fetch list of sessions.  Err: ', err, ' Result: ', result, 'query:', query);
          return res.send('Could not fetch list of sessions.  Err: ' + err + ' Result: ' + result);
        }
        let list = result.hits.hits;
        if (req.query.segments && req.query.segments.match(/^(time|all)$/)) {
          sessionsListAddSegments(req, indices, query, list, function (err, list) {
            cb(err, list);
          });
        } else {
          cb(err, list);
        }
      });
    });
  };

  /**
   * Builds the session query based on req.query
   * @ignore
   * @name buildSessionQuery
   * @param {object} req - the client request
   * @param {function} buildCb - the callback to call when building the query is complete
   * @param {boolean} queryOverride=null - override the client query with overriding query
   * @returns {function} - the callback to call once the session query is built or an error occurs
   */
  module.buildSessionQuery = (req, buildCb, queryOverride = null) => {
    // validate time limit is not exceeded
    let timeLimitExceeded = false;
    let interval;

    // queryOverride can supercede req.query if specified
    let reqQuery = queryOverride || req.query;

    // determineQueryTimes calculates startTime, stopTime, and interval from reqQuery
    let startAndStopParams = ViewerUtils.determineQueryTimes(reqQuery);
    if (startAndStopParams[0] !== undefined) {
      reqQuery.startTime = startAndStopParams[0];
    }
    if (startAndStopParams[1] !== undefined) {
      reqQuery.stopTime = startAndStopParams[1];
    }
    interval = startAndStopParams[2];

    if ((parseInt(reqQuery.date) > parseInt(req.user.timeLimit)) ||
      ((reqQuery.date === '-1') && req.user.timeLimit)) {
      timeLimitExceeded = true;
    } else if ((reqQuery.startTime) && (reqQuery.stopTime) && (req.user.timeLimit) &&
               ((reqQuery.stopTime - reqQuery.startTime) / 3600 > req.user.timeLimit)) {
      timeLimitExceeded = true;
    }

    if (timeLimitExceeded) {
      console.log(`${req.user.userName} trying to exceed time limit: ${req.user.timeLimit} hours`);
      return buildCb(`User time limit (${req.user.timeLimit} hours) exceeded`, {});
    }

    let limit = Math.min(2000000, +reqQuery.length || 100);

    let query = { from: reqQuery.start || 0,
      size: limit,
      timeout: internals.esQueryTimeout,
      query: { bool: { filter: [] } }
    };

    if (query.from === 0) {
      delete query.from;
    }

    if (reqQuery.strictly === 'true') {
      reqQuery.bounding = 'both';
    }

    if ((reqQuery.date && reqQuery.date === '-1') ||
        (reqQuery.segments && reqQuery.segments === 'all')) {
      // interval is already assigned above from result of determineQueryTimes

    } else if (reqQuery.startTime !== undefined && reqQuery.stopTime) {
      switch (reqQuery.bounding) {
      case 'first':
        query.query.bool.filter.push({ range: { firstPacket: { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
        break;
      default:
      case 'last':
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
        break;
      case 'both':
        query.query.bool.filter.push({ range: { firstPacket: { gte: reqQuery.startTime * 1000 } } });
        query.query.bool.filter.push({ range: { lastPacket: { lte: reqQuery.stopTime * 1000 } } });
        break;
      case 'either':
        query.query.bool.filter.push({ range: { firstPacket: { lte: reqQuery.stopTime * 1000 } } });
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      case 'database':
        query.query.bool.filter.push({ range: { timestamp: { gte: reqQuery.startTime * 1000, lte: reqQuery.stopTime * 1000 } } });
        break;
      }
    } else {
      switch (reqQuery.bounding) {
      case 'first':
        query.query.bool.filter.push({ range: { firstPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      default:
      case 'both':
      case 'last':
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      case 'either':
        query.query.bool.filter.push({ range: { firstPacket: { lte: reqQuery.stopTime * 1000 } } });
        query.query.bool.filter.push({ range: { lastPacket: { gte: reqQuery.startTime * 1000 } } });
        break;
      case 'database':
        query.query.bool.filter.push({ range: { timestamp: { gte: reqQuery.startTime * 1000 } } });
        break;
      }
    }

    if (parseInt(reqQuery.facets) === 1) {
      query.aggregations = {};
      // only add map aggregations if requested
      if (reqQuery.map === 'true') {
        query.aggregations = {
          mapG1: { terms: { field: 'srcGEO', size: 1000, min_doc_count: 1 } },
          mapG2: { terms: { field: 'dstGEO', size: 1000, min_doc_count: 1 } },
          mapG3: { terms: { field: 'http.xffGEO', size: 1000, min_doc_count: 1 } }
        };
      }

      query.aggregations.dbHisto = { aggregations: {} };

      let filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;
      for (let i = 0; i < filters.length; i++) {
        let filter = filters[i];

        // Will also grap src/dst of these options instead to show on the timeline
        if (filter === 'totPackets') {
          query.aggregations.dbHisto.aggregations.srcPackets = { sum: { field: 'srcPackets' } };
          query.aggregations.dbHisto.aggregations.dstPackets = { sum: { field: 'dstPackets' } };
        } else if (filter === 'totBytes') {
          query.aggregations.dbHisto.aggregations.srcBytes = { sum: { field: 'srcBytes' } };
          query.aggregations.dbHisto.aggregations.dstBytes = { sum: { field: 'dstBytes' } };
        } else if (filter === 'totDataBytes') {
          query.aggregations.dbHisto.aggregations.srcDataBytes = { sum: { field: 'srcDataBytes' } };
          query.aggregations.dbHisto.aggregations.dstDataBytes = { sum: { field: 'dstDataBytes' } };
        } else {
          query.aggregations.dbHisto.aggregations[filter] = { sum: { field: filter } };
        }
      }

      switch (reqQuery.bounding) {
      case 'first':
        query.aggregations.dbHisto.histogram = { field: 'firstPacket', interval: interval * 1000, min_doc_count: 1 };
        break;
      case 'database':
        query.aggregations.dbHisto.histogram = { field: 'timestamp', interval: interval * 1000, min_doc_count: 1 };
        break;
      default:
        query.aggregations.dbHisto.histogram = { field: 'lastPacket', interval: interval * 1000, min_doc_count: 1 };
        break;
      }
    }

    addSortToQuery(query, reqQuery, 'firstPacket');

    let err = null;

    molochparser.parser.yy = {
      views: req.user.views,
      fieldsMap: Config.getFieldsMap(),
      dbFieldsMap: Config.getDBFieldsMap(),
      prefix: internals.prefix,
      emailSearch: req.user.emailSearch === true,
      lookups: req.lookups,
      lookupTypeMap: internals.lookupTypeMap
    };

    if (reqQuery.expression) {
      // reqQuery.expression = reqQuery.expression.replace(/\\/g, "\\\\");
      try {
        query.query.bool.filter.push(molochparser.parse(reqQuery.expression));
      } catch (e) {
        err = e;
      }
    }

    if (!err && reqQuery.view) {
      addViewToQuery(req, query, ViewerUtils.continueBuildQuery, buildCb, queryOverride);
    } else {
      ViewerUtils.continueBuildQuery(req, query, err, buildCb, queryOverride);
    }
  };

  module.sessionsListFromIds = (req, ids, fields, cb) => {
    let processSegments = false;
    if (req && ((req.query.segments && req.query.segments.match(/^(time|all)$/)) || (req.query.segments && req.query.segments.match(/^(time|all)$/)))) {
      if (fields.indexOf('rootId') === -1) { fields.push('rootId'); }
      processSegments = true;
    }

    let list = [];
    const nonArrayFields = ['ipProtocol', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort', 'srcGEO', 'dstIp', 'dstPort', 'dstGEO', 'totBytes', 'totDataBytes', 'totPackets', 'node', 'rootId', 'http.xffGEO'];
    let fixFields = nonArrayFields.filter(function (x) { return fields.indexOf(x) !== -1; });

    async.eachLimit(ids, 10, function (id, nextCb) {
      Db.getSession(id, { _source: fields.join(',') }, function (err, session) {
        if (err) {
          return nextCb(null);
        }

        for (let i = 0; i < fixFields.length; i++) {
          let field = fixFields[i];
          if (session._source[field] && Array.isArray(session._source[field])) {
            session._source[field] = session._source[field][0];
          }
        }

        list.push(session);
        nextCb(null);
      });
    }, function (err) {
      if (processSegments) {
        module.buildSessionQuery(req, (err, query, indices) => {
          query._source = fields;
          sessionsListAddSegments(req, indices, query, list, function (err, list) {
            cb(err, list);
          });
        });
      } else {
        cb(err, list);
      }
    });
  };

  module.isLocalView = (node, yesCb, noCb) => {
    if (internals.isLocalViewRegExp && node.match(internals.isLocalViewRegExp)) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because matches ${internals.isLocalViewRegExp}`);
      }
      return yesCb();
    }

    var pcapWriteMethod = Config.getFull(node, 'pcapWriteMethod');
    var writer = internals.writers[pcapWriteMethod];
    if (writer && writer.localNode === false) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because of writer`);
      }
      return yesCb();
    }
    return Db.isLocalView(node, yesCb, noCb);
  };

  module.getViewUrl = (node, cb) => {
    if (Array.isArray(node)) {
      node = node[0];
    }

    var url = Config.getFull(node, 'viewUrl');
    if (url) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${url} because viewUrl was set for ${node} in config file`);
      }
      cb(null, url, url.slice(0, 5) === 'https' ? https : http);
      return;
    }

    Db.molochNodeStatsCache(node, function (err, stat) {
      if (err) {
        return cb(err);
      }

      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is using ${stat.hostname} from elasticsearch stats index`);
      }

      if (Config.isHTTPS(node)) {
        cb(null, 'https://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005'), https);
      } else {
        cb(null, 'http://' + stat.hostname + ':' + Config.getFull(node, 'viewPort', '8005'), http);
      }
    });
  };

  module.proxyRequest = (req, res, errCb) => {
    ViewerUtils.noCache(req, res);

    module.getViewUrl(req.params.nodeName, function (err, viewUrl, client) {
      if (err) {
        if (errCb) {
          return errCb(err);
        }
        console.log('ERROR - getViewUrl - node:', req.params.nodeName, 'err:', err);
        return res.send(`Can't find view url for '${ViewerUtils.safeStr(req.params.nodeName)}' check viewer logs on '${Config.hostName()}'`);
      }
      var info = url.parse(viewUrl);
      info.path = req.url;
      info.agent = (client === http ? internals.httpAgent : internals.httpsAgent);
      info.timeout = 20 * 60 * 1000;
      ViewerUtils.addAuth(info, req.user, req.params.nodeName);
      ViewerUtils.addCaTrust(info, req.params.nodeName);

      var preq = client.request(info, function (pres) {
        if (pres.headers['content-type']) {
          res.setHeader('content-type', pres.headers['content-type']);
        }
        if (pres.headers['content-disposition']) {
          res.setHeader('content-disposition', pres.headers['content-disposition']);
        }
        pres.on('data', function (chunk) {
          res.write(chunk);
        });
        pres.on('end', function () {
          res.end();
        });
      });

      preq.on('error', function (e) {
        if (errCb) {
          return errCb(e);
        }
        console.log("ERROR - Couldn't proxy request=", info, '\nerror=', e, 'You might want to run viewer with two --debug for more info');
        res.send(`Error talking to node '${ViewerUtils.safeStr(req.params.nodeName)}' using host '${info.host}' check viewer logs on '${Config.hostName()}'`);
      });
      preq.end();
    });
  };

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query and returns the query and the elasticsearch indices to the client.
   * @name /api/buildQuery
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} order - Comma separated list of db field names to sort on. Data is sorted in order of the list supplied. Optionally can be followed by :asc or :desc for ascending or descending sorting.
   * @param {string} fields - Comma separated list of db field names to return.
     Default is ipProtocol,rootId,totDataBytes,srcDataBytes,dstDataBytes,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,srcPackets,dstPackets,totBytes,srcBytes,dstBytes,node,http.uri,srcGEO,dstGEO,email.subject,email.src,email.dst,email.filename,dns.host,cert,irc.channel
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getQuery = (req, res) => {
    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        return res.send({
          recordsTotal: 0,
          recordsFiltered: 0,
          error: bsqErr.toString()
        });
      }

      if (req.query.fields) {
        query._source = ViewerUtils.queryValueToArray(req.query.fields);
      }

      res.send({ 'esquery': query, 'indices': indices });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them to the client.
   * @name /api/sessions
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} order - Comma separated list of db field names to sort on. Data is sorted in order of the list supplied. Optionally can be followed by :asc or :desc for ascending or descending sorting.
   * @param {string} fields - Comma separated list of db field names to return.
     Default is ipProtocol,rootId,totDataBytes,srcDataBytes,dstDataBytes,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,srcPackets,dstPackets,totBytes,srcBytes,dstBytes,node,http.uri,srcGEO,dstGEO,email.subject,email.src,email.dst,email.filename,dns.host,cert,irc.channel
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSessions = (req, res) => {
    let map = {};
    let graph = {};

    let options;
    if (req.query.cancelId) {
      options = {
        cancelId: `${req.user.userId}::${req.query.cancelId}`
      };
    }

    let response = {
      data: [],
      map: {},
      graph: {},
      recordsTotal: 0,
      recordsFiltered: 0,
      health: Db.healthCache()
    };

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        response.error = bsqErr.toString();
        return res.send(response);
      }

      let addMissing = false;
      if (req.query.fields) {
        query._source = ViewerUtils.queryValueToArray(req.query.fields);
        ['node', 'srcIp', 'srcPort', 'dstIp', 'dstPort'].forEach((item) => {
          if (query._source.indexOf(item) === -1) {
            query._source.push(item);
          }
        });
      } else {
        addMissing = true;
        query._source = [
          'ipProtocol', 'rootId', 'totDataBytes', 'srcDataBytes',
          'dstDataBytes', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort',
          'dstIp', 'dstPort', 'totPackets', 'srcPackets', 'dstPackets',
          'totBytes', 'srcBytes', 'dstBytes', 'node', 'http.uri', 'srcGEO',
          'dstGEO', 'email.subject', 'email.src', 'email.dst', 'email.filename',
          'dns.host', 'cert', 'irc.channel', 'http.xffGEO'
        ];
      }

      if (query.aggregations && query.aggregations.dbHisto) {
        graph.interval = query.aggregations.dbHisto.histogram.interval;
      }

      if (Config.debug) {
        console.log(`sessions.json ${indices} query`, JSON.stringify(query, null, 1));
      }

      Promise.all([
        Db.searchPrimary(indices, 'session', query, options),
        Db.numberOfDocuments('sessions2-*'),
        Db.healthCachePromise()
      ]).then(([sessions, total, health]) => {
        if (Config.debug) {
          console.log('sessions.json result', util.inspect(sessions, false, 50));
        }

        if (sessions.error) { throw sessions.err; }

        map = ViewerUtils.mapMerge(sessions.aggregations);
        graph = ViewerUtils.graphMerge(req, query, sessions.aggregations);

        let results = { total: sessions.hits.total, results: [] };
        async.each(sessions.hits.hits, (hit, hitCb) => {
          let fields = hit._source || hit.fields;
          if (fields === undefined) {
            return hitCb(null);
          }

          fields.id = Db.session2Sid(hit);

          if (parseInt(req.query.flatten) === 1) {
            fields = ViewerUtils.flattenFields(fields);
          }

          if (addMissing) {
            ['srcPackets', 'dstPackets', 'srcBytes', 'dstBytes', 'srcDataBytes', 'dstDataBytes'].forEach((item) => {
              if (fields[item] === undefined) {
                fields[item] = -1;
              }
            });
            results.results.push(fields);
            return hitCb();
          } else {
            ViewerUtils.fixFields(fields, function () {
              results.results.push(fields);
              return hitCb();
            });
          }
        }, function () {
          try {
            response.map = map;
            response.graph = graph;
            response.health = health;
            response.data = (results ? results.results : []);
            response.recordsTotal = total.count;
            response.recordsFiltered = (results ? results.total : 0);
            res.logCounts(response.data.length, response.recordsFiltered, response.recordsTotal);
            return res.send(response);
          } catch (e) {
            console.trace('fetch sessions error', e.stack);
            response.error = e.toString();
            return res.send(response);
          }
        });
      }).catch((err) => {
        console.log('ERROR - sessions error', err);
        response.error = err.toString();
        return res.send(response);
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them as CSV to the client.
   * @name /api/sessions
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} length=100 - The number of items to return. Defaults to 100, Max is 2,000,000
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} order - Comma separated list of db field names to sort on. Data is sorted in order of the list supplied. Optionally can be followed by :asc or :desc for ascending or descending sorting.
   * @param {string} fields - Comma separated list of db field names to return.
     Default is ipProtocol,rootId,totDataBytes,srcDataBytes,dstDataBytes,firstPacket,lastPacket,srcIp,srcPort,dstIp,dstPort,totPackets,srcPackets,dstPackets,totBytes,srcBytes,dstBytes,node,http.uri,srcGEO,dstGEO,email.subject,email.src,email.dst,email.filename,dns.host,cert,irc.channel
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSessionsCSV = (req, res) => {
    ViewerUtils.noCache(req, res, 'text/csv');

    // default fields to display in csv
    let fields = [
      'ipProtocol', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort', 'srcGEO',
      'dstIp', 'dstPort', 'dstGEO', 'totBytes', 'totDataBytes', 'totPackets', 'node'
    ];

    // save requested fields because sessionsListFromQuery returns fields with
    // "rootId" appended onto the end
    let reqFields = fields;

    if (req.query.fields) {
      fields = reqFields = ViewerUtils.queryValueToArray(req.query.fields);
    }

    if (req.query.ids) {
      const ids = ViewerUtils.queryValueToArray(req.query.ids);
      module.sessionsListFromIds(req, ids, fields, (err, list) => {
        csvListWriter(req, res, list, reqFields);
      });
    } else {
      module.sessionsListFromQuery(req, res, fields, (err, list) => {
        csvListWriter(req, res, list, reqFields);
      });
    }
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of field values with counts and returns them to the client.
   * @name /api/spiview
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} facets=0 - 1 = include the aggregation information for maps and timeline graphs. Defaults to 0
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} spi - Comma separated list of db fields to return. Optionally can be followed by :{count} to specify the number of values returned for the field (defaults to 100).
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSPIView = (req, res) => {
    if (req.query.spi === undefined) {
      return res.send({ spi: {}, recordsTotal: 0, recordsFiltered: 0 });
    }

    const spiDataMaxIndices = +Config.get('spiDataMaxIndices', 4);

    if (parseInt(req.query.date) === -1 && spiDataMaxIndices !== -1) {
      return res.send({ spi: {}, bsqErr: "'All' date range not allowed for spiview query" });
    }

    let response = {
      spi: {},
      health: Db.healthCache()
    };

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        response.error = bsqErr.toString();
        return res.send(response);
      }

      delete query.sort;

      if (!query.aggregations) {
        query.aggregations = {};
      }

      if (parseInt(req.query.facets) === 1) {
        query.aggregations.protocols = {
          terms: { field: 'protocol', size: 1000 }
        };
      }

      ViewerUtils.queryValueToArray(req.query.spi).forEach(function (item) {
        const parts = item.split(':');
        if (parts[0] === 'fileand') {
          query.aggregations[parts[0]] = { terms: { field: 'node', size: 1000 }, aggregations: { fileId: { terms: { field: 'fileId', size: parts.length > 1 ? parseInt(parts[1], 10) : 10 } } } };
        } else {
          query.aggregations[parts[0]] = { terms: { field: parts[0] } };

          if (parts.length > 1) {
            query.aggregations[parts[0]].terms.size = parseInt(parts[1], 10);
          }
        }
      });

      query.size = 0;

      if (Config.debug) {
        console.log('spiview.json query', JSON.stringify(query), 'indices', indices);
      }

      let graph, map;

      const indicesa = indices.split(',');
      if (spiDataMaxIndices !== -1 && indicesa.length > spiDataMaxIndices) {
        bsqErr = 'To save ES from blowing up, reducing number of spi data indices searched from ' + indicesa.length + ' to ' + spiDataMaxIndices + '.  This can be increased by setting spiDataMaxIndices in the config file.  Indices being searched: ';
        indices = indicesa.slice(-spiDataMaxIndices).join(',');
        bsqErr += indices;
      }

      let protocols;
      let recordsFiltered = 0;

      Promise.all([Db.searchPrimary(indices, 'session', query, null),
        Db.numberOfDocuments('sessions2-*'),
        Db.healthCachePromise()
      ]).then(([sessions, total, health]) => {
        if (Config.debug) {
          console.log('spiview.json result', util.inspect(sessions, false, 50));
        }

        if (sessions.error) {
          bsqErr = ViewerUtils.errorString(null, sessions);
          console.log('spiview.json ERROR', (sessions ? sessions.error : null));
          sendResult();
          return;
        }

        recordsFiltered = sessions.hits.total;

        if (!sessions.aggregations) {
          sessions.aggregations = {};
          for (let spi in query.aggregations) {
            sessions.aggregations[spi] = { sum_other_doc_count: 0, buckets: [] };
          }
        }

        if (sessions.aggregations.ipProtocol) {
          sessions.aggregations.ipProtocol.buckets.forEach(function (item) {
            item.key = Pcap.protocol2Name(item.key);
          });
        }

        if (parseInt(req.query.facets) === 1) {
          protocols = {};
          map = ViewerUtils.mapMerge(sessions.aggregations);
          graph = ViewerUtils.graphMerge(req, query, sessions.aggregations);
          sessions.aggregations.protocols.buckets.forEach(function (item) {
            protocols[item.key] = item.doc_count;
          });

          delete sessions.aggregations.mapG1;
          delete sessions.aggregations.mapG2;
          delete sessions.aggregations.mapG3;
          delete sessions.aggregations.dbHisto;
          delete sessions.aggregations.byHisto;
          delete sessions.aggregations.protocols;
        }

        function sendResult () {
          try {
            response.map = map;
            response.graph = graph;
            response.error = bsqErr;
            response.health = health;
            response.protocols = protocols;
            response.recordsTotal = total.count;
            response.spi = sessions.aggregations;
            response.recordsFiltered = recordsFiltered;
            res.logCounts(response.spi.count, response.recordsFiltered, response.total);
            return res.send(response);
          } catch (e) {
            console.trace('fetch spiview error', e.stack);
            response.error = e.toString();
            return res.send(response);
          }
        }

        if (!sessions.aggregations.fileand) {
          return sendResult();
        }

        let sodc = 0;
        let nresults = [];
        async.each(sessions.aggregations.fileand.buckets, function (nobucket, cb) {
          sodc += nobucket.fileId.sum_other_doc_count;
          async.each(nobucket.fileId.buckets, function (fsitem, cb) {
            Db.fileIdToFile(nobucket.key, fsitem.key, function (file) {
              if (file && file.name) {
                nresults.push({ key: file.name, doc_count: fsitem.doc_count });
              }
              cb();
            });
          }, function () {
            cb();
          });
        }, function () {
          nresults = nresults.sort(function (a, b) {
            if (a.doc_count === b.doc_count) {
              return a.key.localeCompare(b.key);
            }
            return b.doc_count - a.doc_count;
          });
          sessions.aggregations.fileand = { doc_count_error_upper_bound: 0, sum_other_doc_count: sodc, buckets: nresults };
          return sendResult();
        });
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of values for a field with counts and graph data and returns them to the client.
   * @name /api/spigraph
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} field=node - The database field to get data for. Defaults to "node".
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSPIGraph = (req, res) => {
    req.query.facets = 1;

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      let results = { items: [], graph: {}, map: {} };
      if (bsqErr) {
        return res.molochError(403, bsqErr.toString());
      }

      let options;
      if (req.query.cancelId) {
        options = { cancelId: `${req.user.userId}::${req.query.cancelId}` };
      }

      delete query.sort;
      query.size = 0;
      const size = +req.query.size || 20;

      let field = req.query.field || 'node';

      if (req.query.exp === 'ip.dst:port') { field = 'ip.dst:port'; }

      if (field === 'ip.dst:port') {
        query.aggregations.field = { terms: { field: 'dstIp', size: size }, aggregations: { sub: { terms: { field: 'dstPort', size: size } } } };
      } else if (field === 'fileand') {
        query.aggregations.field = { terms: { field: 'node', size: 1000 }, aggregations: { sub: { terms: { field: 'fileId', size: size } } } };
      } else {
        query.aggregations.field = { terms: { field: field, size: size * 2 } };
      }

      Promise.all([
        Db.healthCachePromise(),
        Db.numberOfDocuments('sessions2-*'),
        Db.searchPrimary(indices, 'session', query, options)
      ]).then(([health, total, result]) => {
        if (result.error) { throw result.error; }

        results.health = health;
        results.recordsTotal = total.count;
        results.recordsFiltered = result.hits.total;

        results.graph = ViewerUtils.graphMerge(req, query, result.aggregations);
        results.map = ViewerUtils.mapMerge(result.aggregations);

        if (!result.aggregations) {
          result.aggregations = { field: { buckets: [] } };
        }

        let aggs = result.aggregations.field.buckets;
        let filter = { term: {} };
        let sfilter = { term: {} };
        query.query.bool.filter.push(filter);

        if (field === 'ip.dst:port') {
          query.query.bool.filter.push(sfilter);
        }

        delete query.aggregations.field;

        let queriesInfo = [];
        function endCb () {
          queriesInfo = queriesInfo.sort((a, b) => { return b.doc_count - a.doc_count; }).slice(0, size * 2);
          let queries = queriesInfo.map((item) => { return item.query; });

          Db.msearch(indices, 'session', queries, options, function (err, result) {
            if (!result.responses) {
              return res.send(results);
            }

            result.responses.forEach(function (item, i) {
              let response = {
                name: queriesInfo[i].key,
                count: queriesInfo[i].doc_count
              };

              response.graph = ViewerUtils.graphMerge(req, query, result.responses[i].aggregations);

              let histoKeys = Object.keys(results.graph).filter(i => i.toLowerCase().includes('histo'));
              let xMinName = histoKeys.reduce((prev, curr) => results.graph[prev][0][0] < results.graph[curr][0][0] ? prev : curr);
              let histoXMin = results.graph[xMinName][0][0];
              let xMaxName = histoKeys.reduce((prev, curr) => {
                return results.graph[prev][results.graph[prev].length - 1][0] > results.graph[curr][results.graph[curr].length - 1][0] ? prev : curr;
              });
              let histoXMax = results.graph[xMaxName][results.graph[xMaxName].length - 1][0];

              if (response.graph.xmin === null) {
                response.graph.xmin = results.graph.xmin || histoXMin;
              }

              if (response.graph.xmax === null) {
                response.graph.xmax = results.graph.xmax || histoXMax;
              }

              response.map = ViewerUtils.mapMerge(result.responses[i].aggregations);

              results.items.push(response);
              histoKeys.forEach(item => {
                response[item] = 0.0;
              });

              let graph = response.graph;
              for (let j = 0; j < histoKeys.length; j++) {
                item = histoKeys[j];
                for (let i = 0; i < graph[item].length; i++) {
                  response[item] += graph[item][i][1];
                }
              }

              if (graph.totPacketsTotal !== undefined) {
                response.totPacketsHisto = graph.totPacketsTotal;
              }
              if (graph.totDataBytesTotal !== undefined) {
                response.totDataBytesHisto = graph.totDataBytesTotal;
              }
              if (graph.totBytesTotal !== undefined) {
                response.totBytesHisto = graph.totBytesTotal;
              }

              if (results.items.length === result.responses.length) {
                let s = req.query.sort || 'sessionsHisto';
                results.items = results.items.sort(function (a, b) {
                  let result;
                  if (s === 'name') {
                    result = a.name.localeCompare(b.name);
                  } else {
                    result = b[s] - a[s];
                  }
                  return result;
                }).slice(0, size);
                return res.send(results);
              }
            });
          });
        }

        let intermediateResults = [];
        function findFileNames () {
          async.each(intermediateResults, function (fsitem, cb) {
            let split = fsitem.key.split(':');
            let node = split[0];
            let fileId = split[1];
            Db.fileIdToFile(node, fileId, function (file) {
              if (file && file.name) {
                queriesInfo.push({ key: file.name, doc_count: fsitem.doc_count, query: fsitem.query });
              }
              cb();
            });
          }, function () {
            endCb();
          });
        }

        aggs.forEach((item) => {
          if (field === 'ip.dst:port') {
            filter.term.dstIp = item.key;
            let sep = (item.key.indexOf(':') === -1) ? ':' : '.';
            item.sub.buckets.forEach((sitem) => {
              sfilter.term.dstPort = sitem.key;
              queriesInfo.push({ key: item.key + sep + sitem.key, doc_count: sitem.doc_count, query: JSON.stringify(query) });
            });
          } else if (field === 'fileand') {
            filter.term.node = item.key;
            item.sub.buckets.forEach((sitem) => {
              sfilter.term.fileand = sitem.key;
              intermediateResults.push({ key: filter.term.node + ':' + sitem.key, doc_count: sitem.doc_count, query: JSON.stringify(query) });
            });
          } else {
            filter.term[field] = item.key;
            queriesInfo.push({ key: item.key, doc_count: item.doc_count, query: JSON.stringify(query) });
          }
        });

        if (field === 'fileand') { return findFileNames(); }

        return endCb();
      }).catch((err) => {
        console.log('spigraph.json error', err);
        return res.molochError(403, ViewerUtils.errorString(err));
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of values for each field with counts and returns them to the client.
   * @name /api/spigraph
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} exp - Comma separated list of db fields to populate the graph/table.
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getSPIGraphHierarchy = (req, res) => {
    if (req.query.exp === undefined) {
      return res.molochError(403, 'Missing exp parameter');
    }

    let fields = [];
    let parts = req.query.exp.split(',');
    for (let i = 0; i < parts.length; i++) {
      if (internals.scriptAggs[parts[i]] !== undefined) {
        fields.push(internals.scriptAggs[parts[i]]);
        continue;
      }
      let field = Config.getFieldsMap()[parts[i]];
      if (!field) {
        return res.molochError(403, `Unknown expression ${parts[i]}\n`);
      }
      fields.push(field);
    }

    module.buildSessionQuery(req, (err, query, indices) => {
      query.size = 0; // Don't need any real results, just aggregations
      delete query.sort;
      delete query.aggregations;
      const size = +req.query.size || 20;

      if (!query.query.bool.must) {
        query.query.bool.must = [];
      }

      let lastQ = query;
      for (let i = 0; i < fields.length; i++) {
        // Require that each field exists
        query.query.bool.must.push({ exists: { field: fields[i].dbField } });

        if (fields[i].script) {
          lastQ.aggregations = { field: { terms: { script: { lang: 'painless', source: fields[i].script }, size: size } } };
        } else {
          lastQ.aggregations = { field: { terms: { field: fields[i].dbField, size: size } } };
        }
        lastQ = lastQ.aggregations.field;
      }

      if (Config.debug > 2) {
        console.log('spigraph pie aggregations', indices, JSON.stringify(query, false, 2));
      }

      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err) {
          console.log('spigraphpie ERROR', err);
          res.status(400);
          return res.end(err);
        }

        if (Config.debug > 2) {
          console.log('result', JSON.stringify(result, false, 2));
        }

        // format the data for the pie graph
        let hierarchicalResults = { name: 'Top Talkers', children: [] };
        function addDataToPie (buckets, addTo) {
          for (let i = 0; i < buckets.length; i++) {
            let bucket = buckets[i];
            addTo.push({
              name: bucket.key,
              size: bucket.doc_count
            });
            if (bucket.field) {
              addTo[i].children = [];
              addTo[i].size = undefined; // size is interpreted from children
              addTo[i].sizeValue = bucket.doc_count; // keep sizeValue for display
              addDataToPie(bucket.field.buckets, addTo[i].children);
            }
          }
        }

        let grandparent;
        let tableResults = [];
        // assumes only 3 levels deep
        function addDataToTable (buckets, parent) {
          for (let i = 0; i < buckets.length; i++) {
            let bucket = buckets[i];
            if (bucket.field) {
              if (parent) { grandparent = parent; }
              addDataToTable(bucket.field.buckets, {
                name: bucket.key,
                size: bucket.doc_count
              });
            } else {
              tableResults.push({
                parent: parent,
                grandparent: grandparent,
                name: bucket.key,
                size: bucket.doc_count
              });
            }
          }
        }

        addDataToPie(result.aggregations.field.buckets, hierarchicalResults.children);
        addDataToTable(result.aggregations.field.buckets);

        return res.send({
          success: true,
          tableResults: tableResults,
          hierarchicalResults: hierarchicalResults
        });
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets a list of unique field values (with or without counts) and sends them to the client.
   * @name /api/unique.txt
   * @param {number} counts=0 - Whether to return counts with he list of unique field values. Defaults to 0. 0 = no counts, 1 - counts.
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} exp - Comma separated list of expression field names to return.
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
    'first' - First Packet: the timestamp of the first packet received for the session.
    'last' - Last Packet: The timestamp of the last packet received for the session.
    'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
    'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
    'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getUnique = (req, res) => {
    ViewerUtils.noCache(req, res, 'text/plain; charset=utf-8');

    if (req.query.field === undefined && req.query.exp === undefined) {
      return res.send('Missing field or exp parameter');
    }

    /* How should the results be written.  Use setImmediate to not blow stack frame */
    let writeCb;
    let doneCb;
    let items = [];
    let aggSize = +Config.get('maxAggSize', 10000);

    if (req.query.autocomplete !== undefined) {
      if (!Config.get('valueAutoComplete', !Config.get('multiES', false))) {
        res.send([]);
        return;
      }

      let spiDataMaxIndices = +Config.get('spiDataMaxIndices', 4);
      if (spiDataMaxIndices !== -1) {
        if (req.query.date === '-1' ||
            (req.query.date !== undefined && +req.query.date > spiDataMaxIndices)) {
          console.log(`INFO For autocomplete replacing date=${ViewerUtils.safeStr(req.query.date)} with ${spiDataMaxIndices}`);
          req.query.date = spiDataMaxIndices;
        }
      }

      aggSize = 1000; // lower agg size for autocomplete
      doneCb = function () {
        res.send(items);
      };
      writeCb = function (item) {
        items.push(item.key);
      };
    } else if (parseInt(req.query.counts, 10) || 0) {
      writeCb = function (item) {
        res.write(`${item.key}, ${item.doc_count}\n`);
      };
    } else {
      writeCb = function (item) {
        res.write(`${item.key}\n`);
      };
    }

    /* How should each item be processed. */
    let eachCb = writeCb;

    if (req.query.field.match(/(ip.src:port.src|a1:p1|srcIp:srtPort|ip.src:srcPort|ip.dst:port.dst|a2:p2|dstIp:dstPort|ip.dst:dstPort)/)) {
      eachCb = function (item) {
        let sep = (item.key.indexOf(':') === -1) ? ':' : '.';
        item.field2.buckets.forEach((item2) => {
          item2.key = item.key + sep + item2.key;
          writeCb(item2);
        });
      };
    }

    module.buildSessionQuery(req, (err, query, indices) => {
      delete query.sort;
      delete query.aggregations;

      if (req.query.field.match(/(ip.src:port.src|a1:p1|srcIp:srcPort|ip.src:srcPort)/)) {
        query.aggregations = { field: { terms: { field: 'srcIp', size: aggSize }, aggregations: { field2: { terms: { field: 'srcPort', size: 100 } } } } };
      } else if (req.query.field.match(/(ip.dst:port.dst|a2:p2|dstIp:dstPort|ip.dst:dstPort)/)) {
        query.aggregations = { field: { terms: { field: 'dstIp', size: aggSize }, aggregations: { field2: { terms: { field: 'dstPort', size: 100 } } } } };
      } else if (req.query.field === 'fileand') {
        query.aggregations = { field: { terms: { field: 'node', size: aggSize }, aggregations: { field2: { terms: { field: 'fileId', size: 100 } } } } };
      } else {
        query.aggregations = { field: { terms: { field: req.query.field, size: aggSize } } };
      }

      query.size = 0;
      console.log('unique aggregations', indices, JSON.stringify(query));

      function findFileNames (result) {
        let intermediateResults = [];
        let aggs = result.aggregations.field.buckets;
        aggs.forEach((item) => {
          item.field2.buckets.forEach((sitem) => {
            intermediateResults.push({ key: item.key + ':' + sitem.key, doc_count: sitem.doc_count });
          });
        });

        async.each(intermediateResults, (fsitem, cb) => {
          let split = fsitem.key.split(':');
          let node = split[0];
          let fileId = split[1];
          Db.fileIdToFile(node, fileId, function (file) {
            if (file && file.name) {
              eachCb({ key: file.name, doc_count: fsitem.doc_count });
            }
            cb();
          });
        }, function () {
          return res.end();
        });
      }

      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err) {
          console.log('Error', query, err);
          return doneCb ? doneCb() : res.end();
        }
        if (Config.debug) {
          console.log('unique.txt result', util.inspect(result, false, 50));
        }
        if (!result.aggregations || !result.aggregations.field) {
          return doneCb ? doneCb() : res.end();
        }

        if (req.query.field === 'fileand') {
          return findFileNames(result);
        }

        for (let i = 0, ilen = result.aggregations.field.buckets.length; i < ilen; i++) {
          eachCb(result.aggregations.field.buckets[i]);
        }

        return doneCb ? doneCb() : res.end();
      });
    });
  };

  /**
   * POST/GET (preferred method is POST)
   *
   * Builds an elasticsearch session query. Gets an intersection of unique field values (with or without counts) and sends them to the client.
   * @name /api/multiunique.txt
   * @param {number} counts=0 - Whether to return counts with he list of unique field values. Defaults to 0. 0 = no counts, 1 - counts.
   * @param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1
   * @param {string} expression - The search expression string
   * @param {number} startTime - If the date parameter is not set, this is the start time of data to return. Format is seconds since Unix EPOC.
   * @param {number} stopTime  - If the date parameter is not set, this is the stop time of data to return. Format is seconds since Unix EPOC.
   * @param {string} view - The view name to apply before the expression.
   * @param {string} exp - The expression field to return unique data for. Either exp or field is required, field is given priority if both are present.
   * @param {string} field - The database field to return unique data for. Either exp or field is required, field is given priority if both are present.
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} Sends the response to the client
   */
  module.getMultiunique = (req, res) => {
    ViewerUtils.noCache(req, res, 'text/plain; charset=utf-8');

    if (req.query.exp === undefined) {
      return res.send('Missing exp parameter');
    }

    let fields = [];
    let parts = req.query.exp.split(',');
    for (let i = 0; i < parts.length; i++) {
      let field = Config.getFieldsMap()[parts[i]];
      if (!field) {
        return res.send(`Unknown expression ${parts[i]}\n`);
      }
      fields.push(field);
    }

    let separator = req.query.separator || ', ';
    let doCounts = parseInt(req.query.counts, 10) || 0;

    let results = [];
    function printUnique (buckets, line) {
      for (let i = 0; i < buckets.length; i++) {
        if (buckets[i].field) {
          printUnique(buckets[i].field.buckets, line + buckets[i].key + separator);
        } else {
          results.push({ line: line + buckets[i].key, count: buckets[i].doc_count });
        }
      }
    }

    module.buildSessionQuery(req, (err, query, indices) => {
      delete query.sort;
      delete query.aggregations;
      query.size = 0;

      if (!query.query.bool.must) {
        query.query.bool.must = [];
      }

      let lastQ = query;
      for (let i = 0; i < fields.length; i++) {
        query.query.bool.must.push({ exists: { field: fields[i].dbField } });
        lastQ.aggregations = { field: { terms: { field: fields[i].dbField, size: +Config.get('maxAggSize', 10000) } } };
        lastQ = lastQ.aggregations.field;
      }

      if (Config.debug > 2) {
        console.log('multiunique aggregations', indices, JSON.stringify(query, false, 2));
      }
      Db.searchPrimary(indices, 'session', query, null, function (err, result) {
        if (err) {
          console.log('multiunique ERROR', err);
          res.status(400);
          return res.end(err);
        }

        if (Config.debug > 2) {
          console.log('result', JSON.stringify(result, false, 2));
        }
        printUnique(result.aggregations.field.buckets, '');

        if (req.query.sort !== 'field') {
          results = results.sort(function (a, b) { return b.count - a.count; });
        }

        if (doCounts) {
          for (let i = 0; i < results.length; i++) {
            res.write(results[i].line + separator + results[i].count + '\n');
          }
        } else {
          for (let i = 0; i < results.length; i++) {
            res.write(results[i].line + '\n');
          }
        }
        return res.end();
      });
    });
  };

  /**
   * GET
   *
   * Gets SPI data for a session
   * @name /:nodeName/session/:id/detail
   * @returns {object} Sends the response to the client
   */
  module.getDetail = (req, res) => {
    Db.getSession(req.params.id, {}, function (err, session) {
      if (err || !session.found) {
        return res.end("Couldn't look up SPI data, error for session " + ViewerUtils.safeStr(req.params.id) + ' Error: ' + err);
      }

      session = session._source;

      session.id = req.params.id;

      sortFields(session);

      let hidePackets = (session.fileId === undefined || session.fileId.length === 0) ? 'true' : 'false';
      ViewerUtils.fixFields(session, () => {
        pug.render(internals.sessionDetailNew, {
          filename: 'sessionDetail',
          cache: internals.isProduction,
          compileDebug: !internals.isProduction,
          user: req.user,
          session: session,
          Db: Db,
          query: req.query,
          basedir: '/',
          hidePackets: hidePackets,
          reqFields: Config.headers('headers-http-request'),
          resFields: Config.headers('headers-http-response'),
          emailFields: Config.headers('headers-email')
        }, function (err, data) {
          if (err) {
            console.trace('ERROR - fixFields - ', err);
            return req.next(err);
          }
          if (Config.debug > 1) {
            console.log('Detail Rendering', data.replace(/>/g, '>\n'));
          }
          res.send(data);
        });
      });
    });
  };

  /**
   * GET
   *
   * Gets packets for a session
   * @name /:nodeName/session/:id/packets
   * @returns {object} Sends the response to the client
   */
  module.getPackets = (req, res) => {
    module.isLocalView(req.params.nodeName, () => {
      ViewerUtils.noCache(req, res);
      req.packetsOnly = true;
      localSessionDetail(req, res);
    },
    function () {
      return module.proxyRequest(req, res);
    });
  };

  return module;
};
