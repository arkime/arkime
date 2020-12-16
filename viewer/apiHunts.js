'use strict';

const async = require('async');
const path = require('path');
const RE2 = require('re2');

module.exports = (Config, Db, internals, notifierAPIs, Pcap, sessionAPIs, ViewerUtils) => {
  const module = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  function packetSearch (packet, options) {
    let found = false;

    switch (options.searchType) {
    case 'asciicase':
      if (packet.toString().includes(options.search)) {
        found = true;
      }
      break;
    case 'ascii':
      if (packet.toString().toLowerCase().includes(options.search.toLowerCase())) {
        found = true;
      }
      break;
    case 'regex':
      if (options.regex && packet.toString().match(options.regex)) {
        found = true;
      }
      break;
    case 'hex':
      if (packet.toString('hex').includes(options.search)) {
        found = true;
      }
      break;
    case 'hexregex':
      if (options.regex && packet.toString('hex').match(options.regex)) {
        found = true;
      }
      break;
    default:
      console.log('Invalid hunt search type');
    }

    return found;
  }

  function sessionHunt (sessionId, options, cb) {
    if (options.type === 'reassembled') {
      sessionAPIs.processSessionIdAndDecode(sessionId, options.size || 10000, function (err, session, packets) {
        if (err) {
          return cb(null, false);
        }

        let i = 0;
        let increment = 1;
        let len = packets.length;

        if (options.src && !options.dst) {
          increment = 2;
        } else if (options.dst && !options.src) {
          i = 1;
          increment = 2;
        }

        for (i; i < len; i += increment) {
          if (packetSearch(packets[i].data, options)) { return cb(null, true); }
        }

        return cb(null, false);
      });
    } else if (options.type === 'raw') {
      let packets = [];
      sessionAPIs.processSessionId(sessionId, true, null, function (pcap, buffer, cb, i) {
        if (options.src === options.dst) {
          packets.push(buffer);
        } else {
          let packet = {};
          pcap.decode(buffer, packet);
          packet.data = buffer.slice(16);
          packets.push(packet);
        }
        cb(null);
      }, function (err, session) {
        if (err) {
          return cb(null, false);
        }

        let len = packets.length;
        if (options.src === options.dst) {
          // If search both src/dst don't need to check key
          for (let i = 0; i < len; i++) {
            if (packetSearch(packets[i], options)) { return cb(null, true); }
          }
        } else {
          // If searching src NOR dst need to check key
          let skey = Pcap.keyFromSession(session);
          for (let i = 0; i < len; i++) {
            let key = Pcap.key(packets[i]);
            let isSrc = key === skey;
            if (options.src && isSrc) {
              if (packetSearch(packets[i].data, options)) { return cb(null, true); }
            } else if (options.dst && !isSrc) {
              if (packetSearch(packets[i].data, options)) { return cb(null, true); }
            }
          }
        }
        return cb(null, false);
      },
      options.size || 10000, 10);
    }
  }

  function updateHuntStats (hunt, huntId, session, searchedSessions, cb) {
    // update the hunt with number of matchedSessions and searchedSessions
    // and the date of the first packet of the last searched session
    let lastPacketTime = session.lastPacket;
    let now = Math.floor(Date.now() / 1000);

    if ((now - hunt.lastUpdated) >= 2) { // only update every 2 seconds
      Db.get('hunts', 'hunt', huntId, (err, huntHit) => {
        if (!huntHit || !huntHit.found) { // hunt hit not found, likely deleted
          return cb('undefined');
        }

        if (err) {
          let errorText = `Error finding hunt: ${hunt.name} (${huntId}): ${err}`;
          pauseHuntJobWithError(huntId, hunt, { value: errorText });
          return cb({ success: false, text: errorText });
        }

        hunt.status = huntHit._source.status;
        hunt.lastUpdated = now;
        hunt.searchedSessions = searchedSessions || hunt.searchedSessions;
        hunt.lastPacketTime = lastPacketTime;

        Db.setHunt(huntId, hunt, () => {});

        if (hunt.status === 'paused') {
          return cb('paused');
        } else {
          return cb(null);
        }
      });
    } else {
      return cb(null);
    }
  }

  function updateSessionWithHunt (session, sessionId, hunt, huntId) {
    Db.addHuntToSession(Db.sid2Index(sessionId), Db.sid2Id(sessionId), huntId, hunt.name, (err, data) => {
      if (err) { console.log('add hunt info error', session, err, data); }
    });
  }

  function buildHuntOptions (huntId, hunt) {
    let options = {
      src: hunt.src,
      dst: hunt.dst,
      size: hunt.size,
      type: hunt.type,
      search: hunt.search,
      searchType: hunt.searchType
    };

    if (hunt.searchType === 'regex' || hunt.searchType === 'hexregex') {
      try {
        options.regex = new RE2(hunt.search);
      } catch (e) {
        pauseHuntJobWithError(huntId, hunt, {
          value: `Fatal Error: Regex parse error. Fix this issue with your regex and create a new hunt: ${e}`,
          unrunnable: true
        });
      }
    }

    return options;
  }

  // if we couldn't retrieve the seession, skip it but add it to failedSessionIds
  // so that we can go back and search for it at the end of the hunt
  function continueHuntSkipSession (hunt, huntId, session, sessionId, searchedSessions, cb) {
    if (!hunt.failedSessionIds) {
      hunt.failedSessionIds = [ sessionId ];
    } else {
      // pause the hunt if there are more than 10k failed sessions
      if (hunt.failedSessionIds.length > 10000) {
        return pauseHuntJobWithError(huntId, hunt, {
          value: 'Error hunting: Too many sessions are unreachable. Please contact your administrator.'
        });
      }
      // make sure the session id is not already in the array
      // if it's not the first pass and a node is still down, this could be a duplicate
      if (hunt.failedSessionIds.indexOf(sessionId) < 0) {
        hunt.failedSessionIds.push(sessionId);
      }
    }

    return updateHuntStats(hunt, huntId, session, searchedSessions, cb);
  }

  // function updateHuntStatus (req, res, status, successText, errorText) {
  //   Db.getHunt(req.params.id, (err, hit) => {
  //     if (err) {
  //       console.log(errorText, err, hit);
  //       return res.molochError(500, errorText);
  //     }
  //
  //     // don't let a user play a hunt job if one is already running
  //     if (status === 'running' && internals.runningHuntJob) {
  //       return res.molochError(403, 'You cannot start a new hunt until the running job completes or is paused.');
  //     }
  //
  //     let hunt = hit._source;
  //
  //     // if hunt is finished, don't allow pause
  //     if (hunt.status === 'finished' && status === 'paused') {
  //       return res.molochError(403, 'You cannot pause a completed hunt.');
  //     }
  //
  //     // clear the running hunt job if this is it
  //     if (hunt.status === 'running') { internals.runningHuntJob = undefined; }
  //     hunt.status = status; // update the hunt job
  //
  //     Db.setHunt(req.params.id, hunt, (err, info) => {
  //       if (err) {
  //         console.log(errorText, err, info);
  //         return res.molochError(500, errorText);
  //       }
  //       res.send(JSON.stringify({ success: true, text: successText }));
  //       module.processHuntJobs();
  //     });
  //   });
  // }

  // if there are failed sessions, go through them one by one and do a packet search
  // if there are no failed sessions left at the end then the hunt is done
  // if there are still failed sessions, but some sessions were searched during the last pass, try again
  // if there are still failed sessions, but no new sessions coudl be searched, pause the job with an error
  function huntFailedSessions (hunt, huntId, options, searchedSessions, user) {
    if (!hunt.failedSessionIds && !hunt.failedSessionIds.length) { return; }

    let changesSearchingFailedSessions = false;

    options.searchingFailedSessions = true;
    // copy the failed session ids so we can remove them from the hunt
    // but still loop through them iteratively
    let failedSessions = JSON.parse(JSON.stringify(hunt.failedSessionIds));
    // we don't need to search the db for session, we just need to search each session in failedSessionIds
    async.forEachLimit(failedSessions, 3, function (sessionId, cb) {
      Db.getSession(sessionId, { _source: 'node,huntName,huntId,lastPacket,field' }, (err, session) => {
        if (err) {
          return continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
        }

        session = session._source;

        ViewerUtils.makeRequest(session.node, `${session.node}/hunt/${huntId}/remote/${sessionId}`, user, (err, response) => {
          if (err) {
            return continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
          }

          let json = JSON.parse(response);

          if (json.error) {
            console.log(`Error hunting on remote viewer: ${json.error} - ${path}`);
            return continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
          }

          // remove from failedSessionIds if it was found
          hunt.failedSessionIds.splice(hunt.failedSessionIds.indexOf(sessionId), 1);
          // there were changes to this hunt; we're making progress
          changesSearchingFailedSessions = true;

          if (json.matched) { hunt.matchedSessions++; }
          return updateHuntStats(hunt, huntId, session, searchedSessions, cb);
        });
      });
    }, function (err) { // done running a pass of the failed sessions
      function continueProcess () {
        Db.setHunt(huntId, hunt, (err, info) => {
          internals.runningHuntJob = undefined;
          module.processHuntJobs(); // Start new hunt
        });
      }

      if (hunt.failedSessionIds && hunt.failedSessionIds.length === 0) {
        options.searchingFailedSessions = false; // no longer searching failed sessions
        // we had failed sessions but we're done searching through them
        // so we're completely done with this hunt (inital search and failed sessions)
        hunt.status = 'finished';

        if (hunt.notifier) {
          let message = `*${hunt.name}* hunt job finished:\n*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions`;
          notifierAPIs.issueAlert(hunt.notifier, message, continueProcess);
        } else {
          return continueProcess();
        }
      } else if (hunt.failedSessionIds && hunt.failedSessionIds.length > 0 && changesSearchingFailedSessions) {
        // there are still failed sessions, but there were also changes,
        // so keep going
        // uninitialize hunts so that the running job with failed sessions will kick off again
        internals.proccessHuntJobsInitialized = false;
        return continueProcess();
      } else if (!changesSearchingFailedSessions) {
        options.searchingFailedSessions = false; // no longer searching failed sessions
        // there were no changes, we're still struggling to connect to one or
        // more renote nodes, so error out
        return pauseHuntJobWithError(huntId, hunt, {
          value: 'Error hunting previously unreachable sessions. There is likely a node down. Please contact your administrator.'
        });
      }
    });
  }

  // Actually do the search against ES and process the results.
  function runHuntJob (huntId, hunt, query, user) {
    let options = buildHuntOptions(huntId, hunt);
    let searchedSessions;

    // look for failed sessions if we're done searching sessions normally
    if (!options.searchingFailedSessions && hunt.searchedSessions === hunt.totalSessions && hunt.failedSessionIds && hunt.failedSessionIds.length) {
      options.searchingFailedSessions = true;
      return huntFailedSessions(hunt, huntId, options, searchedSessions, user);
    }

    Db.search('sessions2-*', 'session', query, { scroll: internals.esScrollTimeout }, function getMoreUntilDone (err, result) {
      if (err || result.error) {
        pauseHuntJobWithError(huntId, hunt, { value: `Hunt error searching sessions: ${err}` });
        return;
      }

      let hits = result.hits.hits;
      if (searchedSessions === undefined) {
        searchedSessions = hunt.searchedSessions || 0;
        // if the session query results length is not equal to the total sessions that the hunt
        // job is searching, update the hunt total sessions so that the percent works correctly
        if (!options.searchingFailedSessions && hunt.totalSessions !== (result.hits.total + searchedSessions)) {
          hunt.totalSessions = result.hits.total + searchedSessions;
        }
      }

      async.forEachLimit(hits, 3, function (hit, cb) {
        searchedSessions++;
        let session = hit._source;
        let sessionId = Db.session2Sid(hit);
        let node = session.node;

        // There are no files, this is a fake session, don't hunt it
        if (session.fileId === undefined || session.fileId.length === 0) {
          return updateHuntStats(hunt, huntId, session, searchedSessions, cb);
        }

        sessionAPIs.isLocalView(node, function () {
          sessionHunt(sessionId, options, function (err, matched) {
            if (err) {
              return pauseHuntJobWithError(huntId, hunt, { value: `Hunt error searching session (${sessionId}): ${err}` }, node);
            }

            if (matched) {
              hunt.matchedSessions++;
              updateSessionWithHunt(session, sessionId, hunt, huntId);
            }

            updateHuntStats(hunt, huntId, session, searchedSessions, cb);
          });
        },
        function () { // Check Remotely
          let path = `${node}/hunt/${huntId}/remote/${sessionId}`;

          ViewerUtils.makeRequest(node, path, user, (err, response) => {
            if (err) {
              return continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
            }
            let json = JSON.parse(response);
            if (json.error) {
              console.log(`Error hunting on remote viewer: ${json.error} - ${path}`);
              return pauseHuntJobWithError(huntId, hunt, { value: `Error hunting on remote viewer: ${json.error}` }, node);
            }
            if (json.matched) { hunt.matchedSessions++; }
            return updateHuntStats(hunt, huntId, session, searchedSessions, cb);
          });
        });
      }, function (err) { // done running this section of hunt job
        // Some kind of error, stop now
        if (err === 'paused' || err === 'undefined') {
          if (result && result._scroll_id) {
            Db.clearScroll({ body: { scroll_id: result._scroll_id } });
          }
          internals.runningHuntJob = undefined;
          return;
        }

        // There might be more, issue another scroll
        if (result.hits.hits.length !== 0) {
          return Db.scroll({ body: { scroll_id: result._scroll_id }, scroll: internals.esScrollTimeout }, getMoreUntilDone);
        }

        Db.clearScroll({ body: { scroll_id: result._scroll_id } });

        hunt.searchedSessions = hunt.totalSessions;

        function continueProcess () {
          Db.setHunt(huntId, hunt, (err, info) => {
            internals.runningHuntJob = undefined;
            module.processHuntJobs(); // start new hunt or go back over failedSessionIds
          });
        }

        // the hunt is not actually finished, need to go through the failed session ids
        if (hunt.failedSessionIds && hunt.failedSessionIds.length) {
          // uninitialize hunts so that the running job with failed sessions will kick off again
          internals.proccessHuntJobsInitialized = false;
          return continueProcess();
        }

        // We are totally done with this hunt
        hunt.status = 'finished';

        if (hunt.notifier) {
          let message = `*${hunt.name}* hunt job finished:\n*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions`;
          notifierAPIs.issueAlert(hunt.notifier, message, continueProcess);
        } else {
          return continueProcess();
        }
      });
    });
  }

  // Do the house keeping before actually running the hunt job
  function processHuntJob (huntId, hunt) {
    const now = Math.floor(Date.now() / 1000);

    hunt.lastUpdated = now;
    if (!hunt.started) { hunt.started = now; }

    Db.setHunt(huntId, hunt, (err, info) => {
      if (err) {
        pauseHuntJobWithError(huntId, hunt, { value: `Error starting hunt job: ${err} ${info}` });
      }
    });

    ViewerUtils.getUserCacheIncAnon(hunt.userId, (err, user) => {
      if (err && !user) {
        pauseHuntJobWithError(huntId, hunt, { value: err });
        return;
      }
      if (!user || !user.found) {
        pauseHuntJobWithError(huntId, hunt, { value: `User ${hunt.userId} doesn't exist` });
        return;
      }
      if (!user.enabled) {
        pauseHuntJobWithError(huntId, hunt, { value: `User ${hunt.userId} is not enabled` });
        return;
      }

      Db.getLookupsCache(hunt.userId, (err, lookups) => {
        const fakeReq = {
          user: user,
          query: {
            from: 0,
            size: 100, // only fetch 100 items at a time
            _source: ['_id', 'node'],
            sort: 'lastPacket:asc'
          }
        };

        if (hunt.query.expression) {
          fakeReq.query.expression = hunt.query.expression;
        }

        if (hunt.query.view) {
          fakeReq.query.view = hunt.query.view;
        }

        sessionAPIs.buildSessionQuery(fakeReq, (err, query, indices) => {
          if (err) {
            pauseHuntJobWithError(huntId, hunt, {
              value: 'Fatal Error: Session query expression parse error. Fix your search expression and create a new hunt.',
              unrunnable: true
            });
            return;
          }

          ViewerUtils.lookupQueryItems(query.query.bool.filter, (lerr) => {
            query.query.bool.filter[0] = {
              range: {
                lastPacket: {
                  gte: hunt.lastPacketTime || hunt.query.startTime * 1000,
                  lt: hunt.query.stopTime * 1000
                }
              }
            };

            query._source = ['lastPacket', 'node', 'huntId', 'huntName', 'fileId'];

            if (Config.debug > 2) {
              console.log('HUNT', hunt.name, hunt.userId, '- start:', new Date(hunt.lastPacketTime || hunt.query.startTime * 1000), 'stop:', new Date(hunt.query.stopTime * 1000));
            }

            // do sessions query
            runHuntJob(huntId, hunt, query, user);
          });
        });
      });
    });
  }

  // Kick off the process of running a hunt job
  // cb is optional and is called either when a job has been started or end of function
  module.processHuntJobs = (cb) => {
    if (Config.debug) {
      console.log('HUNT - processing hunt jobs');
    }

    if (internals.runningHuntJob) { return (cb ? cb() : null); }
    internals.runningHuntJob = true;

    const query = {
      size: 10000,
      sort: { created: { order: 'asc' } },
      query: { terms: { status: ['queued', 'paused', 'running'] } }
    };

    Db.searchHunt(query)
      .then((hunts) => {
        if (hunts.error) { throw hunts.error; }

        for (const hit of hunts.hits.hits) {
          const hunt = hit._source;
          const id = hit._id;

           // there is a job already running
          if (hunt.status === 'running') {
            internals.runningHuntJob = hunt;
            if (!internals.proccessHuntJobsInitialized) {
              internals.proccessHuntJobsInitialized = true;
              // restart the abandoned or incomplete hunt
              processHuntJob(id, hunt);
            }
            return (cb ? cb() : null);
          } else if (hunt.status === 'queued') { // get the first queued hunt
            internals.runningHuntJob = hunt;
            hunt.status = 'running'; // update the hunt job
            processHuntJob(id, hunt);
            return (cb ? cb() : null);
          }
        }

        // Made to the end without starting a job
        internals.proccessHuntJobsInitialized = true;
        internals.runningHuntJob = undefined;
        return (cb ? cb() : null);
      }).catch(err => {
        console.log('Error fetching hunt jobs', err);
        return (cb ? cb() : null);
      });
  };

  // TODO ECR move this up top
  function pauseHuntJobWithError (huntId, hunt, error, node) {
    let errorMsg = `${hunt.name} (${huntId}) hunt ERROR: ${error.value}.`;
    if (node) {
      errorMsg += ` On ${node} node`;
      error.node = node;
    }

    console.log(errorMsg);

    error.time = Math.floor(Date.now() / 1000);

    hunt.status = 'paused';

    if (error.unrunnable) {
      delete error.unrunnable;
      hunt.unrunnable = true;
    }

    if (!hunt.errors) {
      hunt.errors = [ error ];
    } else {
      hunt.errors.push(error);
    }

    function continueProcess () {
      Db.setHunt(huntId, hunt, (err, info) => {
        internals.runningHuntJob = undefined;
        if (err) {
          console.log('Error adding errors and pausing hunt job', err, info);
          return;
        }
        module.processHuntJobs();
      });
    }

    let message = `*${hunt.name}* hunt job paused with error: *${error.value}*\n*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions`;
    notifierAPIs.issueAlert(hunt.notifier, message, continueProcess);
  }

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * POST - /api/hunt
   *
   * Creates a new hunt (packet search job).
   * @name /hunt
   * @param {SessionsQuery} query - The request query to filter sessions.
   * @param {number} totalSessions - The number of sessions that a hunt applies to.
   * @param {string} name - The name of the hunt (not unique).
   * @param {number} size - The number of packets to search within each session.
   * @param {boolean} src - Whether to search the source packets. Must search src or dst or both.
   * @param {boolean} dst - Whether to search the destination packets. Must search src or dst or both.
   * @param {string} search - The search text to search for within packets.
   * @param {string} searchType - What type of search the text is. Options include:
     ascii - search for case insensitive ascii text.
     asciicase - search for case sensitive ascii text.
     hex - search for hex text.
     regex - search for text using <a href="https://github.com/google/re2/wiki/Syntax">safe regex</a>.
     hexregex - search for text using <a href="https://github.com/google/re2/wiki/Syntax">safe hex regex</a>.
   * @returns {object} query - The elasticsearch query
   * @returns {object} indices - The elasticsearch indices that contain sessions in this query
   */
  module.createHunt = (req, res) => {
    // make sure viewer is not multi
    if (Config.get('multiES', false)) { return res.molochError(401, 'Not supported in multies'); }
    // make sure all the necessary data is included in the post body
    if (!req.body.totalSessions) { return res.molochError(403, 'This hunt does not apply to any sessions'); }
    if (!req.body.name) { return res.molochError(403, 'Missing hunt name'); }
    if (!req.body.size) { return res.molochError(403, 'Missing max mumber of packets to examine per session'); }
    if (!req.body.search) { return res.molochError(403, 'Missing packet search text'); }
    if (!req.body.src && !req.body.dst) {
      return res.molochError(403, 'The hunt must search source or destination packets (or both)');
    }
    if (!req.body.query) { return res.molochError(403, 'Missing query'); }
    if (req.body.query.startTime === undefined || req.body.query.stopTime === undefined) {
      return res.molochError(403, 'Missing fully formed query (must include start time and stop time)');
    }

    const searchTypes = [ 'ascii', 'asciicase', 'hex', 'regex', 'hexregex' ];
    if (!req.body.searchType) {
      return res.molochError(403, 'Missing packet search text type');
    } else if (searchTypes.indexOf(req.body.searchType) === -1) {
      return res.molochError(403, 'Improper packet search text type. Must be "ascii", "asciicase", "hex", "hexregex", or "regex"');
    }

    if (!req.body.type) {
      return res.molochError(403, 'Missing packet search type (raw or reassembled packets)');
    } else if (req.body.type !== 'raw' && req.body.type !== 'reassembled') {
      return res.molochError(403, 'Improper packet search type. Must be "raw" or "reassembled"');
    }

    const limit = req.user.createEnabled ? Config.get('huntAdminLimit', 10000000) : Config.get('huntLimit', 1000000);
    if (parseInt(req.body.totalSessions) > limit) {
      return res.molochError(403, `This hunt applies to too many sessions. Narrow down your session search to less than ${limit} first.`);
    }

    const now = Math.floor(Date.now() / 1000);

    req.body.name = req.body.name.replace(/[^-a-zA-Z0-9_: ]/g, '');

    const hunt = {
      name: req.body.name,
      size: req.body.size,
      search: req.body.search,
      searchType: req.body.searchType,
      type: req.body.type,
      src: req.body.src,
      dst: req.body.dst,
      totalSessions: req.body.totalSessions,
      notifier: req.body.notifier,
      created: now,
      status: 'queued', // always starts as queued
      userId: req.user.userId,
      matchedSessions: 0, // start with no matches
      searchedSessions: 0, // start with no sessions searched
      query: { // only use the necessary query items
        expression: req.body.query.expression,
        startTime: req.body.query.startTime,
        stopTime: req.body.query.stopTime,
        view: req.body.query.view
      }
    };

    function doneCb (hunt, invalidUsers) {
      Db.createHunt(hunt, (err, result) => {
        if (err) {
          console.log('create hunt error', err, result);
          return res.molochError(500, 'Error creating hunt - ' + err);
        }
        hunt.id = result._id;
        module.processHuntJobs(() => {
          const response = {
            success: true,
            hunt: hunt
          };

          if (invalidUsers) {
            response.invalidUsers = invalidUsers;
          }

          return res.send(JSON.stringify(response));
        });
      });
    }

    if (!req.body.users || !req.body.users.length) {
      return doneCb(hunt);
    }

    const reqUsers = ViewerUtils.commaStringToArray(req.body.users);

    ViewerUtils.validateUserIds(reqUsers).then((response) => {
      hunt.users = response.validUsers;

      // dedupe the array of users
      hunt.users = [...new Set(hunt.users)];

      return doneCb(hunt, response.invalidUsers);
    }).catch((error) => {
      res.molochError(500, error);
    });
  };

  return module;
};
