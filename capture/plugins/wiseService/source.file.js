/******************************************************************************/
/*
 *
 * Copyright 2012-2014 AOL Inc. All rights reserved.
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

var fs             = require('fs')
  ;

var internals = {};
//////////////////////////////////////////////////////////////////////////////////
function createImpl(section) {
  var api = internals.api;

  var impl = {
    file: api.getConfig(section, "file"),
    column: +api.getConfig(section, "column", 0),
    section: section,
    cache: {},
    load: function() {
      var self = this;
      fs.readFile(self.file, function (err, data) {
        if (!err) {
          self.parse(data, self, function(err, cache) {
            if (err) {
              console.log("ERROR loading", self.section, err);
              return;
            }
            self.cache = cache;
            var cnt = 0;
            for (var key in self.cache) {
              cnt++;
            }
            console.log(section, "- Done Loading", cnt, "elements");
          });
        } else {
          console.log("Couldn't load", section, self.file, err);
        }
      });
    },
    dump: function(res) {
      var cache = this.cache;
      for (var key in cache) {
        var str = "{key: \"" + key + "\", ops:\n" + 
          internals.api.result2Str(internals.api.combineResults([impl.result, cache[key]])) + "},\n";
        res.write(str);
      }
      res.end();
    },
    sendResult: function(key, cb) {
      var result = this.cache[key];

      // Not found, or found but no extra values to add
      if (!result) {
        return cb(null, undefined);
      }
      if (result.num === 0) {
        return cb(null, this.result);
      }

      // Found, so combine the two results (per item, and per source)
      var newresult = {num: result.num + this.result.num, buffer: Buffer.concat([result.buffer, this.result.buffer])};
      return cb(null, newresult);
    }
  };

  if (impl.file === undefined) {
    console.log("FILE - ERROR not loading", section, "since no file specified in config file");
    return null;
  }

  if (!fs.existsSync(impl.file)) {
    console.log("FILE - ERROR not loading", section, "since", impl.file, "doesn't exist");
    return null;
  }

  impl.type = api.getConfig(section, "type");
  if (!impl.type) {
    console.log("FILE - ERROR not loading", section, "since no type specified in config file");
    return null;
  }

  var tagsField = api.addField("field:tags");
  var tags = api.getConfig(section, "tags");
  if (!tags) {
    console.log("FILE - ERROR not loading", section, "since no tags specified in config file");
    return null;
  }
  var args = [];
  tags.split(",").forEach(function (part) {
    args.push(tagsField, part);
  });
  impl.result = {num: args.length/2, buffer: internals.api.encode.apply(null, args)};


  impl.format = api.getConfig(section, "format", "csv");
  if (impl.format === "csv") {
    impl.parse = api.parseCSV
  } else if (impl.format === "tagger") {
    impl.parse = api.parseTagger
  } else {
    console.log("FILE - ERROR not loading", section, "unknown data format", impl.format);
    return null;
  }

  if (impl.type === "domain") {
    impl.getDomain = function(domain, cb) {
      if (this.cache[domain]) {
        return impl.sendResult(domain, cb)
      }
      domain = domain.substring(domain.indexOf(".")+1);
      return impl.sendResult(domain, cb)
    };
  } else if (impl.type === "ip") {
    impl.getIp = impl.sendResult;
  } else if (impl.type === "md5") {
    impl.getMd5 = impl.sendResult;
  } else if (impl.type === "email") {
    impl.getEmail = impl.sendResult;
  } else {
    console.log("FILE - ERROR not loading", section, "since unknown type specified in config file", impl.type);
    return null;
  }

  setImmediate(impl.load.bind(impl));


  // Watch file for changes, have to do the 100 because of vim moving file
  impl.watch = fs.watch(impl.file, function watchCb(event, filename) {
    if (event === "rename") {
      impl.watch.close();
      setTimeout(function () {
        impl.load();
        impl.watch = fs.watch(impl.file, watchCb);
      }, 100);
    } else {
      impl.load();
    }
  });

  return impl;
}
//////////////////////////////////////////////////////////////////////////////////
exports.initSource = function(api) {
  internals.api = api;

  var sections = api.getConfigSections().filter(function(e) {return e.match(/^file:/);});
  sections.forEach(function(section) {
    var impl = createImpl(section);
    if (impl) {
      api.addSource(section, impl);
    }
  });
};
//////////////////////////////////////////////////////////////////////////////////
