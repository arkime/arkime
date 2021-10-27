const Integration = require('../integration.js');
const axios = require('axios');

class RDAPIntegration extends Integration {
  name = 'RDAP';
  itypes = {
    ip: 'fetchIp'
  };

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchIp (user, ip) {
    const query = 'https://rdap.db.ripe.net/ip/' + ip;
    const res = await axios.get(query, {
      validateStatus: false,
      headers: {
        Accept: 'application/json',
        'User-Agent': this.userAgent()
      }
    });
    const data = await res.data;
    return (res.status === 200) ? { status: res.status, name: data.name, link: data.links[0].value } : { status: res.status, error: res.status };
  }
}

// eslint-disable-next-line no-new
new RDAPIntegration();
