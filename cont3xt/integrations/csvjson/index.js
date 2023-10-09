const Integration = require('../../integration.js');
const ArkimeConfig = require('../../../common/arkimeConfig');
const ArkimeUtil = require('../../../common/arkimeUtil');
const fs = require('fs');
const csv = require('csv');
const axios = require('axios');
const iptrie = require('iptrie');

class CsvJsonIntegration extends Integration {
  // Integration Items
  name;
  icon;
  order;
  itypes = {};

  static #order = 50000;

  settings = {
    disabled: {
      help: 'Disable integration for all queries',
      type: 'boolean'
    }
  };

  card = {
    fields: [
    ]
  };

  // CsvJson Items
  #load; // Function pointer
  #parse; // Function pointer
  #url;
  #redisClient;
  #redisKey;
  #doIp; // process should setup ip cache
  #doOther; // process should setup other cache
  #ipCache;
  #otherCache;
  #keyColumn; // csv - column with the key
  #arrayPath; // json - path to the json array
  #keyPath; // json - path to the key inside each item in json array
  #watch; // file - fs.watch
  #watchTimer; // file - change made, now wait to see if more changes
  #init = true; // not file - initial load
  #reload; // not file - reload interval in min

  constructor (section, isCSV) {
    super();

    this.section = section;
    this.name = ArkimeConfig.getFull(section, 'name', section);
    this.order = CsvJsonIntegration.#order++;
    this.card.title = `${this.name} for %{query}`;
    const itypes = ArkimeConfig.getFullArray(section, 'itypes', ArkimeConfig.exit);

    this.#url = ArkimeConfig.getFull(section, 'url', ArkimeConfig.exit);

    itypes.forEach(itype => {
      if (itype === 'ip') {
        this.#doIp = true;
        this.itypes[itype] = 'getIp';
      } else {
        this.#doOther = true;
        this.itypes[itype] = 'getOther';
      }
    });

    // Set up parse method
    if (isCSV) {
      this.icon = ArkimeConfig.getFull(section, 'icon', 'integrations/memory/CSV.png');
      this.#keyColumn = ArkimeConfig.getFull(section, 'keyColumn', ArkimeConfig.exit);
      this.#parse = this.#parseCSV;
    } else {
      this.icon = ArkimeConfig.getFull(section, 'icon', 'integrations/memory/JSON.png');
      this.#arrayPath = ArkimeConfig.getFull(section, 'arrayPath');
      this.#keyPath = ArkimeConfig.getFull(section, 'keyPath', ArkimeConfig.exit);
      this.#parse = this.#parseJSON;
    }

    this.#reload = parseInt(ArkimeConfig.getFull(section, 'reload', -1));

    // Set up load function
    if (this.#url.startsWith('file:///')) {
      this.#url = this.#url.substring(7);
    }

    if (this.#url[0] === '/' || this.#url.startsWith('./') || this.#url.startsWith('../')) {
      if (!fs.existsSync(this.#url)) {
        console.log(this.section, '- ERROR not loading', this.section, 'since', this.#url, "doesn't exist");
        return;
      }
      this.#load = this.#loadFile;
    } else if (this.#url.startsWith('elasticsearch://') || this.#url.startsWith('elasticsearchs://')) {
      this.#url = this.#url.replace('elasticsearch', 'http');
      if (!this.#url.includes('/_source/')) {
        throw new Error('Missing _source in #url, should be format elasticsearch://user:pass@host:port/INDEX/_source/DOC');
      }
      this.#load = this.#loadHttp;
    } else if (this.#url.startsWith('opensearch://') || this.#url.startsWith('opensearchs://')) {
      this.#url = this.#url.replace('opensearch', 'http');
      if (!this.#url.includes('/_source/')) {
        throw new Error('Missing _source in #url, should be format opensearch://user:pass@host:port/INDEX/_source/DOC');
      }
      this.#load = this.#loadHttp;
    } else if (this.#url.startsWith('http')) {
      this.#load = this.#loadHttp;
    } else if (this.#url.startsWith('redis')) {
      this.#load = this.#loadRedis;
      const redisParts = this.#url.split('/');
      if (redisParts.length !== 5) {
        throw new Error(`Invalid redis url - ${redisParts[0]}//[:pass@]redishost[:redisport]/redisDbNum/key`);
      }
      this.#redisKey = redisParts.pop();
      this.#redisClient = ArkimeUtil.createRedisClient(redisParts.join('/'), section);
    } else {
      console.log(this.section, '- ERROR not loading', this.section, 'don\'t know how to open', this.#url);
      return;
    }

    setImmediate(this.#load.bind(this));
    Integration.register(this);
  }

  // ----------------------------------------------------------------------------
  async getIp (user, item) {
    const result = this.#ipCache.find(item);
    if (!result) {
      return Integration.NoResult;
    }

    return {
      data: result,
      _cont3xt: {
        count: 1
      }
    };
  }

  // ----------------------------------------------------------------------------
  async getOther (user, item) {
    const result = this.#otherCache.get(item);

    if (!result) {
      return Integration.NoResult;
    }

    return {
      data: result,
      _cont3xt: {
        count: result.length
      }
    };
  }

  // ----------------------------------------------------------------------------
  #parseCSV (body, setCb, endCb) {
    csv.parse(body, { skip_empty_lines: true, comment: '#', relax_column_count: true, columns: true }, (err, data) => {
      if (err) {
        return endCb(err);
      }

      for (let i = 0; i < data.length; i++) {
        setCb(data[i][this.#keyColumn], data[i]);
      }
      endCb();
    });
  }

  // ----------------------------------------------------------------------------
  #parseJSON (data, setCb, endCb) {
    let json = JSON.parse(data);
    if (this.#arrayPath !== undefined) {
      const arrayPath = this.#arrayPath.split('.');
      for (let i = 0; i < arrayPath.length; i++) {
        json = json[arrayPath[i]];
        if (!json) {
          return endCb(`Couldn't find ${arrayPath[i]} in results`);
        }
      }
    }

    // The rest of the code assumes an array
    if (!Array.isArray(json)) {
      json = [json];
    }

    const keyPath = this.#keyPath.split('.');

    for (let i = 0; i < json.length; i++) {
      // Walk the key path
      let key = json[i];
      for (let j = 0; key && j < keyPath.length; j++) {
        key = key[keyPath[j]];
      }

      if (key === undefined || key === null) {
        continue;
      }

      if (Array.isArray(key)) {
        key.forEach((part) => setCb(part, json[i]));
      } else {
        setCb(key, json[i]);
      }
    }
    endCb();
  }

  // ----------------------------------------------------------------------------
  #process (data) {
    if (!data) { return; }
    if (data instanceof Buffer) {
      data = data.toString('utf8');
    }
    if (data === '') { return; }

    if (this.#doIp) {
      const ipCache = new iptrie.IPTrie();
      const setCb = (key, value) => {
        if (!key) { return; }
        key.split(',').forEach((cidr) => {
          const parts = cidr.split('/');
          try {
            ipCache.add(ArkimeUtil.expandIp(parts[0]), +parts[1] ?? (parts[0].includes(':') ? 128 : 32), value);
          } catch (e) {
            console.log('ERROR adding', this.section, cidr, e);
          }
        });
      };
      const endCb = (err) => {
        if (!err) {
          this.#ipCache = ipCache;
        } else {
          console.log(err);
        }
      };
      this.#parse(data, setCb, endCb);
    }

    if (this.#doOther) {
      const otherCache = new Map();
      const setCb = (key, value) => {
        if (!key) { return; }
        const result = otherCache.get(key);
        if (result) {
          result.push(value);
        } else {
          otherCache.set(key, [value]);
        }
      };
      const endCb = (err) => {
        if (!err) {
          this.#otherCache = otherCache;
        } else {
          console.log(err);
        }
      };
      this.#parse(data, setCb, endCb);
    }

    if (this.#init && this.#reload > 0) {
      this.#init = false;
      setInterval(this.#load.bind(this), this.#reload * 1000 * 60);
    }
  }

  // ----------------------------------------------------------------------------
  #loadFile () {
    if (!fs.existsSync(this.#url)) {
      console.log(this.section, '- ERROR not loading', this.section, 'since', this.#url, "doesn't exist");
      return;
    }

    // Proess the file
    this.#process(fs.readFileSync(this.#url));

    // Watch for file changes, debounce over 500ms
    if (!this.#watch) {
      // Need to as variable because of 'this'
      const watchCb = () => {
        clearTimeout(this.#watchTimer);
        this.#watchTimer = setTimeout(() => {
          this.#load();
        }, 1000);
      };
      this.#watch = fs.watch(this.#url, watchCb);
    }
  };

  // ----------------------------------------------------------------------------
  #loadRedis () {
    if (this.#redisClient && this.#redisKey) {
      this.#redisClient.get(this.#redisKey, (err, data) => {
        if (err) {
          console.log(this.section, '- ERROR', err);
          return;
        }
        if (data === null) {
          return;
        }
        this.#process(data);
      });
    }
  };

  // ----------------------------------------------------------------------------
  #loadHttp () {
    axios.get(this.url)
      .then((response) => {
        return this.#process(response.data);
      })
      .catch((error) => {
        if (error.response && error.response.status === 404) {
          return;
        }
        console.log(this.section, '- ERROR', error);
      });
  };
}

let sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^csv:/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new CsvJsonIntegration(section, true);
});

sections = ArkimeConfig.getSections().filter((e) => { return e.match(/^json:/); });
sections.forEach((section) => {
  // eslint-disable-next-line no-new
  new CsvJsonIntegration(section, false);
});
