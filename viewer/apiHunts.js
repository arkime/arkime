/******************************************************************************/
/* apiHunts.js -- api calls for hunts tab
 *
 * Copyright Yahoo Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const Config = require('./config.js');
const Db = require('./db.js');
const async = require('async');
const RE2 = require('re2');
const util = require('util');
const Pcap = require('./pcap.js');
const ArkimeUtil = require('../common/arkimeUtil');
const Notifier = require('../common/notifier');
const User = require('../common/user');
const internals = require('./internals');
const ViewerUtils = require('./viewerUtils');
const SessionAPIs = require('./apiSessions');
const CronAPIs = require('./apiCrons');

class HuntAPIs {
  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
  static #packetSearch (packet, options) {
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
      console.log('ERROR - packetSearch: Invalid hunt search type.');
    }

    return found;
  }

  // --------------------------------------------------------------------------
  static #sessionHunt (sessionId, options, cb) {
    if (options.type === 'reassembled') {
      SessionAPIs.processSessionIdAndDecode(sessionId, options.size || 10000, (err, session, packets) => {
        if (err) {
          return cb(null, false);
        }

        let i = 0;
        let increment = 1;
        const len = packets.length;

        if (options.src && !options.dst) {
          increment = 2;
        } else if (options.dst && !options.src) {
          i = 1;
          increment = 2;
        }

        for (i; i < len; i += increment) {
          if (HuntAPIs.#packetSearch(packets[i].data, options)) { return cb(null, true); }
        }

        return cb(null, false);
      });
    } else if (options.type === 'raw') {
      const packets = [];
      SessionAPIs.processSessionId(sessionId, true, null, (pcap, buffer, processSessionIdCb, i) => {
        if (options.src === options.dst) {
          packets.push(buffer);
        } else {
          const packet = {};
          pcap.decode(buffer, packet);
          packet.data = buffer.slice(16);
          packets.push(packet);
        }
        processSessionIdCb(null);
      }, (err, session) => {
        if (err) {
          return; /* cb(null, false); */
        }

        const len = packets.length;
        if (options.src === options.dst) {
          // If search both src/dst don't need to check key
          for (let i = 0; i < len; i++) {
            if (HuntAPIs.#packetSearch(packets[i], options)) { return cb(null, true); }
          }
        } else {
          // If searching src NOR dst need to check key
          const skey = Pcap.keyFromSession(session);
          for (let i = 0; i < len; i++) {
            const key = Pcap.key(packets[i]);
            const isSrc = key === skey;
            if (options.src && isSrc) {
              if (HuntAPIs.#packetSearch(packets[i].data, options)) { return cb(null, true); }
            } else if (options.dst && !isSrc) {
              if (HuntAPIs.#packetSearch(packets[i].data, options)) { return cb(null, true); }
            }
          }
        }
        return cb(null, false);
      },
      options.size || 10000, 10);
    }
  }

  // --------------------------------------------------------------------------
  static #pauseHuntJobWithError (huntId, hunt, error, node) {
    let errorMsg = `${hunt.name} (${huntId}) hunt ERROR: ${error.value}.`;
    if (node) {
      errorMsg += ` On ${node} node`;
      error.node = node;
    }

    if (Config.debug) {
      console.log('HUNT - pauseHuntJobWithError', errorMsg);
    }

    error.time = Math.floor(Date.now() / 1000);

    hunt.status = 'paused';

    if (error.unrunnable) {
      delete error.unrunnable;
      hunt.unrunnable = true;
    }

    if (!hunt.errors) {
      hunt.errors = [error];
    } else {
      hunt.errors.push(error);
    }

    async function continueProcess () {
      try {
        await Db.setHunt(huntId, hunt);
        if (Config.debug) {
          console.log('HUNT - pauseHuntJobWithError - cleared running');
        }
        internals.runningHuntJob = undefined;
        HuntAPIs.processHuntJobs();
      } catch (err) {
        return console.log('ERROR - pauseHuntJobWithError - could not update hunt with errors:', util.inspect(err, false, 50));
      }
    }

    const message = `
*${hunt.name}* hunt job paused with error: *${error.value}*
*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions.
${Config.arkimeWebURL()}hunt
    `;

    Notifier.issueAlert(hunt.notifier, message, continueProcess);
  }

  // --------------------------------------------------------------------------
  static async #updateHuntStats (hunt, huntId, session, searchedSessions, cb) {
    // update the hunt with number of matchedSessions and searchedSessions
    // and the date of the first packet of the last searched session
    const lastPacketTime = session.lastPacket;
    const now = Math.floor(Date.now() / 1000);

    if ((now - hunt.lastUpdated) >= 2) { // only update every 2 seconds
      try {
        const { body: huntHit } = await Db.get('hunts', 'hunt', huntId);

        if (!huntHit) { return cb('undefined'); }

        hunt.status = huntHit._source.status;
        hunt.lastUpdated = now;
        hunt.searchedSessions = searchedSessions || hunt.searchedSessions;
        hunt.lastPacketTime = lastPacketTime;
        hunt.errors = huntHit._source.errors;

        Db.setHunt(huntId, hunt);

        if (hunt.status === 'paused' || hunt.status === 'finished') {
          return cb('paused');
        } else {
          return cb(null);
        }
      } catch (err) {
        const errorText = `Error finding hunt: ${hunt.name} (${huntId}): ${err}`;
        HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: errorText });
        return cb({ success: false, text: errorText });
      }
    } else {
      return cb(null);
    }
  }

  // --------------------------------------------------------------------------
  static #updateSessionWithHunt (session, sessionId, hunt, huntId) {
    Db.addHuntToSession(Db.sid2Index(sessionId), Db.sid2Id(sessionId), huntId, hunt.name, (err, data) => {
      if (err) {
        console.log('ERROR - updateSessionWithHunt - could not add hunt info to session:', session, util.inspect(err, false, 50), data);
      }
    });
  }

  // --------------------------------------------------------------------------
  static #buildHuntOptions (huntId, hunt) {
    const options = {
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
        HuntAPIs.#pauseHuntJobWithError(huntId, hunt, {
          value: `Fatal Error: Regex parse error. Fix this issue with your regex and create a new hunt: ${e}`,
          unrunnable: true
        });
      }
    }

    return options;
  }

  // --------------------------------------------------------------------------
  // if we couldn't retrieve the seession, skip it but add it to failedSessionIds
  // so that we can go back and search for it at the end of the hunt
  static #continueHuntSkipSession (hunt, huntId, session, sessionId, searchedSessions, cb) {
    if (!hunt.failedSessionIds) {
      hunt.failedSessionIds = [sessionId];
    } else {
      // pause the hunt if there are more than 10k failed sessions
      if (hunt.failedSessionIds.length > 10000) {
        return HuntAPIs.#pauseHuntJobWithError(huntId, hunt, {
          value: 'Error hunting: Too many sessions are unreachable. Please contact your administrator.'
        });
      }
      // make sure the session id is not already in the array
      // if it's not the first pass and a node is still down, this could be a duplicate
      if (hunt.failedSessionIds.indexOf(sessionId) < 0) {
        hunt.failedSessionIds.push(sessionId);
      }
    }

    return HuntAPIs.#updateHuntStats(hunt, huntId, session, searchedSessions, cb);
  }

  // --------------------------------------------------------------------------
  static async #updateHuntStatus (req, res, huntStatus, successText, errorText) {
    try {
      const { body: { _source: hunt } } = await Db.getHunt(req.params.id);
      // don't let a user play a hunt job if one is already running
      if (huntStatus === 'running' && internals.runningHuntJob) {
        return res.serverError(403, 'You cannot start a new hunt until the running job completes or is paused.');
      }

      // if hunt is finished, don't allow pause
      if (hunt.status === 'finished' && huntStatus === 'paused') {
        return res.serverError(403, 'You cannot pause a completed hunt.');
      }

      // clear the running hunt job if this is it
      if (hunt.status === 'running') { internals.runningHuntJob = undefined; }

      try {
        await Db.updateHunt(req.params.id, { status: huntStatus });
        res.send(JSON.stringify({ success: true, text: successText }));
        HuntAPIs.processHuntJobs();
      } catch (err) {
        console.log('ERROR - updateHuntStatus -', errorText, util.inspect(err, false, 50));
        return res.serverError(500, errorText);
      }
    } catch (err) {
      console.log('ERROR - updateHuntStatus -', errorText, util.inspect(err, false, 50));
      return res.serverError(500, errorText);
    }
  }

  // --------------------------------------------------------------------------
  // if there are failed sessions, go through them one by one and do a packet search
  // if there are no failed sessions left at the end then the hunt is done
  // if there are still failed sessions, but some sessions were searched during the last pass, try again
  // if there are still failed sessions, but no new sessions coudl be searched, pause the job with an error
  static #huntFailedSessions (hunt, huntId, options, searchedSessions, user) {
    if (!hunt.failedSessionIds && !hunt.failedSessionIds.length) { return; }

    let changesSearchingFailedSessions = false;

    options.searchingFailedSessions = true;
    // copy the failed session ids so we can remove them from the hunt
    // but still loop through them iteratively
    const failedSessions = JSON.parse(JSON.stringify(hunt.failedSessionIds));
    // we don't need to search the db for session, we just need to search each session in failedSessionIds
    async.forEachLimit(failedSessions, 3, (sessionId, cb) => {
      Db.getSession(sessionId, { arkime_unflatten: true, _source: false, fields: 'node,huntName,huntId,lastPacket,field'.split(',') }, (err, session) => {
        if (err) {
          return HuntAPIs.#continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
        }

        session = session.fields;

        const huntRemotePath = `api/hunt/${session.node}/${huntId}/remote/${sessionId}`;

        if (Config.debug > 1) {
          console.log('HUNT - remote', huntRemotePath);
        }
        ViewerUtils.makeRequest(session.node, huntRemotePath, user, (err, response) => {
          if (Config.debug > 1) {
            console.log('HUNT - remote response', huntRemotePath, err, response);
          }
          if (err) {
            return HuntAPIs.#continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
          }

          const json = JSON.parse(response);

          if (json.error) {
            console.log(`ERROR - huntFailedSessions - hunting on remote viewer: ${huntRemotePath}`, util.inspect(json.error, false, 50));
            return HuntAPIs.#continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
          }

          // remove from failedSessionIds if it was found
          hunt.failedSessionIds.splice(hunt.failedSessionIds.indexOf(sessionId), 1);
          // there were changes to this hunt; we're making progress
          changesSearchingFailedSessions = true;

          if (json.matched) { hunt.matchedSessions++; }
          return HuntAPIs.#updateHuntStats(hunt, huntId, session, searchedSessions, cb);
        });
      });
    }, (err) => { // done running a pass of the failed sessions
      async function continueProcess () {
        try {
          await Db.setHunt(huntId, hunt);
          if (Config.debug) {
            console.log('HUNT - huntFailedSessions - cleared running');
          }
          internals.runningHuntJob = undefined;
          HuntAPIs.processHuntJobs(); // start new hunt
        } catch (err) {
          console.log(`ERROR - huntFailedSessions - could not update hunt (${huntId})`, util.inspect(err, false, 50));
        }
      }

      if (hunt.failedSessionIds && hunt.failedSessionIds.length === 0) {
        options.searchingFailedSessions = false; // no longer searching failed sessions
        // we had failed sessions but we're done searching through them
        // so we're completely done with this hunt (inital search and failed sessions)
        hunt.status = 'finished';

        if (hunt.notifier) {
          const message = `
*${hunt.name}* hunt job finished:
*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions.
${Config.arkimeWebURL()}sessions?expression=huntId==${huntId}&stopTime=${hunt.query.stopTime}&startTime=${hunt.query.startTime}
          `;
          Notifier.issueAlert(hunt.notifier, message, continueProcess);
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
        return HuntAPIs.#pauseHuntJobWithError(huntId, hunt, {
          value: 'Error hunting previously unreachable sessions. There is likely a node down. Please contact your administrator.'
        });
      }
    });
  }

  // --------------------------------------------------------------------------
  // Actually do the search against ES and process the results.
  static #runHuntJob (huntId, hunt, query, user) {
    const options = HuntAPIs.#buildHuntOptions(huntId, hunt);
    let searchedSessions;

    // look for failed sessions if we're done searching sessions normally
    if (!options.searchingFailedSessions && hunt.searchedSessions === hunt.totalSessions && hunt.failedSessionIds && hunt.failedSessionIds.length) {
      options.searchingFailedSessions = true;
      return HuntAPIs.#huntFailedSessions(hunt, huntId, options, searchedSessions, user);
    }

    Db.searchSessions(Db.getSessionIndices(true), query, { scroll: internals.esScrollTimeout }, function getMoreUntilDone (err, result) {
      if (err || result.error) {
        HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: `Hunt error searching sessions: ${err}` });
        return;
      }

      const hits = result.hits.hits;
      if (searchedSessions === undefined) {
        searchedSessions = hunt.searchedSessions || 0;
        // if the session query results length is not equal to the total sessions that the hunt
        // job is searching, update the hunt total sessions so that the percent works correctly
        if (!options.searchingFailedSessions && hunt.totalSessions !== (result.hits.total + searchedSessions)) {
          hunt.totalSessions = result.hits.total + searchedSessions;
        }
      }

      async.forEachLimit(hits, 3, (hit, cb) => {
        searchedSessions++;
        const session = hit._source;
        const sessionId = Db.session2Sid(hit);
        const node = session.node;

        // There are no files, this is a fake session, don't hunt it
        if (session.fileId === undefined || session.fileId.length === 0) {
          return HuntAPIs.#updateHuntStats(hunt, huntId, session, searchedSessions, cb);
        }

        SessionAPIs.isLocalView(node, () => {
          HuntAPIs.#sessionHunt(sessionId, options, (err, matched) => {
            if (err) {
              return HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: `Hunt error searching session (${sessionId}): ${err}` }, node);
            }

            if (matched) {
              hunt.matchedSessions++;
              HuntAPIs.#updateSessionWithHunt(session, sessionId, hunt, huntId);
            }

            HuntAPIs.#updateHuntStats(hunt, huntId, session, searchedSessions, cb);
          });
        }, () => { // Check Remotely
          const huntRemotePath = `api/hunt/${node}/${huntId}/remote/${sessionId}`;

          if (Config.debug > 1) {
            console.log('HUNT - failed remote', huntRemotePath);
          }
          ViewerUtils.makeRequest(node, huntRemotePath, user, (err, response) => {
            if (Config.debug > 1) {
              console.log('HUNT - failed remote response', huntRemotePath, err, response);
            }
            if (err) {
              return HuntAPIs.#continueHuntSkipSession(hunt, huntId, session, sessionId, searchedSessions, cb);
            }
            const json = JSON.parse(response);
            if (json.error) {
              console.log(`ERROR - runHuntJob - hunting on remote viewer: ${huntRemotePath}`, util.inspect(json.error, false, 50));
              return HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: `Error hunting on remote viewer: ${json.error}` }, node);
            }
            if (json.matched) { hunt.matchedSessions++; }
            return HuntAPIs.#updateHuntStats(hunt, huntId, session, searchedSessions, cb);
          });
        });
      }, async (err) => { // done running this section of hunt job
        // Some kind of error, stop now
        if (err === 'paused' || err === 'undefined') {
          if (result && result._scroll_id) {
            Db.clearScroll({ body: { scroll_id: result._scroll_id } });
          }
          if (Config.debug) {
            console.log('HUNT - runHuntJob - cleared running', huntId, hunt.name);
          }
          internals.runningHuntJob = undefined;
          return;
        }

        // There might be more, issue another scroll
        if (result.hits.hits.length !== 0) {
          try {
            const { body: results } = await Db.scroll({
              body: { scroll_id: result._scroll_id }, scroll: internals.esScrollTimeout
            });
            return getMoreUntilDone(null, results);
          } catch (err) {
            console.log('ERROR - runHuntJob - issuing scroll for hunt job', util.inspect(err, false, 50));
            return getMoreUntilDone(err, {});
          }
        }

        Db.clearScroll({ body: { scroll_id: result._scroll_id } });

        hunt.searchedSessions = hunt.totalSessions;

        async function continueProcess () {
          try {
            await Db.setHunt(huntId, hunt);
            internals.runningHuntJob = undefined;
            if (Config.debug) {
              console.log('HUNT - runHuntJob - cleared running', huntId, hunt.name);
            }
            HuntAPIs.processHuntJobs(); // start new hunt or go back over failedSessionIds
          } catch (err) {
            console.log(`ERROR - runHuntJob - updating hunt (${huntId})`, util.inspect(err, false, 50));
          }
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
          const message = `
*${hunt.name}* hunt job finished:
*${hunt.matchedSessions}* matched sessions out of *${hunt.searchedSessions}* searched sessions.
${Config.arkimeWebURL()}sessions?expression=huntId==${huntId}&stopTime=${hunt.query.stopTime}&startTime=${hunt.query.startTime}
          `;

          Notifier.issueAlert(hunt.notifier, message, continueProcess);
        } else {
          return continueProcess();
        }
      });
    });
  }

  // --------------------------------------------------------------------------
  // Do the house keeping before actually running the hunt job
  static #processHuntJob (huntId, hunt) {
    const now = Math.floor(Date.now() / 1000);

    hunt.lastUpdated = now;
    if (!hunt.started) { hunt.started = now; }

    try {
      Db.setHunt(huntId, hunt);
    } catch (err) {
      HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: `Error starting hunt job: ${err}` });
    }

    ViewerUtils.getUserCacheIncAnon(hunt.userId, (err, user) => {
      if (err && !user) {
        HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: err });
        return;
      }
      if (!user) {
        HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: `User ${hunt.userId} doesn't exist` });
        return;
      }
      if (!user.enabled) {
        HuntAPIs.#pauseHuntJobWithError(huntId, hunt, { value: `User ${hunt.userId} is not enabled` });
        return;
      }

      const fakeReq = {
        user,
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

      SessionAPIs.buildSessionQuery(fakeReq, (err, query, indices) => {
        if (err) {
          HuntAPIs.#pauseHuntJobWithError(huntId, hunt, {
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
            console.log('HUNT -', hunt.name, hunt.userId, '- start:', new Date(hunt.lastPacketTime || hunt.query.startTime * 1000), 'stop:', new Date(hunt.query.stopTime * 1000));
          }

          // do sessions query
          HuntAPIs.#runHuntJob(huntId, hunt, query, user);
        });
      });
    });
  }

  // --------------------------------------------------------------------------
  // Kick off the process of running a hunt job
  // cb is optional and is called either when a job has been started or end of function
  static async processHuntJobs (cb) {
    if (!CronAPIs.isPrimaryViewer()) {
      return;
    }

    if (internals.runningHuntJob) {
      if (Config.debug) {
        console.log('HUNT - processing hunt jobs already', internals.runningHuntJob?.name);
      }
      return (cb ? cb() : null);
    }
    internals.runningHuntJob = true;

    if (Config.debug) {
      console.log('HUNT - processing hunt jobs');
    }

    const query = {
      size: 10000,
      sort: { created: { order: 'asc' } },
      query: { terms: { status: ['queued', 'paused', 'running'] } }
    };

    try {
      const { body: { hits: hunts } } = await Db.searchHunt(query);
      for (const hit of hunts.hits) {
        const hunt = hit._source;
        const id = hit._id;

        // there is a job already running
        if (hunt.status === 'running') {
          internals.runningHuntJob = hunt;
          if (!internals.proccessHuntJobsInitialized) {
            internals.proccessHuntJobsInitialized = true;
            // restart the abandoned or incomplete hunt
            HuntAPIs.#processHuntJob(id, hunt);
          }
          return (cb ? cb() : null);
        } else if (hunt.status === 'queued') { // get the first queued hunt
          internals.runningHuntJob = hunt;
          hunt.status = 'running'; // update the hunt job
          HuntAPIs.#processHuntJob(id, hunt);
          return (cb ? cb() : null);
        }
      }

      // Made to the end without starting a job
      internals.proccessHuntJobsInitialized = true;
      internals.runningHuntJob = undefined;
      return (cb ? cb() : null);
    } catch (err) {
      console.log('ERROR - processHuntJobs - fetching hunt jobs', util.inspect(err, false, 50));
      return (cb ? cb() : null);
    }
  };

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * A packet search job that allows users to search within session packets for text.
   * @typedef Hunt
   * @type {object}
   * @param {SessionsQuery} See_List - This API supports a common set of parameters documented in the SessionsQuery section
   * @property {string} userId - The ID of the user that created the hunt.
   * @property {string} status - The status of the hunt. Options include:
     queued - The hunt is queued to search packets once the currently running hunt has finished.
     running - The hunt is currently searching packets.
     paused - The hunt is paused, either by a user or by error.
     finished - The hunt has searched all requested sessions.
   * @property {string} name - The name of the hunt (not unique).
   * @property {number} size - The number of packets to search within each session.
   * @property {string} search - The search text to search for within packets.
   * @property {string} searchType - What type of search the text is. Options include:
     ascii - search for case insensitive ascii text.
     asciicase - search for case sensitive ascii text.
     hex - search for hex text.
     regex - search for text using <a href="https://github.com/google/re2/wiki/Syntax">safe regex</a>.
     hexregex - search for text using <a href="https://github.com/google/re2/wiki/Syntax">safe hex regex</a>.
   * @property {boolean} src - Whether to search the source packets. Must search src or dst or both.
   * @property {boolean} dst - Whether to search the destination packets. Must search src or dst or both.
   * @property {string} type - Whether to search raw or reassembled packets.
   * @property {number} matchedSessions - How many sessions contain packets that match the search text.
   * @property {number} searchedSessions - How many sessions have had their packets searched.
   * @property {number} totalSessions - The number of sessions to search.
   * @property {number} lastPacketTime - The date of the first packet of the last searched session. Used to query for the next chunk of sessions to search. Format is seconds since Unix EPOC.
   * @property {number} created - The time that the hunt was created. Format is seconds since Unix EPOC.
   * @property {number} lastUpdated - The time that the hunt was last updated in the DB. Used to only update every 2 seconds. Format is seconds since Unix EPOC.
   * @property {number} started - The time that the hunt was started (put into running state). Format is seconds since Unix EPOC.
   * @property {array} errors - The list of errors that a hunt encountered. A hunt error includes:
     value - The error text to display to the user.
     time - The time the error was encountered.
     node - The Arkime node that the hunt was searching sessions for when the error occurred.
   * @property {string} notifier - The otional notifier name to fire when there is an error, or there are matches (every 10 minutes), or when the hunt is complete.
   * @property {boolean} unrunnable - Whether an error has rendered the hunt unrunnable.
   * @property {array} failedSessionIds - The list of sessions that have failed to be searched. Used to run the search against them again once the rest of the hunt is complete.
   * @property {array} users - The list of users to be added to the hunt so they can view the results.
   * @property {boolean} removed - Whether the hunt name and ID fields have been removed from the matched sessions.
   */

  // --------------------------------------------------------------------------
  /**
   * POST - /api/hunt
   *
   * Creates a new hunt.
   * @name /hunt
   * @param {SessionsQuery} See_List - This API supports a common set of parameters documented in the SessionsQuery section
   * @param {number} totalSessions - The number of sessions to search.
   * @param {string} name - The name of the hunt (not unique).
   * @param {number} size - The number of packets to search within each session.
   * @param {boolean} src - Whether to search the source packets. Must search src or dst or both.
   * @param {boolean} dst - Whether to search the destination packets. Must search src or dst or both.
   * @param {string} type - Whether to search raw or reassembled packets.
   * @param {string} search - The search text to search for within packets.
   * @param {string} searchType - What type of search the text is. Options include:
     ascii - search for case insensitive ascii text.
     asciicase - search for case sensitive ascii text.
     hex - search for hex text.
     regex - search for text using <a href="https://github.com/google/re2/wiki/Syntax">safe regex</a>.
     hexregex - search for text using <a href="https://github.com/google/re2/wiki/Syntax">safe hex regex</a>.
   * @param {string} notifier - The optional notifier name to fire when there is an error, or there are matches (every 10 minutes), or when the hunt is complete.
   * @param {string} users - The comma separated list of users to be added to the hunt so they can view the results.
   * @returns {boolean} success - Whether the creation of the hunt was successful.
   * @returns {Hunt} hunt - The newly created hunt object.
   * @returns {array} invalidUsers - The list of users that could not be added to the hunt because they were invalid or nonexitent.
   */
  static async createHunt (req, res) {
    // make sure all the necessary data is included in the post body
    if (!req.body.totalSessions) { return res.serverError(403, 'This hunt does not apply to any sessions'); }
    if (!ArkimeUtil.isString(req.body.name)) { return res.serverError(403, 'Missing hunt name'); }
    if (!req.body.size) { return res.serverError(403, 'Missing max mumber of packets to examine per session'); }
    if (!ArkimeUtil.isString(req.body.search)) { return res.serverError(403, 'Missing packet search text'); }
    if (!req.body.src && !req.body.dst) {
      return res.serverError(403, 'The hunt must search source or destination packets (or both)');
    }
    if (!req.body.query) { return res.serverError(403, 'Missing query'); }
    if (req.body.query.startTime === undefined || req.body.query.stopTime === undefined) {
      return res.serverError(403, 'Missing fully formed query (must include start time and stop time)');
    }

    const searchTypes = ['ascii', 'asciicase', 'hex', 'regex', 'hexregex'];
    if (!ArkimeUtil.isString(req.body.searchType)) {
      return res.serverError(403, 'Missing packet search text type');
    } else if (searchTypes.indexOf(req.body.searchType) === -1) {
      return res.serverError(403, 'Improper packet search text type. Must be "ascii", "asciicase", "hex", "hexregex", or "regex"');
    }

    if (!ArkimeUtil.isString(req.body.type)) {
      return res.serverError(403, 'Missing packet search type (raw or reassembled packets)');
    } else if (req.body.type !== 'raw' && req.body.type !== 'reassembled') {
      return res.serverError(403, 'Improper packet search type. Must be "raw" or "reassembled"');
    }

    const limit = req.user.hasRole('arkimeAdmin') ? Config.get('huntAdminLimit', 10000000) : Config.get('huntLimit', 1000000);
    if (parseInt(req.body.totalSessions) > limit) {
      return res.serverError(403, `This hunt applies to too many sessions. Narrow down your session search to less than ${limit} first.`);
    }

    if (req.body.roles !== undefined && !ArkimeUtil.isStringArray(req.body.roles)) {
      return res.serverError(403, 'Roles field must be an array of strings');
    }

    if (req.body.users !== undefined && !ArkimeUtil.isString(req.body.users, 0)) {
      return res.serverError(403, 'Users field must be a string');
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
      },
      description: req.body.description,
      roles: req.body.roles
    };

    async function doneCb (doneHunt, invalidUsers) {
      try {
        const { body: result } = await Db.createHunt(doneHunt);
        doneHunt.id = result._id;
        HuntAPIs.processHuntJobs(() => {
          const response = {
            success: true,
            hunt: doneHunt
          };

          if (invalidUsers) {
            response.invalidUsers = invalidUsers;
          }

          return res.send(JSON.stringify(response));
        });
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/hunt`, util.inspect(err, false, 50));
        return res.serverError(500, 'Error creating hunt');
      }
    }

    if (!req.body.users || !req.body.users.length) {
      return doneCb(hunt);
    }

    const reqUsers = ArkimeUtil.commaOrNewlineStringToArray(req.body.users);

    try {
      const users = await User.validateUserIds(reqUsers);
      hunt.users = users.validUsers;
      // dedupe the array of users
      hunt.users = [...new Set(hunt.users)];
      return doneCb(hunt, users.invalidUsers);
    } catch (err) {
      return res.serverError(500, err);
    }
  };

  // --------------------------------------------------------------------------
  /**
   * GET - /api/hunts
   *
   * Retrieves a list of hunts.
   * @name /hunts
   * @param {string} searchTerm - The search text to search hunt results for.
   * @param {number} length=10000 - The number of items to return. Defaults to 10000.
   * @param {number} start=0 - The entry to start at. Defaults to 0
   * @param {string} sortField=created - The field to sort the hunt results by. Defaults to "created".
   * @param {string} desc=false - Whether to sort the results in descending order. Default is ascending.
   * @param {string} history=false - Whether to return only finished hunts. Default is to return queued, paused, and running hunts.
   * @returns {Hunt} runningJob - If there is a hunt running, returns the currently running hunt object.
   * @returns {Hunt[]} data - The list of hunts (either finished or queued/paused/running).
   * @returns {number} recordsTotal - The total number of hunts Arkime has.
   * @returns {number} recordsFiltered - The number of hunts returned in this result.
   */
  static getHunts (req, res) {
    const query = {
      sort: {},
      from: parseInt(req.query.start) || 0,
      size: parseInt(req.query.length) || 10000,
      query: { bool: { filter: [] } }
    };

    query.sort[req.query.sortField || 'created'] = { order: req.query.desc === 'true' ? 'desc' : 'asc' };

    if (req.query.history) { // only get finished jobs
      query.query.bool.filter.push({ term: { status: 'finished' } });
      if (req.query.searchTerm) { // apply search term
        query.query.bool.filter.push({
          query_string: {
            query: req.query.searchTerm,
            fields: ['name', 'userId']
          }
        });
      }
    } else { // get queued, paused, running jobs
      query.from = 0;
      query.size = 1000;
      if (req.query.all === undefined) {
        query.query.bool.filter.push({ terms: { status: ['queued', 'paused', 'running'] } });
      }
    }

    if (Config.debug) {
      console.log('Hunt query:', JSON.stringify(query, null, 2));
    }

    Promise.all([
      Db.searchHunt(query),
      Db.countHunts(),
      Db.getQueriesNode()
    ]).then(([{ body: { hits: hunts } }, { body: { count: total } }, nodeInfo]) => {
      let runningJob;

      const results = { total: hunts.total, results: [] };
      for (let i = 0, ilen = hunts.hits.length; i < ilen; i++) {
        const hit = hunts.hits[i];
        const hunt = hit._source;
        hunt.id = hit._id;
        hunt.index = hit._index;
        hunt.users = hunt.users || [];

        // clear out secret fields for users who don't have access to that hunt
        // if the user is not an admin and didn't create the hunt and isn't part of the user's list
        if (!req.user.hasRole('arkimeAdmin') && req.user.userId !== hunt.userId && hunt.users.indexOf(req.user.userId) < 0) {
          if (!hunt.roles || (hunt.roles.length && !req.user.hasRole(hunt.roles))) {
            // since hunt isn't cached we can just modify
            hunt.id = '';
            hunt.search = '';
            hunt.userId = '';
            hunt.searchType = '';
            delete hunt.query;
          }
        }

        // don't add the running job to the queue
        if (hunt.status === 'running' && (!CronAPIs.isPrimaryViewer() || internals.runningHuntJob)) {
          runningJob = hunt;
          if (req.query.all !== undefined) {
            continue;
          }
        }
        results.results.push(hunt);
      }

      res.send({
        recordsTotal: total,
        recordsFiltered: results.total,
        data: results.results,
        runningJob,
        nodeInfo
      });
    }).catch(err => {
      console.log(`ERROR - ${req.method} /api/hunts`, util.inspect(err, false, 50));
      return res.serverError(500, 'Error retrieving hunts');
    });
  };

  // --------------------------------------------------------------------------
  /**
   * DELETE - /api/hunt/:id
   *
   * Delete a hunt.
   * @name /hunt/:id
   * @returns {boolean} success - Whether the delete hunt operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async deleteHunt (req, res) {
    try {
      await Db.deleteHunt(req.params.id);
      return res.send(JSON.stringify({
        success: true,
        text: 'Deleted hunt successfully'
      }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/hunt/%s`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error deleting hunt');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/hunt/:id/cancel
   *
   * Cancel a hunt. Finishes the hunt and puts it into the hunt history.
   * @name /hunt/:id/cancel
   * @returns {boolean} success - Whether the cancel hunt operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async cancelHunt (req, res) {
    try {
      const { body: { _source: hunt } } = await Db.getHunt(req.params.id);

      const error = { // save that the user canceled the hunt
        time: Math.floor(Date.now() / 1000),
        value: `${req.user.userId} canceled hunt.`
      };

      if (!hunt.errors) {
        hunt.errors = [error];
      } else {
        hunt.errors.push(error);
      }

      await Db.updateHunt(req.params.id, { status: 'finished', errors: hunt.errors });
      internals.runningHuntJob = undefined;
      HuntAPIs.processHuntJobs();
      return res.send(JSON.stringify({ success: true, text: 'Canceled hunt successfully' }));
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/hunt/%s/cancel`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Error canceling hunt');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/hunt/:id/pause
   *
   * Pause a hunt.
   * @name /hunt/:id/pause
   * @returns {boolean} success - Whether the pause hunt operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static pauseHunt (req, res) {
    HuntAPIs.#updateHuntStatus(req, res, 'paused', 'Paused hunt successfully', 'Error pausing hunt');
  };

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/hunt/:id/play
   *
   * Play a hunt.
   * @name /hunt/:id/play
   * @returns {boolean} success - Whether the play hunt operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static playHunt (req, res) {
    HuntAPIs.#updateHuntStatus(req, res, 'queued', 'Queued hunt successfully', 'Error starting hunt');
  };

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/hunt/:id/removefromsessions
   *
   * Remove the hunt ID and name from matched sessions.
   * @name /hunt/:id/removefromsessions
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async removeFromSessions (req, res) {
    try {
      const { body: { _source: hunt } } = await Db.getHunt(req.params.id);

      if (!hunt.matchedSessions) {
        return res.serverError(202, 'Nothing to do: this hunt has not matched any sessions.');
      }

      const fakeReq = {
        user: req.user,
        query: {
          from: 0,
          size: hunt.matchedSessions, // only fetch number of sessions that matched the hunt
          _source: ['_id', 'node', 'huntId', 'huntName']
        }
      };

      fakeReq.query.expression = `huntId == ${req.params.id}`;

      SessionAPIs.buildSessionQuery(fakeReq, (err, query, indices) => {
        if (err) {
          return res.serverError(500, 'Unable to build sessions query to fetch sessions that matched this hunt.');
        }

        query.query.bool.filter[0] = {
          range: {
            lastPacket: {
              gte: hunt.query.startTime * 1000,
              lt: hunt.query.stopTime * 1000
            }
          }
        };

        Db.searchSessions(indices, query, {}, async (err, result) => {
          if (err) {
            return res.serverError(500, 'Unable to fetch sessions that matched this hunt.');
          }

          // iterate through sessions and remove hunt stuff from each one
          for (const hit of result.hits.hits) {
            Db.removeHuntFromSession(hit._index, hit._id, req.params.id, hunt.name, () => {});
          }

          await Db.updateHunt(req.params.id, { removed: true });

          return res.send({ success: true, text: 'Succesfully removed the hunt name and ID from the matched sessions.' });
        });
      });
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/hunt/%s/removefromsessions`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Unable to remove hunt name and ID from the matched sessions.');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * PUT - /api/hunt/:id
   *
   * Update a hunt - can only update description & roles
   * @name /hunt/:id
   * @param {Hunt} hunt - The new hunt data
   * @returns {boolean} success - Whether the operation was successful.
   * @returns {string} text - The success/error message to (optionally) display to the user.
   */
  static async updateHunt (req, res) {
    try {
      const { body: { _source: hunt } } = await Db.getHunt(req.params.id);

      // update properties
      if (ArkimeUtil.isString(req.body.description)) {
        hunt.description = req.body.description;
      }

      if (ArkimeUtil.isStringArray(req.body.roles)) {
        hunt.roles = req.body.roles;
      }

      try {
        await Db.setHunt(req.params.id, hunt);
        res.send(JSON.stringify({
          success: true,
          text: 'Updated Hunt Succesfully!'
        }));
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/hunt/%s (setHunt)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.serverError(500, 'Unable to update hunt');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/hunt/%s/users (getHunt)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Unable update hunt');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * POST - /api/hunt/:id/users
   *
   * Add user(s) to a hunt.
   * @name /hunt/:id/users
   * @param {string} users - Comma separated list of user ids to add to the hunt.
   * @returns {boolean} success - Whether the add users operation was successful.
   * @returns {array} users - The list of users that were added to the hunt.
   * @returns {array} invalidUsers - The list of users that could not be added to the hunt because they were invalid or nonexitent.
   */
  static async addUsers (req, res) {
    if (!ArkimeUtil.isString(req.body.users)) {
      return res.serverError(403, 'You must provide users in a comma separated string');
    }

    try {
      const { body: { _source: hunt } } = await Db.getHunt(req.params.id);

      const reqUsers = ArkimeUtil.commaOrNewlineStringToArray(req.body.users);

      try {
        const users = await User.validateUserIds(reqUsers);
        if (!users.validUsers.length) {
          return res.serverError(404, 'Unable to validate user IDs provided');
        }

        if (!hunt.users) {
          hunt.users = users.validUsers;
        } else {
          hunt.users = hunt.users.concat(users.validUsers);
        }

        // dedupe the array of users
        hunt.users = [...new Set(hunt.users)];

        try {
          await Db.updateHunt(req.params.id, { users: hunt.users });
          res.send(JSON.stringify({
            success: true,
            users: hunt.users,
            invalidUsers: users.invalidUsers
          }));
        } catch (err) {
          console.log(`ERROR - ${req.method} /api/hunt/%s/users (updateHunt)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
          return res.serverError(500, 'Unable to add user(s)');
        }
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/hunt/%s/users (validateUserIds)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
        return res.serverError(500, err);
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/hunt/%s/users (getHunt)`, ArkimeUtil.sanitizeStr(req.params.id), util.inspect(err, false, 50));
      return res.serverError(500, 'Unable to add user(s)');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * DELETE - /api/hunt/:id/user/:user
   *
   * Remove user(s) from a hunt.
   * @name /hunt/:id/user/:user
   * @returns {boolean} success - Whether the remove users operation was successful.
   * @returns {array} users - The list of users who have access to the hunt.
   * @returns {array} invalidUsers - The list of users that could not be removed from the hunt because they were invalid or nonexitent.
   */
  static async removeUsers (req, res) {
    try {
      const { body: { _source: hunt } } = await Db.getHunt(req.params.id);

      if (!hunt.users || !hunt.users.length) {
        return res.serverError(404, 'There are no users that have access to view this hunt');
      }

      const userIdx = hunt.users.indexOf(req.params.user);

      if (userIdx < 0) { // user doesn't have access to this hunt
        return res.serverError(404, 'That user does not have access to this hunt');
      }

      hunt.users.splice(userIdx, 1); // remove the user from the list

      try {
        await Db.updateHunt(req.params.id, { users: hunt.users });
        res.send(JSON.stringify({ success: true, users: hunt.users }));
      } catch (err) {
        console.log(`ERROR - ${req.method} /api/hunt/%s/user/%s (updateHunt)`, ArkimeUtil.sanitizeStr(req.params.id), ArkimeUtil.sanitizeStr(req.params.user), util.inspect(err, false, 50));
        return res.serverError(500, 'Unable to remove user');
      }
    } catch (err) {
      console.log(`ERROR - ${req.method} /api/hunt/%s/user/%s (getHunt)`, ArkimeUtil.sanitizeStr(req.params.id), ArkimeUtil.sanitizeStr(req.params.user), util.inspect(err, false, 50));
      return res.serverError(500, 'Unable to remove user');
    }
  };

  // --------------------------------------------------------------------------
  /**
   * @ignore
   * GET - /api/hunt/:nodeName/:huntId/remote/:sessionId
   *
   * Searches a session on a remote node.
   * @name /:nodeName/hunt/:huntId/remote/:sessionId
   * @returns {boolean} matched - Whether searching the session packets resulted in a match with the search text.
   * @returns {string} error - If an error occurred, describes the error.
   */
  static async remoteHunt (req, res) {
    const huntId = req.params.huntId;
    const sessionId = req.params.sessionId;

    if (Config.debug > 1) {
      console.log('HUNT - incoming', huntId, sessionId);
    }

    // fetch hunt and session
    Promise.all([
      Db.get('hunts', 'hunt', huntId),
      Db.getSessionPromise(sessionId)
    ]).then(([{ body: hunt }, session]) => {
      if (hunt.error || session.error) {
        console.log('HUNT - remoteHunt error', hunt.error || session.error);
        if (!res.headersSent) { res.send({ matched: false }); }
        return;
      }

      hunt = hunt._source;
      session = session._source;

      const options = HuntAPIs.#buildHuntOptions(huntId, hunt);

      HuntAPIs.#sessionHunt(sessionId, options, (err, matched) => {
        if (Config.debug > 1) {
          console.log('HUNT - result', huntId, sessionId, err, matched);
        }
        if (err) {
          if (!res.headersSent) { res.send({ matched: false, error: err }); }
          return;
        }

        if (matched) {
          HuntAPIs.#updateSessionWithHunt(session, sessionId, hunt, huntId);
        }

        if (!res.headersSent) { res.send({ matched }); }
      });
    }).catch((err) => {
      console.log(`ERROR - ${req.method} /api/hunt/%s/%s/remote/%s`, ArkimeUtil.sanitizeStr(req.params.nodeName), ArkimeUtil.sanitizeStr(req.params.huntId), ArkimeUtil.sanitizeStr(req.params.sessionId), util.inspect(err, false, 50));
      if (!res.headersSent) { res.send({ matched: false, error: err }); }
    });
  };
};

module.exports = HuntAPIs;
