const Integration = require('../integration.js');
const axios = require('axios');

class URLScanIntegration extends Integration {
  name = 'RDAP';
  itypes = {
    url: 'fetchUrl'
  };

  key;

  constructor () {
    super();

    this.key = 'foo';

    Integration.register(this);
  }

  async fetchUrl (url) {
    try {
      const urlScanRes = await axios.get('https://urlscan.io/api/v1/search/', {
        params: {
          q: url
        },
        headers: {
          'API-Key': this.key
        }
      });
      return urlScanRes.data;
    } catch (err) {
      console.log(err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new URLScanIntegration();
