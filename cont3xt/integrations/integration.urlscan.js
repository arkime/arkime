const Integration = require('../integration.js');
const axios = require('axios');

class URLScanIntegration extends Integration {
  name = 'URLScan';
  itypes = {
    url: 'fetchUrl'
  };

  key;

  constructor () {
    super();

    Integration.register(this);
  }

  async fetchUrl (user, url) {
    try {
      const key = this.getUserConfig(user, 'URLScanKey');
      if (!key) {
        return undefined;
      }

      const urlScanRes = await axios.get('https://urlscan.io/api/v1/search/', {
        params: {
          q: url
        },
        headers: {
          'API-Key': key,
          'User-Agent': this.userAgent()
        }
      });
      return urlScanRes.data;
    } catch (err) {
      console.log('URLSCAN', url, err);
      return null;
    }
  }
}

// eslint-disable-next-line no-new
new URLScanIntegration();
