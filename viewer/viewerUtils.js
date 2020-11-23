'use strict';

module.exports = (async, Db, Config, molochparser, internals) => {
  let module = {};

  module.queryValueToArray = (val) => {
     if (val === undefined || val === null) {
       return [];
     }
     if (!Array.isArray(val)) {
       val = [val];
     }
     return val.join(',').split(',');
   };

   // -------------------------------------------------------------------------
   // determineQueryTimes(reqQuery)
   //
   // Returns [startTimeSec, stopTimeSec, interval] using values from
   // reqQuery.date, reqQuery.startTime, reqQuery.stopTime, reqQuery.interval,
   // and reqQuery.segments.
   //
   // This code was factored out from buildSessionQuery.
   // -------------------------------------------------------------------------
   module.determineQueryTimes = (reqQuery) => {
     let startTimeSec = null;
     let stopTimeSec = null;
     let interval = 60 * 60;

     if (Config.debug) {
       console.log('determineQueryTimes <-', reqQuery);
     }

     if ((reqQuery.date && reqQuery.date === '-1') ||
         (reqQuery.segments && reqQuery.segments === 'all')) {
       interval = 60 * 60; // Hour to be safe
     } else if ((reqQuery.startTime !== undefined) && (reqQuery.stopTime !== undefined)) {
       if (!/^-?[0-9]+$/.test(reqQuery.startTime)) {
         startTimeSec = Date.parse(reqQuery.startTime.replace('+', ' ')) / 1000;
       } else {
         startTimeSec = parseInt(reqQuery.startTime, 10);
       }

       if (!/^-?[0-9]+$/.test(reqQuery.stopTime)) {
         stopTimeSec = Date.parse(reqQuery.stopTime.replace('+', ' ')) / 1000;
       } else {
         stopTimeSec = parseInt(reqQuery.stopTime, 10);
       }

       const diff = reqQuery.stopTime - reqQuery.startTime;
       if (diff < 30 * 60) {
         interval = 1; // second
       } else if (diff <= 5 * 24 * 60 * 60) {
         interval = 60; // minute
       } else {
         interval = 60 * 60; // hour
       }
     } else {
       let queryDate = reqQuery.date || 1;
       startTimeSec = (Math.floor(Date.now() / 1000) - 60 * 60 * parseInt(queryDate, 10));
       stopTimeSec = Date.now() / 1000;

       if (queryDate <= 5 * 24) {
         interval = 60; // minute
       } else {
         interval = 60 * 60; // hour
       }
     }

     switch (reqQuery.interval) {
     case 'second':
       interval = 1;
       break;
     case 'minute':
       interval = 60;
       break;
     case 'hour':
       interval = 60 * 60;
       break;
     case 'day':
       interval = 60 * 60 * 24;
       break;
     case 'week':
       interval = 60 * 60 * 24 * 7;
       break;
     }

     if (Config.debug) {
       console.log('determineQueryTimes ->', 'startTimeSec', startTimeSec, 'stopTimeSec', stopTimeSec, 'interval', interval);
     }

     return [startTimeSec, stopTimeSec, interval];
   };

   /* This method fixes up parts of the query that jison builds to what ES actually
    * understands.  This includes mapping all the tag fields from strings to numbers
    * and any of the filename stuff
    */
   module.lookupQueryItems = (query, doneCb) => {
     if (Config.get('multiES', false)) {
       return doneCb(null);
     }

     let outstanding = 0;
     let finished = 0;
     let err = null;

     function process (parent, obj, item) {
       // console.log("\nprocess:\n", item, obj, typeof obj[item], "\n");
       if (item === 'fileand' && typeof obj[item] === 'string') {
         const name = obj.fileand;
         delete obj.fileand;
         outstanding++;
         Db.fileNameToFiles(name, function (files) {
           outstanding--;
           if (files === null || files.length === 0) {
             err = "File '" + name + "' not found";
           } else if (files.length > 1) {
             obj.bool = { should: [] };
             files.forEach(function (file) {
               obj.bool.should.push({ bool: { must: [{ term: { node: file.node } }, { term: { fileId: file.num } }] } });
             });
           } else {
             obj.bool = { must: [{ term: { node: files[0].node } }, { term: { fileId: files[0].num } }] };
           }
           if (finished && outstanding === 0) {
             doneCb(err);
           }
         });
       } else if (item === 'field' && obj.field === 'fileand') {
         obj.field = 'fileId';
       } else if (typeof obj[item] === 'object') {
         convert(obj, obj[item]);
       }
     }

     function convert (parent, obj) {
       for (let item in obj) {
         process(parent, obj, item);
       }
     }

     convert(null, query);
     if (outstanding === 0) {
       return doneCb(err);
     }

     finished = 1;
   };

   module.continueBuildQuery = (req, query, err, finalCb, queryOverride = null) => {
     // queryOverride can supercede req.query if specified
     let reqQuery = queryOverride || req.query;

     if (!err && req.user.expression && req.user.expression.length > 0) {
       try {
         // Expression was set by admin, so assume email search ok
         molochparser.parser.yy.emailSearch = true;
         const userExpression = molochparser.parse(req.user.expression);
         query.query.bool.filter.push(userExpression);
       } catch (e) {
         console.log(`ERROR - Forced expression (${req.user.expression}) doesn't compile -`, e);
         err = e;
       }
     }

     module.lookupQueryItems(query.query.bool.filter, function (lerr) {
       if (reqQuery.date === '-1' || // An all query
           Config.get('queryAllIndices', Config.get('multiES', false))) { // queryAllIndices (default: multiES)
         return finalCb(err || lerr, query, 'sessions2-*'); // Then we just go against all indices for a slight overhead
       }

       Db.getIndices(reqQuery.startTime, reqQuery.stopTime, reqQuery.bounding, Config.get('rotateIndex', 'daily'), function (indices) {
         if (indices.length > 3000) { // Will url be too long
           return finalCb(err || lerr, query, 'sessions2-*');
         } else {
           return finalCb(err || lerr, query, indices);
         }
       });
     });
   };

   module.mapMerge = (aggregations) => {
     let map = { src: {}, dst: {}, xffGeo: {} };

     if (!aggregations || !aggregations.mapG1) {
       return {};
     }

     aggregations.mapG1.buckets.forEach(function (item) {
       map.src[item.key] = item.doc_count;
     });

     aggregations.mapG2.buckets.forEach(function (item) {
       map.dst[item.key] = item.doc_count;
     });

     aggregations.mapG3.buckets.forEach(function (item) {
       map.xffGeo[item.key] = item.doc_count;
     });

     return map;
   };

   module.graphMerge = (req, query, aggregations) => {
     let filters = req.user.settings.timelineDataFilters || internals.settingDefaults.timelineDataFilters;

     let graph = {
       xmin: req.query.startTime * 1000 || null,
       xmax: req.query.stopTime * 1000 || null,
       interval: query.aggregations ? query.aggregations.dbHisto.histogram.interval / 1000 || 60 : 60,
       sessionsHisto: [],
       sessionsTotal: 0
     };

     // allowed tot* data map
     let filtersMap = {
       'totPackets': ['srcPackets', 'dstPackets'],
       'totBytes': ['srcBytes', 'dstBytes'],
       'totDataBytes': ['srcDataBytes', 'dstDataBytes']
     };

     for (let i = 0; i < filters.length; i++) {
       let filter = filters[i];
       if (filtersMap[filter] !== undefined) {
         for (const j of filtersMap[filter]) {
           graph[j + 'Histo'] = [];
         }
       } else {
         graph[filter + 'Histo'] = [];
       }

       graph[filters[i] + 'Total'] = 0;
     }

     if (!aggregations || !aggregations.dbHisto) {
       return graph;
     }

     aggregations.dbHisto.buckets.forEach(function (item) {
       let key = item.key;

       // always add session information
       graph.sessionsHisto.push([key, item.doc_count]);
       graph.sessionsTotal += item.doc_count;

       for (let prop in item) {
         // excluding every item prop that isnt a summed up aggregate collection (ie. es keys)
         // tot* filters are exceptions: they will pass src/dst histo [], but keep a *Total count for filtered total
         // ie. totPackets selected filter => {srcPacketsHisto: [], dstPacketsHisto:[], totPacketsTotal: n, ...}
         if (filters.includes(prop) ||
           prop === 'srcPackets' || prop === 'dstPackets' || prop === 'srcBytes' ||
           prop === 'dstBytes' || prop === 'srcDataBytes' || prop === 'dstDataBytes') {
           // Note: prop will never be one of the chosen tot* exceptions
           graph[prop + 'Histo'].push([key, item[prop].value]);

           // Need to specify for when src/dst AND tot* filters are chosen
           if (filters.includes(prop)) {
             graph[prop + 'Total'] += item[prop].value;
           }

           // Add src/dst to tot* counters.
           if ((prop === 'srcPackets' || prop === 'dstPackets') && filters.includes('totPackets')) {
             graph.totPacketsTotal += item[prop].value;
           } else if ((prop === 'srcBytes' || prop === 'dstBytes') && filters.includes('totBytes')) {
             graph.totBytesTotal += item[prop].value;
           } else if ((prop === 'srcDataBytes' || prop === 'dstDataBytes') && filters.includes('totDataBytes')) {
             graph.totDataBytesTotal += item[prop].value;
           }
         }
       }
     });

     return graph;
   };

   /**
    * Flattens fields that are objects (only goes 1 level deep)
    *
    * @example
    * { http: { statuscode: [200, 302] } } => { "http.statuscode": [200, 302] }
    * @example
    * { cert: [ { alt: ["test.com"] } ] } => { "cert.alt": ["test.com"] }
    *
    * @param {object} fields The object containing fields to be flattened
    * @returns {object} fields The object with fields flattened
    */
   module.flattenFields = (fields) => {
     let newFields = {};

     for (let key in fields) {
       if (fields.hasOwnProperty(key)) {
         let field = fields[key];
         let baseKey = key + '.';
         if (typeof field === 'object' && !field.length) {
           // flatten out object
           for (let nestedKey in field) {
             if (field.hasOwnProperty(nestedKey)) {
               let nestedField = field[nestedKey];
               let newKey = baseKey + nestedKey;
               newFields[newKey] = nestedField;
             }
           }
           fields[key] = null;
           delete fields[key];
         } else if (Array.isArray(field)) {
           // flatten out list
           for (let nestedField of field) {
             if (typeof nestedField === 'object') {
               for (let nestedKey in nestedField) {
                 let newKey = baseKey + nestedKey;
                 if (newFields[newKey] === undefined) {
                   newFields[newKey] = nestedField[nestedKey];
                 } else if (Array.isArray(newFields[newKey])) {
                   newFields[newKey].push(nestedField[nestedKey]);
                 } else {
                   newFields[newKey] = [newFields[newKey], nestedField[nestedKey]];
                 }
               }
               fields[key] = null;
               delete fields[key];
             }
           }
         }
       }
     }

     for (let key in newFields) {
       if (newFields.hasOwnProperty(key)) {
         fields[key] = newFields[key];
       }
     }

     return fields;
   };

   module.fixFields = (fields, fixCb) => {
     if (!fields.fileId) {
       fields.fileId = [];
       return fixCb(null, fields);
     }

     let files = [];
     async.forEachSeries(fields.fileId, function (item, cb) {
       Db.fileIdToFile(fields.node, item, function (file) {
         if (file && file.locked === 1) {
           files.push(file.name);
         }
         cb(null);
       });
     },
     function (err) {
       fields.fileId = files;
       fixCb(err, fields);
     });
   };

   module.errorString = (err, result) => {
     let str;
     if (err && typeof err === 'string') {
       str = err;
     } else if (err && typeof err.message === 'string') {
       str = err.message;
     } else if (result && result.error) {
       str = result.error;
     } else {
       str = 'Unknown issue, check logs';
       console.log(err, result);
     }

     if (str.match('IndexMissingException')) {
       return "Arkime's Elasticsearch database has no matching session indices for the timeframe selected.";
     } else {
       return 'Elasticsearch error: ' + str;
     }
   };

   return module;
};
