'use strict';

module.exports = (Config, Db, sessionAPIs, ViewerUtils) => {
  const module = {};

  // --------------------------------------------------------------------------
  // HELPERS
  // --------------------------------------------------------------------------
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

    getUserCacheIncAnon(hunt.userId, (err, user) => {
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

  // --------------------------------------------------------------------------
  // APIs
  // --------------------------------------------------------------------------
  /**
   * POST - /api/hunt
   *
   * Creates a new hunt (packet search job).
   * @ignore
   * @name /hunt
   TODO ECR DOCUMENT!!!!!!!!
   * @param {SessionsQuery} query - The request query to filter sessions
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

    const searchTypes = [ 'ascii', 'asciicase', 'hex', 'wildcard', 'regex', 'hexregex' ];
    if (!req.body.searchType) {
      return res.molochError(403, 'Missing packet search text type');
    } else if (searchTypes.indexOf(req.body.searchType) === -1) {
      return res.molochError(403, 'Improper packet search text type. Must be "ascii", "asciicase", "hex", "wildcard", "hexregex", or "regex"');
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

    validateUserIds(reqUsers).then((response) => {
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
