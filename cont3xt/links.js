const Db = require('./db.js');

class Links {
  constructor (data) {
    console.log('ALW DATA', data);
    console.log('ALW this', this);
  }

  save () {
  }

  static async get (query) {
    console.log('ENTER Links.get', query);
    const results = await Db.getLinks(query);

    const hits = results.body.hits.hits;
    const links = [];
    for (let i = 0; i < hits.length; i++) {
      links.push(new Links(hits[i]._source));
    }

    return links;
  }
}

module.exports = Links;
