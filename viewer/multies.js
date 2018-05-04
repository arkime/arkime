/******************************************************************************/
/* multies.js  -- Make multiple ES servers look like one but merging results
 *
 * Copyright 2012-2016 AOL Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this Software except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*jshint
  node: true, plusplus: false, curly: true, eqeqeq: true, immed: true, latedef: true, newcap: true, nonew: true, undef: true, strict: true, trailing: true
*/
'use strict';


//// Modules
//////////////////////////////////////////////////////////////////////////////////
try {
var Config         = require('./config.js'),
    express        = require('express'),
    async          = require('async'),
    os             = require('os'),
    util           = require('util'),
    URL            = require('url'),
    ESC            = require('elasticsearch'),
    http           = require('http'),
    https          = require('https');
} catch (e) {
  console.log ("ERROR - Couldn't load some dependancies, maybe need to 'npm update' inside viewer directory", e);
  process.exit(1);
}

var clients = {};
var nodes = [];
var httpAgent  =  new http.Agent({keepAlive: true, keepAliveMsecs:5000, maxSockets: 100});
var httpsAgent =  new https.Agent({keepAlive: true, keepAliveMsecs:5000, maxSockets: 100});

function hasBody(req) {
  var encoding = 'transfer-encoding' in req.headers;
  var length = 'content-length' in req.headers && req.headers['content-length'] !== '0';
  return encoding || length;
}

function saveBody (req, res, next) {
  if (req._body) {return next();}
  req.body = req.body || {};

  if (!hasBody(req)) {return next();}

  // flag as parsed
  req._body = true;

  // parse
  var buf = '';
  req.setEncoding('utf8');
  req.on('data', (chunk) => { buf += chunk; });
  req.on('end', () => {
    req.body = buf;
    next();
  });
}

// app.configure
var logger = require("morgan");
var favicon = require("serve-favicon");
var compression = require('compression');

var app = express();
app.enable("jsonp callback");
app.use(favicon(__dirname + '/public/favicon.ico'));
app.use(logger(':date \x1b[1m:method\x1b[0m \x1b[33m:url\x1b[0m :res[content-length] bytes :response-time ms'));
app.use(saveBody);
app.use(compression());
app.use(function(req, res, next) {
  if (res.setTimeout) {
    res.setTimeout(10 * 60 * 1000); // Increase default from 2 min to 10 min
    return next();
  }
});

function node2Url(node) {
  var url = node.split(',')[0];
  if (url.match(/^http/)) {return url;}
  return "http://" + url;
}

function node2Prefix(node) {
  var parts = node.split(',');
  for (var p = 1; p < parts.length; p++) {
    var kv = parts[p].split(':');
    if (kv[0] === "prefix") {
      if (kv[1].charAt(kv[1].length-1) !== "_") {
        return kv[1] + "_";
      }
      return kv[1];
    }
  }
  return "";
}

function simpleGather(req, res, bodies, doneCb) {
  async.map(nodes, (node, asyncCb) => {
    var result = "";
    var url = node2Url(node) + req.url;
    var prefix = node2Prefix(node);

    url = url.replace(/MULTIPREFIX_/g, prefix);
    var info = URL.parse(url);
    info.method = req.method;
    var client;
    if (url.match(/^https:/)) {
      info.agent  = httpsAgent;
      client = https;
    } else {
      info.agent  = httpAgent;
      client = http;
    }
    var preq = client.request(info, (pres) => {
      pres.on('data', (chunk) => {
        result += chunk.toString();
      });
      pres.on('end', () => {
        if (result.length) {
          result = result.replace(new RegExp('(index":\s*|[,{]|  )"' + prefix + "(sessions2|sessions|stats|dstats|sequence|files|users|history)", "g"), "$1\"MULTIPREFIX_$2");
          result = result.replace(new RegExp('(index":\s*)"' + prefix + "(fields_v[12])\"", "g"), "$1\"MULTIPREFIX_$2\"");
          result = JSON.parse(result);
        } else {
          result = {};
        }
        result._node = node;
        asyncCb(null, result);
      });
    });
    preq.setHeader('content-type', "application/json");
    if (req._body) {
      if (bodies && bodies[node]) {
        preq.end(bodies[node]);
      } else {
        preq.end(req.body);
      }
    }
    preq.on('error', (e) => {
      console.log("Request error with node", node, e);
    });
    preq.end();
  }, doneCb);
}

function shallowCopy(obj1, obj2) {
  for (var attrname in obj2) {
    obj1[attrname] = obj2[attrname];
  }
}

function deepCopy(obj) {
  return JSON.parse(JSON.stringify(obj));
}

function shallowAdd(obj1, obj2) {
  for (var attrname in obj2) {
    if (typeof obj2[attrname] === "number") {
      obj1[attrname] += obj2[attrname];
    }
  }
}

function simpleGatherCopy(req, res) {
  simpleGather(req, res, null, (err, results) => {
    var obj = results[0];
    for (var i = 1; i < results.length; i++) {
      shallowCopy(obj.nodes, results[i].nodes);
    }
    res.send(obj);
  });
}

function simpleGatherAdd(req, res) {
  simpleGather(req, res, null, (err, results) => {
    var obj = results[0];
    for (var i = 1; i < results.length; i++) {
      shallowAdd(obj, results[i]);
    }
    obj.cluster_name = "COMBINED";
    res.send(obj);
  });
}

function simpleGatherFirst(req, res) {
  simpleGather(req, res, null, (err, results) => {
    res.send(results[0]);
  });
}

app.get("/_cluster/nodes/stats", simpleGatherCopy);
app.get("/_nodes/stats", simpleGatherCopy);
app.get("/_nodes/stats/:kinds", simpleGatherCopy);
app.get("/_cluster/health", simpleGatherAdd);

app.get("/:index/_aliases", simpleGatherCopy);
app.get("/:index/_alias", simpleGatherCopy);

app.get("/:index/_status", (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    var obj = results[0];
    for (var i = 1; i < results.length; i++) {
      for (var index in results[i].indices) {
        if (obj.indices[index]) {
          obj.indices[index].docs.num_docs += results[i].indices[index].docs.num_docs;
          obj.indices[index].docs.max_doc += results[i].indices[index].docs.max_doc;
        } else {
          obj.indices[index] = results[i].indices[index];
        }
      }
    }
    res.send(obj);
  });
});

app.get("/:index/_stats", (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    //console.log("DEBUG - _stats results", util.inspect(results, false, 50));
    var obj = results[0];
    for (var i = 1; i < results.length; i++) {
      for (var index in results[i].indices) {
        if (obj.indices[index]) {
          obj.indices[index].total.docs.count += results[i].indices[index].total.docs.count;
          obj.indices[index].total.docs.deleted += results[i].indices[index].total.docs.deleted;
        } else {
          obj.indices[index] = results[i].indices[index];
        }
      }
    }
    res.send(obj);
  });
});

app.get("/_template/MULTIPREFIX_sessions2_template", (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    //console.log("DEBUG -", util.inspect(results, false, 50));

    var obj = results[0];
    for (var i = 1; i < results.length; i++) {
      if (results[i].MULTIPREFIX_sessions2_template.mappings.session._meta.molochDbVersion < obj.MULTIPREFIX_sessions2_template.mappings.session._meta.molochDbVersion) {
        obj = results[i];
      }
    }
    res.send(obj);
  });
});

app.get("/users/user/:user", (req, res) => {
  clients[nodes[0]].get({index: "users", type: "user", id: req.params.user}, (err, result) => {
    res.send(result);
  });
});

app.get("/:index/:type/_search", (req, res) => {
  simpleGather(req, res, null, (err, results) => {
    var obj = results[0];
    for (var i = 1; i < results.length; i++) {
      if (results[i].error) {
        console.log("ERROR - GET _search", req.query.index, req.query.type,  results[i].error);
      }
      obj.hits.total += results[i].hits.total;
      obj.hits.hits = obj.hits.hits.concat(results[i].hits.hits);
    }
    res.send(obj);
  });
});

app.get("/:index/:type/:id", function(req, res) {
  simpleGather(req, res, null, (err, results) => {
    for (var i = 0; i < results.length; i++) {
      if (results[i].found) {
        return res.send(results[i]);
      }
    }
    res.send(results[0]);
  });
});

app.get("/_cluster/settings", function(req, res) {
  res.send({persistent: {}, transient: {}});
});


app.head(/^\/$/, function(req, res) {
  res.send("");
});

app.get(/^\/$/, function(req, res) {
  simpleGather(req, res, null, (err, results) => {
    res.send(results[0]);
  });
});

app.get(/./, function(req, res) {
  simpleGather(req, res, null, (err, results) => {
    console.log("UNKNOWN", req.method, req.url, results);
  });

});


// Facets are a pain, we convert from array to map to back to array
//////////////////////////////////////////////////////////////////////////////////

function facet2Obj(field, facet) {
  var obj = {};
  for (var i = 0; i < facet.length; i++) {
    obj[facet[i][field]] = facet[i];
  }
  return obj;
}

function facet2Arr(facet, type) {
  var arr = [];
  for (var attrname in facet) {
    arr.push(facet[attrname]);
  }

  if (type === "histogram") {
    arr = arr.sort((a,b) => {return a.key - b.key;});
  } else {
    arr = arr.sort((a,b) => {return b.count - a.count;});
  }
  return arr;
}

function facetConvert2Obj(facets) {
  for (var facetname in facets) {
    if (facets[facetname]._type === "histogram") {
      facets[facetname].entries = facet2Obj("key", facets[facetname].entries);
    } else if (facets[facetname]._type === "terms") {
      facets[facetname].terms = facet2Obj("term", facets[facetname].terms);
    } else {
      console.log("Unknown facet type", facets[facetname]._type);
    }
  }
}

function facetConvert2Arr(facets) {
  for (var facetname in facets) {
    var facetarray = facets[facetname]._type === "histogram"?"entries":"terms";
    facets[facetname][facetarray]= facet2Arr(facets[facetname][facetarray], facets[facetname]._type);

  }
}

function facetAdd(obj1, obj2) {
  for (var facetname in obj2) {
    var facetarray = obj1[facetname]._type === "histogram"?"entries":"terms";

    obj1[facetname].total += obj2[facetname].total;
    obj1[facetname].missing += obj2[facetname].missing;
    obj1[facetname].other += obj2[facetname].other;

    for (var entry in obj2[facetname][facetarray]) {
      if (!obj1[facetname][facetarray][entry]) {
        obj1[facetname][facetarray][entry] = obj2[facetname][facetarray][entry];
      } else {
        var o1 = obj1[facetname][facetarray][entry];
        var o2 = obj2[facetname][facetarray][entry];

        o1.count += o2.count;
        if (o1.total) {
          o1.total += o2.total;
        }
      }
    }
  }
}

// Aggregations are a pain, we convert from array to map to back to array
//////////////////////////////////////////////////////////////////////////////////

function agg2Obj(field, agg) {
  var obj = {};
  for (var i = 0; i < agg.length; i++) {
    obj[agg[i][field]] = agg[i];
  }
  return obj;
}

function agg2Arr(agg, type) {
  var arr = [];
  for (var attrname in agg) {
    arr.push(agg[attrname]);
  }

  if (type === "histogram") {
    arr = arr.sort((a,b) => {return a.key - b.key;});
  } else {
    arr = arr.sort((a,b) => {
      if (b.doc_count !== a.doc_count) {
        return b.doc_count - a.doc_count;
      }
      return a.key - b.key;
    });
  }
  return arr;
}

function aggConvert2Obj(aggs) {
  for (var aggname in aggs) {
    aggs[aggname].buckets = agg2Obj("key", aggs[aggname].buckets);
  }
}

function aggConvert2Arr(aggs) {
  for (var aggname in aggs) {
    aggs[aggname].buckets = agg2Arr(aggs[aggname].buckets, aggs[aggname]._type);
    delete aggs[aggname]._type;
  }
}

function aggAdd(obj1, obj2) {
  for (var aggname in obj2) {
    obj1[aggname].doc_count_error_upper_bound += obj2[aggname].doc_count_error_upper_bound;
    obj1[aggname].sum_other_doc_count += obj2[aggname].sum_other_doc_count;
    for (var entry in obj2[aggname].buckets) {
      if (!obj1[aggname].buckets[entry]) {
        obj1[aggname].buckets[entry] = obj2[aggname].buckets[entry];
      } else {
        var o1 = obj1[aggname].buckets[entry];
        var o2 = obj2[aggname].buckets[entry];

        o1.doc_count += o2.doc_count;
        if (o1.db) {
          o1.db.value += o2.db.value;
          o1.pa.value += o2.pa.value;
        }
      }
    }
  }
}

function fixQuery(node, body, doneCb) {
  body = JSON.parse(body);

  // Reset from & size since we do aggregation
  if (body.size) {
    body.size = (+body.size) + ((+body.from) || 0);
  }
  body.from = 0;

  var outstanding = 0;
  var finished = 0;
  var err = null;

  var convert;

  function process(parent, obj, item) {
    var query;

    if (item === "fileand" && typeof obj[item] === "string") {
      var name = obj.fileand;
      delete obj.fileand;
      outstanding++;

      if (name[0] === "/" && name[name.length - 1] === "/") {
        query = {query: {regexp: {name: name.substring(1, name.length-1)}}};
      } else if (name.indexOf("*") !== -1) {
        query = {query: {wildcard: {name: name}}};
      } else {
        query = {query: {term: {name: name}}};
      }
      clients[node].search({index: node2Prefix(node) + 'files', type: 'file', size:500, body: query}, (err, result) => {
        outstanding--;
        obj.bool = {should: []};
        result.hits.hits.forEach((file) => {
          obj.bool.should.push({bool: {must:[{term: {node: file._source.node}}, {term: {fileId: file._source.num}}]}});
        });
        if (obj.bool.should.length === 0) {
          err = "No matching files found";
        }
        if (finished && outstanding === 0) {
          doneCb(err, body);
        }
      });
    } else if (typeof obj[item] === "object") {
      convert(obj, obj[item]);
    }
  }

  convert = function (parent, obj) {
    for (var item in obj) {
      process(parent, obj, item);
    }
  };

  convert(null, body);
  if (outstanding === 0) {
    return doneCb(err, body);
  }

  finished = 1;
}

function combineResults(obj, result) {
  if (!result.hits) {
    console.log("NO RESULTS", result);
    return;
  }

  obj.hits.total += result.hits.total;
  obj.hits.missing += result.hits.missing;
  obj.hits.other += result.hits.other;
  if (result.hits.hits) {
    obj.hits.hits = obj.hits.hits.concat(result.hits.hits);
  }
  if (obj.facets) {
    facetConvert2Obj(result.facets);
    facetAdd(obj.facets, result.facets);
  }

  if (obj.aggregations) {
    aggConvert2Obj(result.aggregations);
    aggAdd(obj.aggregations, result.aggregations);
  }
}

function sortResults(search, obj) {
  // Resort items
  if (search.sort && search.sort.length > 0) {
    var sortorder = [];
    for (var i = 0; i < search.sort.length; i++) {
      sortorder[i] = search.sort[i][Object.keys(search.sort[i])[0]].order === "asc"? 1:-1;
    }

    obj.hits.hits = obj.hits.hits.sort((a, b) => {
      for (var i = 0; i < a.sort.length; i++) {
        if (a.sort[i] === b.sort[i]) {
          continue;
        }
        if (typeof a.sort[i] === "string") {
          return sortorder[i] * a.sort[i].localeCompare(b.sort[i]);
        } else {
          return sortorder[i] * (a.sort[i] - b.sort[i]);
        }
      }
      return 0;
    });
  }

  if (search.size) {
    var from = +search.from || 0;
    obj.hits.hits = obj.hits.hits.slice(from, from + (+search.size));
  }
}
function newResult(search) {
  var result = {hits: {hits: [], total: 0}};
  if (search.facets) {
    result.facets = {};
    for (var facet in search.facets) {
      if (search.facets[facet].histogram) {
        result.facets[facet] = {entries: [], _type: "histogram", total: 0, other: 0, missing: 0};
      } else {
        result.facets[facet] = {terms: [], _type: "terms", total: 0, other: 0, missing: 0};
      }
    }
  }
  if (search.aggregations) {
    result.aggregations = {};
    for (var agg in search.aggregations) {
      if (search.aggregations[agg].histogram) {
        result.aggregations[agg] = {buckets: {}, _type: "histogram", doc_count_error_upper_bound: 0, sum_other_doc_count: 0};
      } else {
        result.aggregations[agg] = {buckets: {}, _type: "terms", doc_count_error_upper_bound: 0, sum_other_doc_count: 0};
      }
    }
  }
  return result;
}

app.post("/MULTIPREFIX_fields/field/_search", function(req, res) {
  simpleGather(req, res, null, (err, results) => {
    var obj = {
      hits: {
        total: 0,
        hits: [
        ]
      }
    };
    var unique = {};
    for (var i = 0; i < results.length; i++) {
      var result = results[i];

      if (result.error) {
        console.log("ERROR - GET /fields/field/_search", result.error);
      }

      for (var h = 0; h < result.hits.total; h++) {
        var hit = result.hits.hits[h];
        if (!unique[hit._id]) {
          unique[hit._id] = 1;
          obj.hits.total++;
          obj.hits.hits.push(hit);
        }
      }
    }
    res.send(obj);
  });
});

app.post("/:index/:type/_search", function(req, res) {
  var bodies = {};
  var search = JSON.parse(req.body);
  //console.log("DEBUG - INCOMING SEARCH", util.inspect(search, false, 50));

  var doField = search.aggregations &&
                search.aggregations.field &&
                search.aggregations.field.terms &&
                search.aggregations.field.terms.field.match(/^(ta|hh1|hh2)$/);

  async.each(nodes, (node, asyncCb) => {
    fixQuery(node, req.body, (err, body) => {
      //console.log("DEBUG - OUTGOING SEARCH", node, util.inspect(body, false, 50));
      bodies[node] = JSON.stringify(body);
      asyncCb(null);
    });
  }, (err) => {
    simpleGather(req, res, bodies, (err, results) => {
      var obj = newResult(search);

      for (var i = 0; i < results.length; i++) {
        combineResults(obj, results[i]);
      }

      if (obj.facets) {
        facetConvert2Arr(obj.facets);
      }

      if (obj.aggregations) {
        aggConvert2Arr(obj.aggregations);
      }

      sortResults(search, obj);

      res.send(obj);
    });
  });
});

function msearch(req, res) {
  var lines = req.body.split(/[\r\n]/);
  var bodies = {};

  async.each(nodes, (node, nodeCb) => {
    var nlines = [];
    async.eachSeries(lines, (line, lineCb) => {
      if (line === "{}" || line === "") {
        nlines.push("{}");
        return lineCb();
      }
      fixQuery(node, line, (err, body) => {
        nlines.push(JSON.stringify(body));
        lineCb();
      });
    }, (err) => {
      bodies[node] = nlines.join("\n") + "\n";
      var prefix = node2Prefix(node);
      bodies[node] = bodies[node].replace(/MULTIPREFIX_/g, prefix);
      nodeCb();
    });
  }, (err) => {
    var responses = [];
    simpleGather(req, res, bodies, (err, results) => {
      var obj = {responses:[]};
      for (var h = 0; h < results[0].responses.length; h++) {
        obj.responses[h] = newResult(JSON.parse(lines[h*2+1]));

        for (var i = 0; i < results.length; i++) {
          combineResults(obj.responses[h], results[i].responses[h]);
        }

        if (obj.responses[h].facets) {
          facetConvert2Arr(obj.responses[h].facets);
        }

        if (obj.responses[h].aggregations) {
          aggConvert2Arr(obj.responses[h].aggregations);
        }

        sortResults(JSON.parse(lines[h*2+1]), obj.responses[h]);
      }

      res.send(obj);
    });
  });
}

app.post("/:index/history", simpleGatherFirst);

app.post("/:index/:type/_msearch", msearch);
app.post("/_msearch", msearch);

app.get("/:index/_count", simpleGatherAdd);
app.post("/:index/_count", simpleGatherAdd);
app.get("/:index/:type/_count", simpleGatherAdd);
app.post("/:index/:type/_count", simpleGatherAdd);


if (Config.get("regressionTests")) {
  app.post('/shutdown', function(req, res) {
    process.exit(0);
    throw new Error("Exiting");
  });
}

app.post(/./, function(req, res) {
  console.log("UNKNOWN", req.method, req.url, req.body);
});

//////////////////////////////////////////////////////////////////////////////////
//// Main
//////////////////////////////////////////////////////////////////////////////////

nodes = Config.get("multiESNodes", "").split(";");
if (nodes.length === 0 || nodes[0] === "") {
  console.log("ERROR - Empty multiESNodes");
  process.exit(1);
}

// First connect
nodes.forEach((node) => {
  clients[node] = new ESC.Client({
    host: node.split(",")[0],
    apiVersion: "5.5",
    requestTimeout: 300000,
    keepAlive: true
  });
});

// Now check version numbers
nodes.forEach((node) => {
  clients[node].info((err,data) => {
    if (err) {
      console.log(err);
    }
    if (data.version.number.match(/^([01234])/)) {
      console.log("ES", data.version.number, "is not supported, upgrade to >= 5.5.x:", node);
      process.exit();
    }
  });
});

console.log(nodes);

console.log("Listen on ", Config.get("multiESPort", "8200"));
var server = http.createServer(app).listen(Config.get("multiESPort", "8200"), Config.get("multiESHost", undefined));
