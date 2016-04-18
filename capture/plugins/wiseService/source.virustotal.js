/******************************************************************************/
/*
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
'use strict';

var request        = require('request')
  , wiseSource     = require('./wiseSource.js')
  , util           = require('util')
  , LRU            = require('lru-cache')
  ;

//////////////////////////////////////////////////////////////////////////////////
function VirusTotalSource (api, section) {
  VirusTotalSource.super_.call(this, api, section);
  this.waiting    = [];
  this.outgoing   = 0;
  this.inprogress = 0;
  this.cached     = 0;
}
util.inherits(VirusTotalSource, wiseSource);

//////////////////////////////////////////////////////////////////////////////////
VirusTotalSource.prototype.performQuery = function () {
  var self = this;

  if (self.waiting.length === 0) {
    return;
  }

  if (self.api.debug > 0) {
    console.log("VirusTotal - Fetching %d", self.waiting.length);
  }

  self.outgoing++;

  var options = {
      url: 'https://www.virustotal.com/vtapi/v2/file/report?',
      qs: {apikey: self.key,
           resource: self.waiting.join(",")},
      method: 'GET',
      json: true
  };

  self.waiting.length = 0;


  var req = request(options, function(err, im, results) {
    if (err) {
      console.log("Error parsing for request:\n", options, "\nresults:\n", results);
      results = [];
    } 
    if (!Array.isArray(results)) {
      results = [results];
    }

    results.forEach(function(result) {
      var info = self.cache.get(result.resource);
      if (!info) {
        return;
      }
      if (result.response_code === 0) {
        info.result = wiseSource.emptyResult;
      }  else {
        var args = [self.hitsField, ""+result.positives, self.linksField, result.permalink];

        for(var i = 0; i < self.dataSources.length; i++) {
          var uc = self.dataSources[i];
          var lc = self.dataSourcesLC[i];

          if (result.scans[uc] && result.scans[uc].detected) {
            args.push(self.dataFields[i], result.scans[uc].result);
          }
        }

        info.result = {num: args.length/2, buffer: wiseSource.encode.apply(null, args)};
      }

      var cb;
      while ((cb = info.cbs.shift())) {
        cb(null, info.result);
      }
    });
  }).on('error', function (err) {
    console.log(err);
  });
};
//////////////////////////////////////////////////////////////////////////////////
VirusTotalSource.prototype.init = function() {
  this.key = this.api.getConfig("virustotal", "key");
  if (this.key === undefined) {
    console.log("VirusTotal - No key defined");
    return;
  }

  this.dataSources = ["McAfee", "Symantec", "Microsoft", "Kaspersky"];
  this.dataSourcesLC = this.dataSources.map(function(x) {return x.toLowerCase();});
  this.dataFields = [];

  this.cache = LRU({max: this.api.getConfig("virustotal", "cacheSize", 200000), 
                      maxAge: 1000 * 60 * +this.api.getConfig("virustotal", "cacheAgeMin", "60")});

  this.api.addSource("virustotal", this);
  setInterval(this.performQuery.bind(this), 500);

  var str = 
    "if (session.virustotal)\n" +
    "  div.sessionDetailMeta.bold VirusTotal\n" +
    "  dl.sessionDetailMeta\n";

  for(var i = 0; i < this.dataSources.length; i++) {
    var uc = this.dataSources[i];
    var lc = this.dataSourcesLC[i];
    this.dataFields[i] = this.api.addField("field:virustotal." + lc + ";db:virustotal." + lc + "-term;kind:lotermfield;friendly:" + uc + ";help:VirusTotal " + uc + " Status;count:true");
    str += "    +arrayList(session.virustotal, '" + lc + "-term', '" + uc + "', 'virustotal." + lc + "')\n";
  }

  this.hitsField = this.api.addField("field:virustotal.hits;db:virustotal.hits;kind:integer;friendly:Hits;help:VirusTotal Hits;count:true");
  this.linksField = this.api.addField("field:virustotal.links;db:virustotal.links-term;kind:termfield;friendly:Link;help:VirusTotal Link;count:true");


  str += "    +arrayList(session.virustotal, 'hits', 'Hits', 'virustotal.hits')\n"
  str += "    +arrayList(session.virustotal, 'links', 'Links', 'virustotal.links')\n"

  this.api.addView("virustotal", str);
};

VirusTotalSource.prototype.getMd5 = function(md5, cb) {
  var info = this.cache.get(md5);
  console.log("Looking for", md5);
  if (info) {
    if (info.result !== undefined) {
      this.cached++;
      return cb(null, info.result);
    }
    this.inprogress++;
    info.cbs.push(cb);
    return;
  }
  info = {cbs:[cb]};
  this.cache.set(md5, info);
  this.waiting.push(md5);
  if (this.waiting.length >= 25) {
    this.performQuery();
  }
};
//////////////////////////////////////////////////////////////////////////////////
var reportApi = function(req, res) {
  source.getMd5(req.query.resource, function(err, result) {
    //console.log(err, result);
    if(result.num === 0) {
      res.send({response_code: 0, resource: req.query.resource, verbose_msg: "The requested resource is not among the finished, queued or pending scans"});
    } else {
      var obj = {scans:{}};
      var offset = 0;
      for (var i = 0; i < result.num; i++) {
        var pos   = result.buffer[offset];
        var len   = result.buffer[offset+1];
        var value = result.buffer.toString('utf8', offset+2, offset+2+len-1);
        offset += 2 + len;
        switch (pos) {
        case source.hitsField:
          obj.positives = +value;
          break;
        case source.linksField:
          obj.permalink = value;
          break;
        default:
          for (var j = 0; j < source.dataFields.length; j++) {
            if (source.dataFields[j] === pos) {
              obj.scans[source.dataSources[j]] = {detected: true, result: value};
              break;
            }
          }
        }
      }
      res.send(obj);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
VirusTotalSource.prototype.dump = function(res) {
  this.cache.forEach(function(value, key, cache) {
    if (value.result) {
      var str = "{key: \"" + key + "\", ops:\n" + 
        wiseSource.result2Str(wiseSource.combineResults([value.result])) + "},\n";
      res.write(str);
    }
  });
  res.end();
};
//////////////////////////////////////////////////////////////////////////////////
VirusTotalSource.prototype.printStats = function() {
  console.log("VirusTotal: outgoing:", this.outgoing, "cached:", this.cached, "inprogress:", this.inprogress, "size:", this.cache.itemCount);
};
//////////////////////////////////////////////////////////////////////////////////
var source;
exports.initSource = function(api) {
  api.app.get("/vtapi/v2/file/report", reportApi);
  source = new VirusTotalSource(api, "virustotal");
  source.init();
};
//////////////////////////////////////////////////////////////////////////////////
