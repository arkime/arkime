'use strict';

const async = require('async');
const contentDisposition = require('content-disposition');
const fs = require('fs');
const http = require('http');
const path = require('path');
const PNG = require('pngjs').PNG;
const pug = require('pug');
const util = require('util');
const decode = require('./decode.js');

module.exports = (Config, Db, internals, molochparser, Pcap, version, ViewerUtils) => {
  const module = {};

  // --------------------------------------------------------------------------
  // INTERNAL HELPERS
  // --------------------------------------------------------------------------
  function sessionsListFromQuery (req, res, fields, cb) {
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
      const options = ViewerUtils.addCluster(req.query.cluster);
      Db.searchPrimary(indices, 'session', query, options, (err, result) => {
        if (err || result.error) {
          console.log('ERROR - Could not fetch list of sessions.  Err: ', err, ' Result: ', result, 'query:', query);
          return res.send('Could not fetch list of sessions.  Err: ' + err + ' Result: ' + result);
        }
        const list = result.hits.hits;
        if (req.query.segments && req.query.segments.match(/^(time|all)$/)) {
          sessionsListAddSegments(req, indices, query, list, (err, list) => {
            cb(err, list);
          });
        } else {
          cb(err, list);
        }
      });
    });
  };

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
        const obj = {};
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

      info.order.split(',').forEach((item) => {
        const parts = item.split(':');
        const field = parts[0];

        const obj = {};
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

    let i = 0;
    for (const ilen = parseInt(info.iSortingCols, 10); i < ilen; i++) {
      if (!info['iSortCol_' + i] || !info['sSortDir_' + i] || !info['mDataProp_' + info['iSortCol_' + i]]) {
        continue;
      }

      const obj = {};
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
    const reqQuery = queryOverride || req.query;

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
          for (const viewName in sharedUser.views) {
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
      list = list.sort((a, b) => { return a.fields.lastPacket - b.fields.lastPacket; });
    } else if (list.length > 0 && list[0]._source) {
      list = list.sort((a, b) => { return a._source.lastPacket - b._source.lastPacket; });
    }

    const fieldObjects = Config.getDBFieldsMap();

    if (fields) {
      const columnHeaders = [];
      for (let i = 0; i < fields.length; ++i) {
        if (fieldObjects[fields[i]] !== undefined) {
          columnHeaders.push(fieldObjects[fields[i]].friendlyName);
        }
      }
      res.write(columnHeaders.join(', '));
      res.write('\r\n');
    }

    for (let j = 0; j < list.length; j++) {
      const sessionData = ViewerUtils.flattenFields(list[j]._source || list[j].fields);
      sessionData._id = list[j]._id;

      if (!fields) { continue; }

      const values = [];
      for (let k = 0; k < fields.length; ++k) {
        let value = sessionData[fields[k]];
        if (fields[k] === 'ipProtocol' && value) {
          value = Pcap.protocol2Name(value);
        }

        if (Array.isArray(value)) {
          const singleValue = '"' + value.join(', ') + '"';
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
    const processedRo = {};

    // Index all the ids we have, so we don't include them again
    const haveIds = {};
    list.forEach((item) => {
      haveIds[item._id] = true;
    });

    delete query.aggregations;

    // Do a ro search on each item
    let writes = 0;
    async.eachLimit(list, 10, (item, nextCb) => {
      const fields = item._source || item.fields;
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

      const options = ViewerUtils.addCluster(req.query.cluster);
      query.query.bool.filter.push({ term: { rootId: fields.rootId } });
      Db.searchPrimary(indices, 'session', query, options, (err, result) => {
        if (err || result === undefined || result.hits === undefined || result.hits.hits === undefined) {
          console.log('ERROR fetching matching sessions', err, result);
          return nextCb(null);
        }
        result.hits.hits.forEach((item) => {
          if (!haveIds[item._id]) {
            haveIds[item._id] = true;
            list.push(item);
          }
        });
        return nextCb(null);
      });
      query.query.bool.filter.pop();
    }, (err) => {
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

  function reqGetRawBody (req, cb) {
    module.processSessionIdAndDecode(req.params.id, 10000, (err, session, incoming) => {
      if (err) {
        return cb(err);
      }

      if (incoming.length === 0) {
        return cb(null, null);
      }

      const options = {
        id: session.id,
        nodeName: req.params.nodeName,
        order: [],
        'ITEM-HTTP': {
          order: []
        },
        'ITEM-SMTP': {
          order: ['BODY-UNBASE64']
        },
        'ITEM-CB': {
        },
        'ITEM-RAWBODY': {
          bodyNumber: +req.params.bodyNum
        }
      };

      if (req.query.needgzip) {
        options['ITEM-HTTP'].order.push('BODY-UNCOMPRESS');
        options['ITEM-SMTP'].order.push('BODY-UNCOMPRESS');
      }

      options.order.push('ITEM-HTTP');
      options.order.push('ITEM-SMTP');

      options.order.push('ITEM-RAWBODY');
      options.order.push('ITEM-CB');
      options['ITEM-CB'].cb = (err, items) => {
        if (err) {
          return cb(err);
        }
        if (items === undefined || items.length === 0) {
          return cb('No match');
        }
        cb(err, items[0].data);
      };

      decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, incoming));
    });
  };

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
      }, (err, data) => {
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
    const numPackets = req.query.packets || 200;
    if (incoming.length > numPackets) {
      incoming.length = numPackets;
    }

    if (incoming.length === 0) {
      return localSessionDetailReturnFull(req, res, session, []);
    }

    const options = {
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

    const decodeOptions = JSON.parse(req.query.decode || '{}');
    for (const key in decodeOptions) {
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
    options['ITEM-CB'].cb = (err, outgoing) => {
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

    const packets = [];
    module.processSessionId(req.params.id, !req.packetsOnly, null, (pcap, buffer, cb, i) => {
      let obj = {};
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
    }, (err, session) => {
      if (err) {
        return res.end('Problem loading packets for ' + ViewerUtils.safeStr(req.params.id) + ' Error: ' + err);
      }
      session.id = req.params.id;
      sortFields(session);

      if (req.query.showFrames && packets.length !== 0) {
        Pcap.packetFlow(session, packets, +req.query.packets || 200, (err, results, sourceKey, destinationKey) => {
          session._err = err;
          session.sourceKey = sourceKey;
          session.destinationKey = destinationKey;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets.length === 0) {
        session._err = 'No pcap data found';
        localSessionDetailReturn(req, res, session, []);
      } else if (packets[0].ether !== undefined && packets[0].ether.data !== undefined) {
        Pcap.reassemble_generic_ether(packets, +req.query.packets || 200, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip === undefined) {
        session._err = "Couldn't decode pcap file, check viewer log";
        localSessionDetailReturn(req, res, session, []);
      } else if (packets[0].ip.p === 1) {
        Pcap.reassemble_icmp(packets, +req.query.packets || 200, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 6) {
        const key = session.srcIp;
        Pcap.reassemble_tcp(packets, +req.query.packets || 200, key + ':' + session.srcPort, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 17) {
        Pcap.reassemble_udp(packets, +req.query.packets || 200, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 132) {
        Pcap.reassemble_sctp(packets, +req.query.packets || 200, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 50) {
        Pcap.reassemble_esp(packets, +req.query.packets || 200, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.p === 58) {
        Pcap.reassemble_icmp(packets, +req.query.packets || 200, (err, results) => {
          session._err = err;
          localSessionDetailReturn(req, res, session, results || []);
        });
      } else if (packets[0].ip.data !== undefined) {
        Pcap.reassemble_generic_ip(packets, +req.query.packets || 200, (err, results) => {
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
    function processFile (pcap, pos, i, nextCb) {
      pcap.ref();
      pcap.readPacket(pos, (packet) => {
        switch (packet) {
        case null:
          const msg = util.format(session._id, 'in file', pcap.filename, "couldn't read packet at", pos, 'packet #', i, 'of', fields.packetPos.length);
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

    const fields = session._source || session.fields;

    let fileNum;
    let itemPos = 0;
    async.eachLimit(fields.packetPos, limit || 1, (pos, nextCb) => {
      if (pos < 0) {
        fileNum = pos * -1;
        return nextCb(null);
      }

      // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
      const opcap = Pcap.get(fields.node + ':' + fileNum);
      if (!opcap.isOpen()) {
        Db.fileIdToFile(fields.node, fileNum, (file) => {
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

          const ipcap = Pcap.get(fields.node + ':' + file.num);

          try {
            ipcap.open(file.name, file);
          } catch (err) {
            console.log("ERROR - Couldn't open file ", err);
            if (err.code === 'EACCES') {
              // Find all the directories to check
              const checks = [];
              let dir = path.resolve(file.name);
              while ((dir = path.dirname(dir)) !== '/') {
                checks.push(dir);
              }

              // Check them in reverse order, smallest to largest
              let i = checks.length - 1;
              for (i; i >= 0; i--) {
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
    }, (pcapErr, results) => {
      endCb(pcapErr, fields);
    });
  }

  function sessionsPcapList (req, res, list, pcapWriter, extension) {
    if (list.length > 0 && list[0].fields) {
      list = list.sort((a, b) => {
        return a.fields.lastPacket - b.fields.lastPacket;
      });
    } else if (list.length > 0 && list[0]._source) {
      list = list.sort((a, b) => {
        return a._source.lastPacket - b._source.lastPacket;
      });
    }

    const options = { writeHeader: true };

    async.eachLimit(list, 10, (item, nextCb) => {
      const fields = item._source || item.fields;
      module.isLocalView(fields.node, () => {
        // Get from our DISK
        pcapWriter(res, Db.session2Sid(item), options, nextCb);
      }, () => {
        // Get from remote DISK
        ViewerUtils.getViewUrl(fields.node, (err, viewUrl, client) => {
          let buffer = Buffer.alloc(Math.min(16200000, fields.totPackets * 20 + fields.totBytes));
          let bufpos = 0;

          const path = Config.basePath(fields.node) + fields.node + '/' + extension + '/' + Db.session2Sid(item) + '.' + extension;
          const url = new URL(path, viewUrl);
          const options = {
            agent: client === http ? internals.httpAgent : internals.httpsAgent
          };

          ViewerUtils.addAuth(options, req.user, fields.node, path);
          ViewerUtils.addCaTrust(options, fields.node);

          const preq = client.request(url, options, (pres) => {
            pres.on('data', (chunk) => {
              if (bufpos + chunk.length > buffer.length) {
                const tmp = Buffer.alloc(buffer.length + chunk.length * 10);
                buffer.copy(tmp, 0, 0, bufpos);
                buffer = tmp;
              }
              chunk.copy(buffer, bufpos);
              bufpos += chunk.length;
            });
            pres.on('end', () => {
              if (bufpos < 24) {
              } else if (options.writeHeader) {
                options.writeHeader = false;
                res.write(buffer.slice(0, bufpos));
              } else {
                res.write(buffer.slice(24, bufpos));
              }
              setImmediate(nextCb);
            });
          });
          preq.on('error', (e) => {
            console.log("ERROR - Couldn't proxy pcap request=", url, '\nerror=', e);
            nextCb(null);
          });
          preq.end();
        });
      });
    }, (err) => {
      res.end();
    });
  }

  function localGetItemByHash (nodeName, sessionID, hash, cb) {
    module.processSessionIdAndDecode(sessionID, 10000, (err, session, incoming) => {
      if (err) {
        return cb(err);
      }

      if (incoming.length === 0) {
        return cb(null, null);
      }

      const options = {
        id: sessionID,
        nodeName: nodeName,
        order: [],
        'ITEM-HTTP': {
          order: []
        },
        'ITEM-SMTP': {
          order: ['BODY-UNBASE64']
        },
        'ITEM-HASH': {
          hash: hash
        },
        'ITEM-CB': {
        }
      };

      options.order.push('ITEM-HTTP');
      options.order.push('ITEM-SMTP');
      options.order.push('ITEM-HASH');
      options.order.push('ITEM-CB');
      options['ITEM-CB'].cb = (err, items) => {
        if (err) {
          return cb(err, null);
        }
        if (items === undefined || items.length === 0) {
          return cb('No match', null);
        }
        return cb(err, items[0]);
      };

      decode.createPipeline(options, options.order, new decode.Pcap2ItemStream(options, incoming));
    });
  }

  function sendSessionsList (req, res, list) {
    if (!list) { return res.molochError(200, 'Missing list of sessions'); }

    const saveId = Config.nodeName() + '-' + new Date().getTime().toString(36);

    async.eachLimit(list, 10, (item, nextCb) => {
      const fields = item._source || item.fields;
      const sid = Db.session2Sid(item);
      module.isLocalView(fields.node, () => {
        const options = {
          user: req.user,
          cluster: req.body.cluster,
          id: sid,
          saveId: saveId,
          tags: req.body.tags,
          nodeName: fields.node
        };
        // Get from our DISK
        internals.sendSessionQueue.push(options, nextCb);
      }, () => {
        let path = `api/sessions/${sid}/${fields.node}/send?saveId=${saveId}&cluster=${req.body.cluster}`;
        if (req.body.tags) {
          path += `&tags=${req.body.tags}`;
        }

        ViewerUtils.makeRequest(fields.node, path, req.user, (err, response) => {
          setImmediate(nextCb);
        });
      });
    }, (err) => {
      return res.end(JSON.stringify({
        success: true,
        text: 'Sending of ' + list.length + ' sessions complete'
      }));
    });
  }

  function sessionsPcap (req, res, pcapWriter, extension) {
    ViewerUtils.noCache(req, res, 'application/vnd.tcpdump.pcap');

    const fields = ['lastPacket', 'node', 'totBytes', 'totPackets', 'rootId'];

    if (req.query.ids) {
      const ids = ViewerUtils.queryValueToArray(req.query.ids);

      module.sessionsListFromIds(req, ids, fields, (err, list) => {
        sessionsPcapList(req, res, list, pcapWriter, extension);
      });
    } else {
      sessionsListFromQuery(req, res, fields, (err, list) => {
        sessionsPcapList(req, res, list, pcapWriter, extension);
      });
    }
  }

  function writePcap (res, id, options, doneCb) {
    let b = Buffer.alloc(0xfffe);
    let nextPacket = 0;
    let boffset = 0;
    const packets = {};

    module.processSessionId(id, false, (pcap, buffer) => {
      if (options.writeHeader) {
        res.write(buffer);
        options.writeHeader = false;
      }
    }, (pcap, buffer, cb, i) => {
      // Save this packet in its spot
      packets[i] = buffer;

      // Send any packets we have in order
      while (packets[nextPacket]) {
        buffer = packets[nextPacket];
        delete packets[nextPacket];
        nextPacket++;

        if (boffset + buffer.length > b.length) {
          res.write(b.slice(0, boffset));
          boffset = 0;
          b = Buffer.alloc(0xfffe);
        }
        buffer.copy(b, boffset, 0, buffer.length);
        boffset += buffer.length;
      }
      cb(null);
    }, (err, session) => {
      if (err) {
        console.trace('writePcap', err);
        return doneCb(err);
      }
      res.write(b.slice(0, boffset));
      doneCb(err);
    }, undefined, 10);
  }

  function writePcapNg (res, id, options, doneCb) {
    let b = Buffer.alloc(0xfffe);
    let boffset = 0;

    module.processSessionId(id, true, (pcap, buffer) => {
      if (options.writeHeader) {
        res.write(pcap.getHeaderNg());
        options.writeHeader = false;
      }
    }, (pcap, buffer, cb) => {
      if (boffset + buffer.length + 20 > b.length) {
        res.write(b.slice(0, boffset));
        boffset = 0;
        b = Buffer.alloc(0xfffe);
      }

      /* Need to write the ng block, and conver the old timestamp */
      b.writeUInt32LE(0x00000006, boffset); // Block Type
      const len = ((buffer.length + 20 + 3) >> 2) << 2;
      b.writeUInt32LE(len, boffset + 4); // Block Len 1
      b.writeUInt32LE(0, boffset + 8); // Interface Id

      // js has 53 bit numbers, this will over flow on Jun 05 2255
      const time = buffer.readUInt32LE(0) * 1000000 + buffer.readUInt32LE(4);
      b.writeUInt32LE(Math.floor(time / 0x100000000), boffset + 12); // Block Len 1
      b.writeUInt32LE(time % 0x100000000, boffset + 16); // Interface Id

      buffer.copy(b, boffset + 20, 8, buffer.length - 8); // cap_len, packet_len
      b.fill(0, boffset + 12 + buffer.length, boffset + 12 + buffer.length + (4 - (buffer.length % 4)) % 4); // padding
      boffset += len - 8;

      b.writeUInt32LE(0, boffset); // Options
      b.writeUInt32LE(len, boffset + 4); // Block Len 2
      boffset += 8;

      cb(null);
    }, (err, session) => {
      if (err) {
        console.log('writePcapNg', err);
        return;
      }
      res.write(b.slice(0, boffset));

      session.version = version.version;
      delete session.packetPos;
      const json = JSON.stringify(session);

      const len = ((json.length + 20 + 3) >> 2) << 2;
      b = Buffer.alloc(len);

      b.writeUInt32LE(0x80808080, 0); // Block Type
      b.writeUInt32LE(len, 4); // Block Len 1
      b.write('MOWL', 8); // Magic
      b.writeUInt32LE(json.length, 12); // Block Len 1
      b.write(json, 16); // Magic
      b.fill(0, 16 + json.length, 16 + json.length + (4 - (json.length % 4)) % 4); // padding
      b.writeUInt32LE(len, len - 4); // Block Len 2
      res.write(b);

      doneCb(err);
    });
  }

  function pcapScrub (req, res, sid, whatToRemove, endCb) {
    if (pcapScrub.scrubbingBuffers === undefined) {
      pcapScrub.scrubbingBuffers = [Buffer.alloc(5000), Buffer.alloc(5000), Buffer.alloc(5000)];
      pcapScrub.scrubbingBuffers[0].fill(0);
      pcapScrub.scrubbingBuffers[1].fill(1);
      const str = 'Scrubbed! Hoot! ';
      for (let i = 0; i < 5000;) {
        i += pcapScrub.scrubbingBuffers[2].write(str, i);
      }
    }

    function processFile (pcap, pos, i, nextCb) {
      pcap.ref();
      pcap.readPacket(pos, (packet) => {
        pcap.unref();
        if (packet) {
          if (packet.length > 16) {
            try {
              const obj = {};
              pcap.decode(packet, obj);
              pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[0], whatToRemove === 'all');
              pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[1], whatToRemove === 'all');
              pcap.scrubPacket(obj, pos, pcapScrub.scrubbingBuffers[2], whatToRemove === 'all');
            } catch (e) {
              console.log(`Couldn't scrub packet at ${pos} -`, e);
            }
            return nextCb(null);
          } else {
            console.log(`Couldn't scrub packet at ${pos}`);
            return nextCb(null);
          }
        }
      });
    }

    Db.getSession(sid, { _source: 'node,ipProtocol,packetPos' }, (err, session) => {
      let fileNum;
      let itemPos = 0;
      const fields = session._source || session.fields;

      if (whatToRemove === 'spi') { // just removing es data for session
        Db.deleteDocument(session._index, 'session', session._id, (err, data) => {
          return endCb(err, fields);
        });
      } else { // scrub the pcap
        async.eachLimit(fields.packetPos, 10, (pos, nextCb) => {
          if (pos < 0) {
            fileNum = pos * -1;
            return nextCb(null);
          }

          // Get the pcap file for this node a filenum, if it isn't opened then do the filename lookup and open it
          const opcap = Pcap.get(`write${fields.node}:${fileNum}`);
          if (!opcap.isOpen()) {
            Db.fileIdToFile(fields.node, fileNum, (file) => {
              if (!file) {
                console.log(`WARNING - Only have SPI data, PCAP file no longer available.  Couldn't look up in file table ${fields.node}-${fileNum}`);
                return nextCb(`Only have SPI data, PCAP file no longer available for ${fields.node}-${fileNum}`);
              }

              const ipcap = Pcap.get(`write${fields.node}:${file.num}`);

              try {
                ipcap.openReadWrite(file.name, file);
              } catch (err) {
                const errorMsg = `Couldn't open file for writing: ${err}`;
                console.log(`Error - ${errorMsg}`);
                return nextCb(errorMsg);
              }
              processFile(ipcap, pos, itemPos++, nextCb);
            });
          } else {
            processFile(opcap, pos, itemPos++, nextCb);
          }
        }, (pcapErr, results) => {
          if (whatToRemove === 'all') { // also remove the session data
            Db.deleteDocument(session._index, 'session', session._id, (err, data) => {
              return endCb(pcapErr, fields);
            });
          } else { // just set who/when scrubbed the pcap
            // Do the ES update
            const document = {
              doc: {
                scrubby: req.user.userId || '-',
                scrubat: new Date().getTime()
              }
            };
            Db.updateSession(session._index, session._id, document, (err, data) => {
              return endCb(pcapErr, fields);
            });
          }
        });
      }
    });
  }

  function scrubList (req, res, whatToRemove, list) {
    if (!list) { return res.molochError(200, 'Missing list of sessions'); }

    async.eachLimit(list, 10, (item, nextCb) => {
      const fields = item._source || item.fields;

      module.isLocalView(fields.node, () => {
        // Get from our DISK
        pcapScrub(req, res, Db.session2Sid(item), whatToRemove, nextCb);
      }, () => {
        // Get from remote DISK
        const path = `${fields.node}/delete/${whatToRemove}/${Db.session2Sid(item)}`;
        ViewerUtils.makeRequest(fields.node, path, req.user, (err, response) => {
          setImmediate(nextCb);
        });
      });
    }, (err) => {
      let text;
      if (whatToRemove === 'all') {
        text = `Deletion PCAP and SPI of ${list.length} sessions complete. Give Elasticsearch 60 seconds to complete SPI deletion.`;
      } else if (whatToRemove === 'spi') {
        text = `Deletion SPI of ${list.length} sessions complete. Give Elasticsearch 60 seconds to complete SPI deletion.`;
      } else {
        text = `Scrubbing PCAP of ${list.length} sessions complete`;
      }
      return res.end(JSON.stringify({ success: true, text: text }));
    });
  }

  // --------------------------------------------------------------------------
  // EXPOSED HELPERS
  // --------------------------------------------------------------------------
  module.processSessionId = (id, fullSession, headerCb, packetCb, endCb, maxPackets, limit) => {
    let options;
    if (!fullSession) {
      options = { _source: 'node,totPackets,packetPos,srcIp,srcPort,ipProtocol,packetLen' };
    }

    Db.getSession(id, options, (err, session) => {
      if (err || !session.found) {
        console.log('session get error', err, session);
        return endCb('Session not found', null);
      }

      const fields = session._source || session.fields;

      if (maxPackets && fields.packetPos.length > maxPackets) {
        fields.packetPos.length = maxPackets;
      }

      /* Go through the list of prefetch the id to file name if we are running in parallel to
       * reduce the number of elasticsearch queries and problems
       */
      let outstanding = 0;
      let i;
      let ilen;

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
        const pcapWriteMethod = Config.getFull(fields.node, 'pcapWriteMethod');
        let psid = processSessionIdDisk;
        const writer = internals.writers[pcapWriteMethod];
        if (writer && writer.processSessionId) {
          psid = writer.processSessionId;
        }

        psid(session, headerCb, packetCb, (err, fields) => {
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

  /**
   * The query params to build an Elasticsearch sessions query.
   *
   * For long expressions use POST for client requests to the server.
   * When using POST the request body and request query are merged. Any duplicate parameters use the request body parameter.
   * @typedef SessionsQuery
   * @type {object}
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
     Default is ipProtocol, rootId, totDataBytes, srcDataBytes, dstDataBytes, firstPacket, lastPacket, srcIp, srcPort, dstIp, dstPort, totPackets, srcPackets, dstPackets, totBytes, srcBytes, dstBytes, node, http.uri, srcGEO, dstGEO, email.subject, email.src, email.dst, email.filename, dns.host, cert, irc.channel
   * @param {string} bounding=last - Query sessions based on different aspects of a session's time. Options include:
     'first' - First Packet: the timestamp of the first packet received for the session.
     'last' - Last Packet: The timestamp of the last packet received for the session.
     'both' - Bounded: Both the first and last packet timestamps for the session must be inside the time window.
     'either' - Session Overlaps: The timestamp of the first packet must be before the end of the time window AND the timestamp of the last packet must be after the start of the time window.
     'database' - Database: The timestamp the session was written to the database. This can be up to several minutes AFTER the last packet was received.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   */

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

    // queryOverride can supercede req.query if specified
    const reqQuery = queryOverride || req.query;

    // determineQueryTimes calculates startTime, stopTime, and interval from reqQuery
    const startAndStopParams = ViewerUtils.determineQueryTimes(reqQuery);
    if (startAndStopParams[0] !== undefined) {
      reqQuery.startTime = startAndStopParams[0];
    }
    if (startAndStopParams[1] !== undefined) {
      reqQuery.stopTime = startAndStopParams[1];
    }

    const interval = startAndStopParams[2];

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

    const limit = Math.min(2000000, +reqQuery.length || 100);

    const query = {
      from: reqQuery.start || 0,
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
      if (reqQuery.map === 'true' || reqQuery.map) {
        query.aggregations = {
          mapG1: { terms: { field: 'srcGEO', size: 1000, min_doc_count: 1 } },
          mapG2: { terms: { field: 'dstGEO', size: 1000, min_doc_count: 1 } },
          mapG3: { terms: { field: 'http.xffGEO', size: 1000, min_doc_count: 1 } }
        };
      }

      query.aggregations.dbHisto = { aggregations: {} };

      const filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;
      for (let i = 0; i < filters.length; i++) {
        const filter = filters[i];

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

    Db.getShortcutsCache(req.user.userId, (lerr, shortcuts) => {
      let err = null;
      molochparser.parser.yy = {
        views: req.user.views,
        fieldsMap: Config.getFieldsMap(),
        dbFieldsMap: Config.getDBFieldsMap(),
        prefix: internals.prefix,
        emailSearch: req.user.emailSearch === true,
        shortcuts: shortcuts || {},
        shortcutTypeMap: internals.shortcutTypeMap
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
    });
  };

  module.sessionsListFromIds = (req, ids, fields, cb) => {
    let processSegments = false;
    if (req && ((req.query.segments && req.query.segments.match(/^(time|all)$/)) || (req.query.segments && req.query.segments.match(/^(time|all)$/)))) {
      if (fields.indexOf('rootId') === -1) { fields.push('rootId'); }
      processSegments = true;
    }

    const list = [];
    const nonArrayFields = ['ipProtocol', 'firstPacket', 'lastPacket', 'srcIp', 'srcPort', 'srcGEO', 'dstIp', 'dstPort', 'dstGEO', 'totBytes', 'totDataBytes', 'totPackets', 'node', 'rootId', 'http.xffGEO'];
    const fixFields = nonArrayFields.filter((x) => { return fields.indexOf(x) !== -1; });

    const options = ViewerUtils.addCluster(req ? req.query.cluster : undefined, { _source: fields.join(',') });
    async.eachLimit(ids, 10, (id, nextCb) => {
      Db.getSession(id, options, (err, session) => {
        if (err) {
          return nextCb(null);
        }

        for (let i = 0; i < fixFields.length; i++) {
          const field = fixFields[i];
          if (session._source[field] && Array.isArray(session._source[field])) {
            session._source[field] = session._source[field][0];
          }
        }

        list.push(session);
        nextCb(null);
      });
    }, (err) => {
      if (processSegments) {
        module.buildSessionQuery(req, (err, query, indices) => {
          query._source = fields;
          sessionsListAddSegments(req, indices, query, list, (err, list) => {
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

    const pcapWriteMethod = Config.getFull(node, 'pcapWriteMethod');
    const writer = internals.writers[pcapWriteMethod];
    if (writer && writer.localNode === false) {
      if (Config.debug > 1) {
        console.log(`DEBUG: node:${node} is local view because of writer`);
      }
      return yesCb();
    }
    return Db.isLocalView(node, yesCb, noCb);
  };

  module.proxyRequest = (req, res, errCb) => {
    ViewerUtils.noCache(req, res);

    ViewerUtils.getViewUrl(req.params.nodeName, (err, viewUrl, client) => {
      if (err) {
        if (errCb) {
          return errCb(err);
        }
        console.log('ERROR - getViewUrl - node:', req.params.nodeName, 'err:', err);
        return res.send(`Can't find view url for '${ViewerUtils.safeStr(req.params.nodeName)}' check viewer logs on '${Config.hostName()}'`);
      }

      const url = new URL(req.url, viewUrl);
      const options = {
        timeout: 20 * 60 * 1000,
        agent: client === http ? internals.httpAgent : internals.httpsAgent
      };

      ViewerUtils.addAuth(options, req.user, req.params.nodeName, req.url);
      ViewerUtils.addCaTrust(options, req.params.nodeName);

      const preq = client.request(url, options, (pres) => {
        if (pres.headers['content-type']) {
          res.setHeader('content-type', pres.headers['content-type']);
        }
        if (pres.headers['content-disposition']) {
          res.setHeader('content-disposition', pres.headers['content-disposition']);
        }
        pres.on('data', (chunk) => {
          res.write(chunk);
        });
        pres.on('end', () => {
          res.end();
        });
      });

      preq.on('error', (e) => {
        if (errCb) {
          return errCb(e);
        }
        console.log("ERROR - Couldn't proxy request=", url, '\nerror=', e, 'You might want to run viewer with two --debug for more info');
        res.send(`Error talking to node '${ViewerUtils.safeStr(req.params.nodeName)}' using host '${url.host}' check viewer logs on '${Config.hostName()}'`);
      });
      preq.end();
    });
  };

  module.addTagsList = (allTagNames, sessionList, doneCb) => {
    if (!sessionList.length) {
      console.log(`No sessions to add tags (${allTagNames}) to`);
      return doneCb(null);
    }

    async.eachLimit(sessionList, 10, (session, nextCb) => {
      if (!session._source && !session.fields) {
        console.log('No Fields', session);
        return nextCb(null);
      }

      const cluster = (Config.get('multiES', false) && session.cluster) ? session.cluster : undefined;

      Db.addTagsToSession(session._index, session._id, allTagNames, cluster, (err, data) => {
        if (err) { console.log('addTagsList error', session, err, data); }
        nextCb(null);
      });
    }, doneCb);
  };

  module.removeTagsList = (res, allTagNames, sessionList) => {
    if (!sessionList.length) {
      return res.molochError(200, 'No sessions to remove tags from');
    }

    async.eachLimit(sessionList, 10, (session, nextCb) => {
      if (!session._source && !session.fields) {
        console.log('No Fields', session);
        return nextCb(null);
      }

      const cluster = (Config.get('multiES', false) && session.cluster) ? session.cluster : undefined;

      Db.removeTagsFromSession(session._index, session._id, allTagNames, cluster, (err, data) => {
        if (err) { console.log('removeTagsList error', session, err, data); }
        nextCb(null);
      });
    }, (err) => {
      return res.send(JSON.stringify({
        success: true,
        text: 'Tags removed successfully'
      }));
    });
  };

  module.processSessionIdAndDecode = (id, numPackets, doneCb) => {
    let packets = [];
    module.processSessionId(id, true, null, (pcap, buffer, cb, i) => {
      let obj = {};
      if (buffer.length > 16) {
        pcap.decode(buffer, obj);
      } else {
        obj = { ip: { p: '' } };
      }
      packets[i] = obj;
      cb(null);
    }, (err, session) => {
      if (err) {
        console.log('ERROR - processSessionIdAndDecode', err);
        return doneCb(err);
      }
      packets = packets.filter(Boolean);
      if (packets.length === 0) {
        return doneCb(null, session, []);
      } else if (packets[0].ip === undefined) {
        return doneCb(null, session, []);
      } else if (packets[0].ip.p === 1) {
        Pcap.reassemble_icmp(packets, numPackets, (err, results) => {
          return doneCb(err, session, results);
        });
      } else if (packets[0].ip.p === 6) {
        const key = session.srcIp;
        Pcap.reassemble_tcp(packets, numPackets, key + ':' + session.srcPort, (err, results) => {
          return doneCb(err, session, results);
        });
      } else if (packets[0].ip.p === 17) {
        Pcap.reassemble_udp(packets, numPackets, (err, results) => {
          return doneCb(err, session, results);
        });
      } else if (packets[0].ip.p === 132) {
        Pcap.reassemble_sctp(packets, numPackets, (err, results) => {
          return doneCb(err, session, results);
        });
      } else {
        return doneCb(null, session, []);
      }
    }, numPackets, 10);
  };

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * POST/GET - /api/buildquery
   *
   * Builds an elasticsearch session query and returns the query and the elasticsearch indices to the client.
   * @name /buildquery
   * @param {SessionsQuery} query - The request query to filter sessions
   * @returns {object} query - The elasticsearch query
   * @returns {object} indices - The elasticsearch indices that contain sessions in this query
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

      res.send({ esquery: query, indices: indices });
    });
  };

  /**
   * POST/GET - /api/sessions
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them to the client.
   * @name /sessions
   * @param {SessionsQuery} query - The request query to filter sessions
   * @returns {object} map - The data to populate the sessions map
   * @returns {object} graph - The data to populate the sessions timeline graph
   * @returns {array} data - The list of sessions with the requested fields
   * @returns {number} recordsTotal - The total number of files Arkime knows about
   * @returns {number} recordsFiltered - The number of files returned in this result
   * @returns {ESHealth} health - The elasticsearch cluster health status and info
   */
  module.getSessions = (req, res) => {
    let map = {};
    let graph = {};

    let options = {};
    if (req.query.cancelId) {
      options.cancelId = `${req.user.userId}::${req.query.cancelId}`;
    }
    options = ViewerUtils.addCluster(req.query.cluster, options);

    const response = {
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
        Db.numberOfDocuments('sessions2-*', options.cluster ? { cluster: options.cluster } : {}),
        Db.healthCachePromise()
      ]).then(([sessions, total, health]) => {
        if (Config.debug) {
          console.log('sessions.json result', util.inspect(sessions, false, 50));
        }

        if (sessions.error) { throw sessions.err; }

        map = ViewerUtils.mapMerge(sessions.aggregations);
        graph = ViewerUtils.graphMerge(req, query, sessions.aggregations);

        const results = { total: sessions.hits.total, results: [] };
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
            ViewerUtils.fixFields(fields, () => {
              results.results.push(fields);
              return hitCb();
            });
          }
        }, () => {
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
   * POST/GET - /api/sessions/csv OR /api/sessions.csv
   *
   * Builds an elasticsearch session query. Gets a list of sessions and returns them as CSV to the client.
   * @name /sessions/csv
   * @param {SessionsQuery} query - The request query to filter sessions
   * @returns {csv} csv - The csv with the sessions requested
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
      sessionsListFromQuery(req, res, fields, (err, list) => {
        csvListWriter(req, res, list, reqFields);
      });
    }
  };

  /**
   * POST/GET - /api/spiview
   *
   * Builds an elasticsearch session query. Gets a list of field values with counts and returns them to the client.
   * @name /spiview
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} spi - Comma separated list of db fields to return. Optionally can be followed by :{count} to specify the number of values returned for the field (defaults to 100).
   * @returns {object} map - The data to populate the sessions map
   * @returns {object} graph - The data to populate the sessions timeline graph
   * @returns {object} spi - The list of spi fields with values and counts
   * @returns {object} protocols - The list of protocols with counts
   * @returns {number} recordsTotal - The total number of files Arkime knows about
   * @returns {number} recordsFiltered - The number of files returned in this result
   * @returns {ESHealth} health - The elasticsearch cluster health status and info
   */
  module.getSPIView = (req, res) => {
    if (req.query.spi === undefined) {
      return res.send({ spi: {}, recordsTotal: 0, recordsFiltered: 0 });
    }

    const spiDataMaxIndices = +Config.get('spiDataMaxIndices', 4);

    if (parseInt(req.query.date) === -1 && spiDataMaxIndices !== -1) {
      return res.send({ spi: {}, bsqErr: "'All' date range not allowed for spiview query" });
    }

    const response = {
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

      ViewerUtils.queryValueToArray(req.query.spi).forEach((item) => {
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

      let map;
      let graph;

      const indicesa = indices.split(',');
      if (spiDataMaxIndices !== -1 && indicesa.length > spiDataMaxIndices) {
        bsqErr = 'To save ES from blowing up, reducing number of spi data indices searched from ' + indicesa.length + ' to ' + spiDataMaxIndices + '.  This can be increased by setting spiDataMaxIndices in the config file.  Indices being searched: ';
        indices = indicesa.slice(-spiDataMaxIndices).join(',');
        bsqErr += indices;
      }

      let protocols;
      let recordsFiltered = 0;
      const options = ViewerUtils.addCluster(req.query.cluster);

      Promise.all([Db.searchPrimary(indices, 'session', query, options),
        Db.numberOfDocuments('sessions2-*', options.cluster ? { cluster: options.cluster } : {}),
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
          for (const spi in query.aggregations) {
            sessions.aggregations[spi] = { sum_other_doc_count: 0, buckets: [] };
          }
        }

        if (sessions.aggregations.ipProtocol) {
          sessions.aggregations.ipProtocol.buckets.forEach((item) => {
            item.key = Pcap.protocol2Name(item.key);
          });
        }

        if (parseInt(req.query.facets) === 1) {
          protocols = {};
          map = ViewerUtils.mapMerge(sessions.aggregations);
          graph = ViewerUtils.graphMerge(req, query, sessions.aggregations);
          sessions.aggregations.protocols.buckets.forEach((item) => {
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
        async.each(sessions.aggregations.fileand.buckets, (nobucket, cb) => {
          sodc += nobucket.fileId.sum_other_doc_count;
          async.each(nobucket.fileId.buckets, (fsitem, cb) => {
            Db.fileIdToFile(nobucket.key, fsitem.key, (file) => {
              if (file && file.name) {
                nresults.push({ key: file.name, doc_count: fsitem.doc_count });
              }
              cb();
            });
          }, () => {
            cb();
          });
        }, () => {
          nresults = nresults.sort((a, b) => {
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
   * POST/GET - /api/spigraph
   *
   * Builds an elasticsearch session query. Gets a list of values for a field with counts and graph data and returns them to the client.
   * @name /spigraph
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} field=node - The database field to get data for. Defaults to "node".
   * @returns {object} map - The data to populate the main/aggregate spigraph sessions map
   * @returns {object} graph - The data to populate the main/aggregate spigraph sessions timeline graph
   * @returns {array} items - The list of field values with their corresponding timeline graph and map data
   * @returns {number} recordsTotal - The total number of files Arkime knows about
   * @returns {number} recordsFiltered - The number of files returned in this result
   * @returns {ESHealth} health - The elasticsearch cluster health status and info
   */
  module.getSPIGraph = (req, res) => {
    req.query.facets = 1;

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      const results = { items: [], graph: {}, map: {} };
      if (bsqErr) {
        return res.molochError(403, bsqErr.toString());
      }

      let options = {};
      if (req.query.cancelId) {
        options.cancelId = `${req.user.userId}::${req.query.cancelId}`;
      }
      options = ViewerUtils.addCluster(req.query.cluster, options);

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
        Db.numberOfDocuments('sessions2-*', options.cluster ? { cluster: options.cluster } : {}),
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

        const aggs = result.aggregations.field.buckets;
        const filter = { term: {} };
        const sfilter = { term: {} };
        query.query.bool.filter.push(filter);

        if (field === 'ip.dst:port') {
          query.query.bool.filter.push(sfilter);
        }

        delete query.aggregations.field;

        let queriesInfo = [];
        function endCb () {
          queriesInfo = queriesInfo.sort((a, b) => { return b.doc_count - a.doc_count; }).slice(0, size * 2);
          const queries = queriesInfo.map((item) => { return item.query; });

          Db.msearch(indices, 'session', queries, options, (err, result) => {
            if (!result.responses) {
              return res.send(results);
            }

            result.responses.forEach((item, i) => {
              const response = {
                name: queriesInfo[i].key,
                count: queriesInfo[i].doc_count
              };

              response.graph = ViewerUtils.graphMerge(req, query, result.responses[i].aggregations);

              const histoKeys = Object.keys(results.graph).filter(i => i.toLowerCase().includes('histo'));
              const xMinName = histoKeys.reduce((prev, curr) => results.graph[prev][0][0] < results.graph[curr][0][0] ? prev : curr);
              const histoXMin = results.graph[xMinName][0][0];
              const xMaxName = histoKeys.reduce((prev, curr) => {
                return results.graph[prev][results.graph[prev].length - 1][0] > results.graph[curr][results.graph[curr].length - 1][0] ? prev : curr;
              });
              const histoXMax = results.graph[xMaxName][results.graph[xMaxName].length - 1][0];

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

              const graph = response.graph;
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
                const s = req.query.sort || 'sessionsHisto';
                results.items = results.items.sort((a, b) => {
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

        const intermediateResults = [];
        function findFileNames () {
          async.each(intermediateResults, (fsitem, cb) => {
            const split = fsitem.key.split(':');
            const node = split[0];
            const fileId = split[1];
            Db.fileIdToFile(node, fileId, (file) => {
              if (file && file.name) {
                queriesInfo.push({ key: file.name, doc_count: fsitem.doc_count, query: fsitem.query });
              }
              cb();
            });
          }, () => {
            endCb();
          });
        }

        aggs.forEach((item) => {
          if (field === 'ip.dst:port') {
            filter.term.dstIp = item.key;
            const sep = (item.key.indexOf(':') === -1) ? ':' : '.';
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
   * POST/GET - /api/spigraphhierarchy
   *
   * Builds an elasticsearch session query. Gets a list of values for each field with counts and returns them to the client.
   * @name /spigraphhierarchy
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} exp - Comma separated list of db fields to populate the graph/table.
   * @param {boolean} strictly=false - When set the entire session must be inside the date range to be observed, otherwise if it overlaps it is displayed. Overwrites the bounding parameter, sets bonding to 'both'
   * @returns {object} hierarchicalResults - The nested data to populate the treemap or pie
   * @returns {array} tableResults - The list data to populate the table
   */
  module.getSPIGraphHierarchy = (req, res) => {
    if (req.query.exp === undefined) {
      return res.molochError(403, 'Missing exp parameter');
    }

    const fields = [];
    const parts = req.query.exp.split(',');
    for (let i = 0; i < parts.length; i++) {
      if (internals.scriptAggs[parts[i]] !== undefined) {
        fields.push(internals.scriptAggs[parts[i]]);
        continue;
      }
      const field = Config.getFieldsMap()[parts[i]];
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

      const options = ViewerUtils.addCluster(req.query.cluster);
      Db.searchPrimary(indices, 'session', query, options, (err, result) => {
        if (err) {
          console.log('spigraphpie ERROR', err);
          res.status(400);
          return res.end(err);
        }

        if (Config.debug > 2) {
          console.log('result', JSON.stringify(result, false, 2));
        }

        // format the data for the pie graph
        const hierarchicalResults = { name: 'Top Talkers', children: [] };
        function addDataToPie (buckets, addTo) {
          for (let i = 0; i < buckets.length; i++) {
            const bucket = buckets[i];
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

        // There is 1 entry per row, the entry is determine by the leafs, with an array of parents.
        // This uses a depth first search.
        const tableResults = [];
        function addDataToTable (parents, buckets) {
          for (const bucket of buckets) {
            if (bucket.field) {
              // Not leaf - add this one to parents list, recurse, and remove from parents list
              parents.push({
                name: bucket.key,
                size: bucket.doc_count
              });
              // Make a copy of parents since the callee saves and we modified after calling
              addDataToTable(JSON.parse(JSON.stringify(parents)), bucket.field.buckets);
              parents.pop();
            } else {
              // Leaf - add an entry to tableResults
              tableResults.push({
                name: bucket.key,
                size: bucket.doc_count,
                parents: parents
              });
            }
          }
        }

        // TODO ECR - go infinity levels deep here too?
        addDataToPie(result.aggregations.field.buckets, hierarchicalResults.children);
        addDataToTable([], result.aggregations.field.buckets);

        return res.send({
          success: true,
          tableResults: tableResults,
          hierarchicalResults: hierarchicalResults
        });
      });
    });
  };

  /**
   * POST/GET - /api/unique
   *
   * Builds an elasticsearch session query. Gets a list of unique field values (with or without counts) and sends them to the client.
   * @name /unique
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {number} counts=0 - Whether to return counts with he list of unique field values. Defaults to 0. 0 = no counts, 1 - counts.
   * @param {string} exp - Comma separated list of expression field names to return.
   * @returns {string} The list of unique fields (with counts if requested)
   */
  module.getUnique = (req, res) => {
    ViewerUtils.noCache(req, res, 'text/plain; charset=utf-8');

    if (req.query.field === undefined && req.query.exp === undefined) {
      return res.send('Missing field or exp parameter');
    }

    /* How should the results be written.  Use setImmediate to not blow stack frame */
    let writeCb;
    let doneCb;
    const items = [];
    let aggSize = +Config.get('maxAggSize', 10000);

    if (req.query.autocomplete !== undefined) {
      if (!Config.get('valueAutoComplete', !Config.get('multiES', false))) {
        res.send([]);
        return;
      }

      const spiDataMaxIndices = +Config.get('spiDataMaxIndices', 4);
      if (spiDataMaxIndices !== -1) {
        if (req.query.date === '-1' ||
            (req.query.date !== undefined && +req.query.date > spiDataMaxIndices)) {
          console.log(`INFO For autocomplete replacing date=${ViewerUtils.safeStr(req.query.date)} with ${spiDataMaxIndices}`);
          req.query.date = spiDataMaxIndices;
        }
      }

      aggSize = 1000; // lower agg size for autocomplete
      doneCb = () => {
        res.send(items);
      };
      writeCb = (item) => {
        items.push(item.key);
      };
    } else if (parseInt(req.query.counts, 10) || 0) {
      writeCb = (item) => {
        res.write(`${item.key}, ${item.doc_count}\n`);
      };
    } else {
      writeCb = (item) => {
        res.write(`${item.key}\n`);
      };
    }

    /* How should each item be processed. */
    let eachCb = writeCb;

    if (req.query.field.match(/(ip.src:port.src|a1:p1|srcIp:srtPort|ip.src:srcPort|ip.dst:port.dst|a2:p2|dstIp:dstPort|ip.dst:dstPort)/)) {
      eachCb = (item) => {
        const sep = (item.key.indexOf(':') === -1) ? ':' : '.';
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
        const intermediateResults = [];
        const aggs = result.aggregations.field.buckets;
        aggs.forEach((item) => {
          item.field2.buckets.forEach((sitem) => {
            intermediateResults.push({ key: item.key + ':' + sitem.key, doc_count: sitem.doc_count });
          });
        });

        async.each(intermediateResults, (fsitem, cb) => {
          const split = fsitem.key.split(':');
          const node = split[0];
          const fileId = split[1];
          Db.fileIdToFile(node, fileId, (file) => {
            if (file && file.name) {
              eachCb({ key: file.name, doc_count: fsitem.doc_count });
            }
            cb();
          });
        }, () => {
          return res.end();
        });
      }

      const options = ViewerUtils.addCluster(req.query.cluster);
      Db.searchPrimary(indices, 'session', query, options, (err, result) => {
        if (err) {
          console.log('Error', query, err);
          return doneCb ? doneCb() : res.end();
        }
        if (Config.debug) {
          console.log('unique result', util.inspect(result, false, 50));
        }
        if (!result.aggregations || !result.aggregations.field) {
          return doneCb ? doneCb() : res.end();
        }

        if (req.query.field === 'fileand') {
          return findFileNames(result);
        }

        const ilen = result.aggregations.field.buckets.length;
        for (let i = 0; i < ilen; i++) {
          eachCb(result.aggregations.field.buckets[i]);
        }

        return doneCb ? doneCb() : res.end();
      });
    });
  };

  /**
   * POST/GET - /api/multiunique
   *
   * Builds an elasticsearch session query. Gets an intersection of unique field values (with or without counts) and sends them to the client.
   * @name /multiunique
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {number} counts=0 - Whether to return counts with he list of unique field values. Defaults to 0. 0 = no counts, 1 - counts.
   * @param {string} exp - The expression field to return unique data for. Either exp or field is required, field is given priority if both are present.
   * @param {string} field - The database field to return unique data for. Either exp or field is required, field is given priority if both are present.
   * @returns {string} The list of an intersection of unique fields (with counts if requested)
   */
  module.getMultiunique = (req, res) => {
    ViewerUtils.noCache(req, res, 'text/plain; charset=utf-8');

    if (req.query.exp === undefined) {
      return res.send('Missing exp parameter');
    }

    const fields = [];
    const parts = req.query.exp.split(',');
    for (let i = 0; i < parts.length; i++) {
      const field = Config.getFieldsMap()[parts[i]];
      if (!field) {
        return res.send(`Unknown expression ${parts[i]}\n`);
      }
      fields.push(field);
    }

    const separator = req.query.separator || ', ';
    const doCounts = parseInt(req.query.counts, 10) || 0;

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

      const options = ViewerUtils.addCluster(req.query.cluster);
      Db.searchPrimary(indices, 'session', query, options, (err, result) => {
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
          results = results.sort((a, b) => { return b.count - a.count; });
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
   * GET - /api/session/:nodeName/:id/detail
   *
   * Gets SPI data for a session.
   * @name /session/:nodeName/:id/detail
   * @returns {html} The html to display as session detail
   */
  module.getDetail = (req, res) => {
    const options = ViewerUtils.addCluster(req.query.cluster);
    Db.getSession(req.params.id, options, (err, session) => {
      if (err || !session.found) {
        return res.end("Couldn't look up SPI data, error for session " + ViewerUtils.safeStr(req.params.id) + ' Error: ' + err);
      }

      session = session._source;

      session.id = req.params.id;

      sortFields(session);

      const hidePackets = (session.fileId === undefined || session.fileId.length === 0) ? 'true' : 'false';
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
        }, (err, data) => {
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
   * GET - /api/session/:nodeName/:id/packets
   *
   * Gets packets for a session.
   * @name /session/:nodeName/:id/packets
   * @returns {html} The html to display as session packets
   */
  module.getPackets = (req, res) => {
    module.isLocalView(req.params.nodeName, () => {
      ViewerUtils.noCache(req, res);
      req.packetsOnly = true;
      localSessionDetail(req, res);
    }, () => {
      return module.proxyRequest(req, res);
    });
  };

  /**
   * POST - /api/sessions/addtags
   *
   * Add tag(s) to individual session(s) by id or by query.
   * @name /sessions/addtags
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} tags - Comma separated list of tags to add to session(s)
   * @param {string} ids - Comma separated list of sessions to add tag(s) to
   * @param {string} segments=no - Whether to add tags to linked session segments. Default is no. Options include:
     no - Don't add tags to linked segments
     all - Add tags to all linked segments
     time - Add tags to segments occurring in the same time period
   * @returns {boolean} success - Whether the add tags operation was successful
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  module.addTags = (req, res) => {
    let tags = [];
    if (req.body.tags) {
      tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, '').split(',');
    }

    if (tags.length === 0) {
      return res.molochError(200, 'No tags specified');
    }

    if (req.body.ids) {
      const ids = ViewerUtils.queryValueToArray(req.body.ids);

      module.sessionsListFromIds(req, ids, ['tags', 'node'], (err, list) => {
        if (!list.length) {
          return res.molochError(200, 'No sessions to add tags to');
        }
        module.addTagsList(tags, list, () => {
          return res.send(JSON.stringify({
            success: true,
            text: 'Tags added successfully'
          }));
        });
      });
    } else {
      sessionsListFromQuery(req, res, ['tags', 'node'], (err, list) => {
        if (!list.length) {
          return res.molochError(200, 'No sessions to add tags to');
        }
        module.addTagsList(tags, list, () => {
          return res.send(JSON.stringify({
            success: true,
            text: 'Tags added successfully'
          }));
        });
      });
    }
  };

  /**
   * POST - /api/sessions/removetags
   *
   * Removes tag(s) from individual session(s) by id or by query.
   * @name /sessions/removetags
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} tags - Comma separated list of tags to remove from session(s)
   * @param {string} ids - Comma separated list of sessions to remove tag(s) from
   * @param {string} segments=no - Whether to remove tags from linked session segments. Default is no. Options include:
     no - Don't remove tags from linked segments
     all - Remove tags from all linked segments
     time - Remove tags from segments occurring in the same time period
   * @returns {boolean} success - Whether the remove tags operation was successful
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  module.removeTags = (req, res) => {
    let tags = [];
    if (req.body.tags) {
      tags = req.body.tags.replace(/[^-a-zA-Z0-9_:,]/g, '').split(',');
    }

    if (tags.length === 0) {
      return res.molochError(200, 'No tags specified');
    }

    if (req.body.ids) {
      const ids = ViewerUtils.queryValueToArray(req.body.ids);

      module.sessionsListFromIds(req, ids, ['tags'], (err, list) => {
        module.removeTagsList(res, tags, list);
      });
    } else {
      sessionsListFromQuery(req, res, ['tags'], (err, list) => {
        module.removeTagsList(res, tags, list);
      });
    }
  };

  /**
   * GET - /api/session/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName
   *
   * Retrieves a file that was transferred in a session.
   * @name /session/:nodeName/:id/body/:bodyType/:bodyNum/:bodyName
   * @returns {file} file - The file in the session
   */
  module.getRawBody = (req, res) => {
    reqGetRawBody(req, (err, data) => {
      if (err) {
        console.trace(err);
        return res.end('Error');
      }

      res.setHeader('Content-Type', 'application/force-download');
      res.setHeader('content-disposition', contentDisposition(req.params.bodyName));

      return res.end(data);
    });
  };

  /**
   * GET - /api/session/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName
   *
   * Retrieves a bitmap image representation of the bytes in a file.
   * @name /session/:nodeName/:id/bodypng/:bodyType/:bodyNum/:bodyName
   * @returns {image/png} image - The bitmap image.
   */
  module.getFilePNG = (req, res) => {
    reqGetRawBody(req, (err, data) => {
      if (err || data === null || data.length === 0) {
        return res.send(internals.emptyPNG);
      }

      res.setHeader('Content-Type', 'image/png');

      const png = new PNG({
        width: internals.PNG_LINE_WIDTH,
        height: Math.ceil(data.length / internals.PNG_LINE_WIDTH)
      });

      png.data = data;

      res.send(PNG.sync.write(png, { inputColorType: 0, colorType: 0, bitDepth: 8, inputHasAlpha: false }));
    });
  };

  /**
   * GET - /api/sessions/pcap OR /api/sessions.pcap
   *
   * Retrieve the raw session data in pcap format.
   * @name /sessions/pcap
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} ids - The list of ids to return
   * @param {boolean} segments=false - When set return linked segments
   * @returns {pcap} A PCAP file with the sessions requested
   */
  module.getPCAP = (req, res) => {
    return sessionsPcap(req, res, writePcap, 'pcap');
  };

  /**
   * GET - /api/sessions/pcapng OR /api/sessions.pcapng
   *
   * Retrieve the raw session data in pcapng format.
   * @name /sessions/pcapng
   * @param {SessionsQuery} query - The request query to filter sessions
   * @param {string} ids - The list of ids to return
   * @param {boolean} segments=false - When set return linked segments
   * @returns {pcap} A PCAPNG file with the sessions requested
   */
  module.getPCAPNG = (req, res) => {
    return sessionsPcap(req, res, writePcapNg, 'pcapng');
  };

  /**
   * GET - /api/session/:nodeName/:id/pcap OR /api/session/:nodeName/:id.pcap
   *
   * Retrieve the raw session data in pcap format from a specific node.
   * @name /session/:nodeName/:id/pcap
   * @returns {pcap} A PCAP file with the session requested
   */
  module.getPCAPFromNode = (req, res) => {
    ViewerUtils.noCache(req, res, 'application/vnd.tcpdump.pcap');
    const writeHeader = !req.query || !req.query.noHeader || req.query.noHeader !== 'true';
    writePcap(res, req.params.id, { writeHeader: writeHeader }, () => {
      res.end();
    });
  };

  /**
   * GET - /api/session/:nodeName/:id/pcapng OR /api/session/:nodeName/:id.pcapng
   *
   * Retrieve the raw session data in pcapng format from a specific node.
   * @name /session/:nodeName/:id/pcapng
   * @returns {pcap} A PCAPNG file with the session requested
   */
  module.getPCAPNGFromNode = (req, res) => {
    ViewerUtils.noCache(req, res, 'application/vnd.tcpdump.pcap');
    const writeHeader = !req.query || !req.query.noHeader || req.query.noHeader !== 'true';
    writePcapNg(res, req.params.id, { writeHeader: writeHeader }, () => {
      res.end();
    });
  };

  /**
   * GET - /api/session/entire/:nodeName/:id/pcap OR /api/session/entire/:nodeName/:id.pcap
   *
   * Retrieve the entire pcap for a session.
   * @name /session/entire/:nodeName/:id/pcap
   * @returns {pcap} A PCAP file with the session requested
   */
  module.getEntirePCAP = (req, res) => {
    ViewerUtils.noCache(req, res, 'application/vnd.tcpdump.pcap');

    const options = { writeHeader: true };

    const query = {
      size: 1000,
      _source: ['rootId'],
      sort: { lastPacket: { order: 'asc' } },
      query: { term: { rootId: req.params.id } }
    };

    if (Config.debug) {
      console.log('entirePcap query', JSON.stringify(query));
    }

    Db.searchPrimary('sessions2-*', 'session', query, null, (err, data) => {
      async.forEachSeries(data.hits.hits, (item, nextCb) => {
        writePcap(res, Db.session2Sid(item), options, nextCb);
      }, (err) => {
        res.end();
      });
    });
  };

  /**
   * GET - /api/session/raw/:nodeName/:id/png OR /api/session/raw/:nodeName/:id.png
   *
   * Retrieve a bitmap image representation of packets in a session.
   * @name /session/raw/:nodeName/:id/png
   * @param {string} type=src - Whether to retrieve the src (source) or dst (desintation) packets bitmap image. Defaults to src.
   * @returns {image/png} image - The bitmap image.
   */
  module.getPacketPNG = (req, res) => {
    ViewerUtils.noCache(req, res, 'image/png');

    module.processSessionIdAndDecode(req.params.id, 1000, (err, session, results) => {
      if (err) {
        return res.send(internals.emptyPNG);
      }

      let size = 0;
      let i = (req.query.type !== 'dst' ? 0 : 1);
      for (let ilen = results.length; i < ilen; i += 2) {
        size += results[i].data.length + 2 * internals.PNG_LINE_WIDTH - (results[i].data.length % internals.PNG_LINE_WIDTH);
      }

      const buffer = Buffer.alloc(size, 0);
      let pos = 0;
      if (size === 0) {
        return res.send(internals.emptyPNG);
      }

      for (let i = (req.query.type !== 'dst' ? 0 : 1); i < results.length; i += 2) {
        results[i].data.copy(buffer, pos);
        pos += results[i].data.length;
        const fillpos = pos;
        pos += 2 * internals.PNG_LINE_WIDTH - (results[i].data.length % internals.PNG_LINE_WIDTH);
        buffer.fill(0xff, fillpos, pos);
      }

      const png = new PNG({ width: internals.PNG_LINE_WIDTH, height: (size / internals.PNG_LINE_WIDTH) - 1 });
      png.data = buffer;
      res.send(PNG.sync.write(
        png,
        {
          inputColorType: 0,
          colorType: 0,
          bitDepth: 8,
          inputHasAlpha: false
        }
      ));
    });
  };

  /**
   * GET - /api/session/raw/:nodeName/:id
   *
   * Retrieve raw packets for a session.
   * @name /session/raw/:nodeName/:id
   * @param {string} type=src - Whether to retrieve the src (source) or dst (desintation) raw packets. Defaults to src.
   * @returns {string} The source or destination packet text.
   */
  module.getRawPackets = (req, res) => {
    ViewerUtils.noCache(req, res, 'application/vnd.tcpdump.pcap');

    module.processSessionIdAndDecode(req.params.id, 10000, (err, session, results) => {
      if (err) {
        return res.send('Error');
      }

      for (let i = (req.query.type !== 'dst' ? 0 : 1); i < results.length; i += 2) {
        res.write(results[i].data);
      }

      res.end();
    });
  };

  /**
   * GET - /api/sessions/bodyhash/:hash
   *
   * Retrieve a file given a hash of that file.
   * @name /sessions/bodyhash/:hash
   * @param {SessionsQuery} query - The request query to filter sessions
   * @returns {file} file - The file that matches the hash
   */
  module.getBodyHash = (req, res) => {
    let hash = null;
    let nodeName = null;
    let sessionID = null;

    module.buildSessionQuery(req, (bsqErr, query, indices) => {
      if (bsqErr) {
        res.status(400);
        return res.end(bsqErr);
      }

      query.size = 1;
      query.sort = { lastPacket: { order: 'desc' } };
      query._source = ['node'];

      if (Config.debug) {
        console.log(`sessions.json ${indices} query`, JSON.stringify(query, null, 1));
      }

      Db.searchPrimary(indices, 'session', query, null, (err, sessions) => {
        if (err) {
          console.log('Error -> Db Search ', err);
          res.status(400);
          res.end(err);
        } else if (sessions.error) {
          console.log('Error -> Db Search ', sessions.error);
          res.status(400);
          res.end(sessions.error);
        } else {
          if (Config.debug) {
            console.log('bodyHash result', util.inspect(sessions, false, 50));
          }

          if (sessions.hits.hits.length > 0) {
            nodeName = sessions.hits.hits[0]._source.node;
            sessionID = Db.session2Sid(sessions.hits.hits[0]);
            hash = req.params.hash;

            module.isLocalView(nodeName, () => { // get file from the local disk
              localGetItemByHash(nodeName, sessionID, hash, (err, item) => {
                if (err) {
                  res.status(400);
                  return res.end(err);
                } else if (item) {
                  ViewerUtils.noCache(req, res, 'application/force-download');
                  res.setHeader('content-disposition', contentDisposition(item.bodyName + '.pellet'));
                  return res.end(item.data);
                } else {
                  res.status(400);
                  return res.end('No Match');
                }
              });
            }, () => { // get file from the remote disk
              const preq = Object.assign({}, req);
              preq.params.nodeName = nodeName;
              preq.params.id = sessionID;
              preq.params.hash = hash;
              preq.url = `api/session/${Config.basePath(nodeName) + nodeName}/${sessionID}/bodyhash/${hash}`;
              return module.proxyRequest(preq, res);
            });
          } else {
            res.status(400);
            res.end('No Match Found');
          }
        }
      });
    });
  };

  /**
   * @ignore
   * POST - /api/sessions/decodings
   *
   * Retrieve decodings.
   * @name /sessions/decodings
   */
  module.getDecodings = (req, res) => {
    res.send(JSON.stringify(decode.settings()));
  };

  /**
   * GET - /api/session/:nodeName/:id/bodyhash/:hash
   *
   * Retrieve a file from a specific node given a hash of that file.
   * @name /session/:nodeName/:id/bodyhash/:hash
   * @param {SessionsQuery} query - The request query to filter sessions
   * @returns {file} file - The file that matches the hash
   */
  module.getBodyHashFromNode = (req, res) => {
    localGetItemByHash(req.params.nodeName, req.params.id, req.params.hash, (err, item) => {
      if (err) {
        res.status(400);
        return res.end(err);
      } else if (item) {
        ViewerUtils.noCache(req, res, 'application/force-download');
        res.setHeader('content-disposition', contentDisposition(item.bodyName + '.pellet'));
        return res.end(item.data);
      } else {
        res.status(400);
        return res.end('No Match');
      }
    });
  };

  /**
   * GET - /api/delete
   *
   * Delete SPI and/or scrub PCAP data (remove persmission required).
   * @name /delete
   * @param {string} removeSpi=false - Whether to remove the SPI data.
   * @param {string} removePcap=true - Whether to remove the PCAP data.
   * @returns {boolean} success - Whether the operation was successful
   * @returns {string} text - The success/error message to (optionally) display to the user
   */
  module.deleteData = (req, res) => {
    if (req.query.removeSpi !== 'true' && req.query.removePcap !== 'true') {
      return res.molochError(403, 'You can\'t delete nothing');
    }

    let whatToRemove;
    if (req.query.removeSpi === 'true' && req.query.removePcap === 'true') {
      whatToRemove = 'all';
    } else if (req.query.removeSpi === 'true') {
      whatToRemove = 'spi';
    } else {
      whatToRemove = 'pcap';
    }

    if (req.body.ids) {
      const ids = ViewerUtils.queryValueToArray(req.body.ids);
      module.sessionsListFromIds(req, ids, ['node'], (err, list) => {
        scrubList(req, res, whatToRemove, list);
      });
    } else if (req.query.expression) {
      sessionsListFromQuery(req, res, ['node'], (err, list) => {
        scrubList(req, res, whatToRemove, list);
      });
    } else {
      return res.molochError(403, 'Error: Missing expression. An expression is required so you don\'t delete everything.');
    }
  };

  /**
   * @ignore
   * GET - /api/session/:nodeName/:id/send
   *
   * Sends a session to a node.
   * @name /session/:nodeName/:id/send
   */
  module.sendSessionToNode = (req, res) => {
    ViewerUtils.noCache(req, res);
    res.statusCode = 200;

    const options = {
      user: req.user,
      cluster: req.query.cluster,
      id: req.params.id,
      saveId: req.query.saveId,
      tags: req.body.tags,
      nodeName: req.params.nodeName
    };

    internals.sendSessionQueue.push(options, () => {
      res.end();
    });
  };

  /**
   * @ignore
   * POST - /api/sessions/:nodeName/send
   *
   * Sends sessions to a node.
   * @name /sessions/:nodeName/send
   * @param {string} ids - Comma separated list of session ids.
   * @param {string} tags - Commas separated list of tags to tag the sent sessions with.
   * @param {string} cluster - The name of the Arkime cluster to send the sessions.
   * @param {saveId} saveId - The sessionId to use on the remote side.
   */
  module.sendSessionsToNode = (req, res) => {
    ViewerUtils.noCache(req, res);
    res.statusCode = 200;

    if (req.body.ids === undefined ||
      req.query.cluster === undefined ||
      req.query.saveId === undefined ||
      req.body.tags === undefined) {
      return res.end();
    }

    let count = 0;
    const ids = ViewerUtils.queryValueToArray(req.body.ids);
    ids.forEach((id) => {
      const options = {
        user: req.user,
        cluster: req.query.cluster,
        id: id,
        saveId: req.query.saveId,
        tags: req.body.tags,
        nodeName: req.params.nodeName
      };

      count++;
      internals.sendSessionQueue.push(options, () => {
        count--;
        if (count === 0) {
          return res.end();
        }
      });
    });
  };

  /**
   * @ignore
   * POST - /api/sessions/send
   *
   * Sends sessions.
   * @name /sessions/send
   * @param {string} ids - Comma separated list of session ids.
   */
  module.sendSessions = (req, res) => {
    if (req.body.ids) {
      const ids = ViewerUtils.queryValueToArray(req.body.ids);

      module.sessionsListFromIds(req, ids, ['node'], (err, list) => {
        sendSessionsList(req, res, list);
      });
    } else {
      sessionsListFromQuery(req, res, ['node'], (err, list) => {
        sendSessionsList(req, res, list);
      });
    }
  };

  /**
   * @ignore
   * POST - /api/sessions/receive
   *
   * Receive sessions.
   * @name /sessions/receive
   * @param {saveId} saveId - The sessionId to save the session.
   */
  module.receiveSession = (req, res) => {
    if (!req.query.saveId) { return res.molochError(200, 'Missing saveId'); }

    req.query.saveId = req.query.saveId.replace(/[^-a-zA-Z0-9_]/g, '');

    // JS Static Variable :)
    this.saveIds = this.saveIds || {};

    let saveId = this.saveIds[req.query.saveId];
    if (!saveId) {
      saveId = this.saveIds[req.query.saveId] = { start: 0 };
    }

    let sessionlen = -1;
    let filelen = -1;
    let written = 0;
    let session = null;
    let buffer;
    let file;
    let writeHeader;

    function makeFilename (cb) {
      if (saveId.filename) {
        return cb(saveId.filename);
      }

      // Just keep calling ourselves every 100 ms until we have a filename
      if (saveId.inProgress) {
        return setTimeout(makeFilename, 100, cb);
      }

      saveId.inProgress = 1;
      Db.getSequenceNumber('fn-' + Config.nodeName(), (err, seq) => {
        const filename = Config.get('pcapDir') + '/' + Config.nodeName() + '-' + seq + '-' + req.query.saveId + '.pcap';
        saveId.seq = seq;
        Db.indexNow('files', 'file', Config.nodeName() + '-' + saveId.seq, { num: saveId.seq, name: filename, first: session.firstPacket, node: Config.nodeName(), filesize: -1, locked: 1 }, () => {
          cb(filename);
          saveId.filename = filename; // Don't set the saveId.filename until after the first request completes its callback.
        });
      });
    }

    function saveSession () {
      const id = session.id;
      delete session.id;
      Db.indexNow(Db.sid2Index(id), 'session', Db.sid2Id(id), session, (err, info) => {});
    }

    function chunkWrite (chunk) {
      // Write full chunk if first packet and writeHeader or not first packet
      if (writeHeader || written !== 0) {
        writeHeader = false;
        file.write(chunk);
      } else {
        file.write(chunk.slice(24));
      }
      written += chunk.length; // Pretend we wrote it all
    }

    req.on('data', (chunk) => {
      // If the file is open, just write the current chunk
      if (file) {
        return chunkWrite(chunk);
      }

      // If no file is open, then save the current chunk to the end of the buffer.
      if (!buffer) {
        buffer = chunk;
      } else {
        buffer = Buffer.concat([buffer, chunk]);
      }

      // Found the lengths
      if (sessionlen === -1 && (buffer.length >= 12)) {
        sessionlen = buffer.readUInt32BE(0);
        filelen = buffer.readUInt32BE(8);
        buffer = buffer.slice(12);
      }

      // If we know the session len and haven't read the session
      if (sessionlen !== -1 && !session && buffer.length >= sessionlen) {
        session = JSON.parse(buffer.toString('utf8', 0, sessionlen));
        session.node = Config.nodeName();
        buffer = buffer.slice(sessionlen);

        if (filelen > 0) {
          req.pause();

          makeFilename((filename) => {
            req.resume();
            session.packetPos[0] = -saveId.seq;
            session.fileId = [saveId.seq];

            if (saveId.start === 0) {
              file = fs.createWriteStream(filename, { flags: 'w' });
            } else {
              file = fs.createWriteStream(filename, { start: saveId.start, flags: 'r+' });
            }
            writeHeader = saveId.start === 0;

            // Adjust packet location based on where we start writing
            if (saveId.start > 0) {
              for (let p = 1; p < session.packetPos.length; p++) {
                session.packetPos[p] += (saveId.start - 24);
              }
            }

            // Filelen always includes header, if we don't write header subtract it
            saveId.start += filelen;
            if (!writeHeader) {
              saveId.start -= 24;
            }

            // Still more data in buffer, start of pcap
            if (buffer.length > 0) {
              chunkWrite(buffer);
            }

            saveSession();
          });
        } else {
          saveSession();
        }
      }
    });

    req.on('end', (chunk) => {
      if (file) {
        file.end();
      }
      return res.send({ success: true });
    });
  };

  return module;
};
