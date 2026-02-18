/******************************************************************************/
/* esProxyCredentials.js -- AWS credential provider for esproxy SigV4 signing.
 *                          Supports Athenz ZTS, STS AssumeRole, static creds,
 *                          and the default AWS credential chain.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const https = require('https');
const fs = require('fs');

// ============================================================================
// EsProxyCredentials
// ============================================================================

class EsProxyCredentials {
  #config;
  #credentials;
  #refreshTimer;
  #source;

  // --------------------------------------------------------------------------
  constructor (config) {
    this.#config = config || {};
    this.#credentials = null;
    this.#refreshTimer = null;
    this.#source = this.#determineSource();
  }

  // --------------------------------------------------------------------------
  #determineSource () {
    if (this.#config.athenzZtsUrl) {
      return 'athenz';
    }
    if (this.#config.roleArn) {
      return 'sts';
    }
    if (this.#config.accessKeyId && this.#config.secretAccessKey) {
      return 'static';
    }
    return 'default';
  }

  // --------------------------------------------------------------------------
  async getCredentials () {
    if (this.#credentials) {
      return this.#credentials;
    }
    return this.#fetchCredentials();
  }

  // --------------------------------------------------------------------------
  async initialize () {
    console.log(`esProxyCredentials: initializing with source=${this.#source}`);
    await this.#fetchCredentials();
    console.log('esProxyCredentials: initial credentials obtained');
  }

  // --------------------------------------------------------------------------
  stop () {
    if (this.#refreshTimer) {
      clearTimeout(this.#refreshTimer);
      this.#refreshTimer = null;
    }
  }

  // --------------------------------------------------------------------------
  async #fetchCredentials () {
    switch (this.#source) {
    case 'athenz':
      return this.#fetchAthenz();
    case 'sts':
      return this.#fetchSts();
    case 'static':
      return this.#fetchStatic();
    case 'default':
      return this.#fetchDefault();
    }
  }

  // --------------------------------------------------------------------------
  async #fetchAthenz () {
    const { athenzZtsUrl, athenzDomain, athenzRole, athenzCert, athenzKey, athenzAwsAccount } = this.#config;

    const certData = fs.readFileSync(athenzCert);
    const keyData = fs.readFileSync(athenzKey);

    const urlStr = `${athenzZtsUrl}/domain/${athenzDomain}/role/${athenzRole}/creds` +
      `?durationSeconds=3600&externalId=${athenzAwsAccount}`;
    const url = new URL(urlStr);

    const options = {
      method: 'POST',
      hostname: url.hostname,
      port: url.port || 443,
      path: url.pathname + url.search,
      cert: certData,
      key: keyData,
      rejectUnauthorized: true
    };

    const body = await new Promise((resolve, reject) => {
      const req = https.request(options, (res) => {
        let data = '';
        res.on('data', (chunk) => { data += chunk; });
        res.on('end', () => {
          if (res.statusCode < 200 || res.statusCode >= 300) {
            reject(new Error(`Athenz ZTS returned status ${res.statusCode}: ${data}`));
            return;
          }
          resolve(data);
        });
      });
      req.on('error', reject);
      req.end();
    });

    const json = JSON.parse(body);
    const creds = json.Credentials || json.credentials;
    if (!creds) {
      throw new Error('Athenz ZTS response missing Credentials field');
    }

    this.#credentials = {
      accessKeyId: creds.AccessKeyId,
      secretAccessKey: creds.SecretAccessKey,
      sessionToken: creds.SessionToken
    };

    const expiration = creds.Expiration;
    if (expiration) {
      this.#scheduleRefresh(new Date(expiration));
    }

    console.log('esProxyCredentials: refreshed credentials from Athenz ZTS');
    return this.#credentials;
  }

  // --------------------------------------------------------------------------
  async #fetchSts () {
    const { STSClient, AssumeRoleCommand } = require('@aws-sdk/client-sts');

    const client = new STSClient({});
    const command = new AssumeRoleCommand({
      RoleArn: this.#config.roleArn,
      RoleSessionName: 'arkime-esproxy'
    });

    const response = await client.send(command);
    const creds = response.Credentials;

    this.#credentials = {
      accessKeyId: creds.AccessKeyId,
      secretAccessKey: creds.SecretAccessKey,
      sessionToken: creds.SessionToken
    };

    if (creds.Expiration) {
      this.#scheduleRefresh(new Date(creds.Expiration));
    }

    console.log('esProxyCredentials: refreshed credentials from STS AssumeRole');
    return this.#credentials;
  }

  // --------------------------------------------------------------------------
  async #fetchStatic () {
    this.#credentials = {
      accessKeyId: this.#config.accessKeyId,
      secretAccessKey: this.#config.secretAccessKey,
      sessionToken: undefined
    };
    console.log('esProxyCredentials: using static credentials');
    return this.#credentials;
  }

  // --------------------------------------------------------------------------
  async #fetchDefault () {
    const { fromNodeProviderChain } = require('@aws-sdk/credential-providers');

    const provider = fromNodeProviderChain();
    const creds = await provider();

    this.#credentials = {
      accessKeyId: creds.accessKeyId,
      secretAccessKey: creds.secretAccessKey,
      sessionToken: creds.sessionToken
    };

    if (creds.expiration) {
      this.#scheduleRefresh(new Date(creds.expiration));
    }

    console.log('esProxyCredentials: refreshed credentials from default provider chain');
    return this.#credentials;
  }

  // --------------------------------------------------------------------------
  #scheduleRefresh (expiration) {
    if (this.#refreshTimer) {
      clearTimeout(this.#refreshTimer);
    }

    const now = Date.now();
    const expiresAt = expiration.getTime();
    const ttl = expiresAt - now;
    const delay = Math.max(ttl * 0.75, 30000); // refresh at 75% of TTL, min 30s

    console.log(`esProxyCredentials: scheduling refresh in ${Math.round(delay / 1000)}s (expires in ${Math.round(ttl / 1000)}s)`);

    this.#refreshTimer = setTimeout(async () => {
      try {
        await this.#fetchCredentials();
      } catch (err) {
        console.log('esProxyCredentials: failed to refresh credentials, will keep using cached:', err.message);
        // Retry again at half the remaining TTL
        const remaining = expiresAt - Date.now();
        if (remaining > 60000) {
          this.#scheduleRefresh(expiration);
        }
      }
    }, delay);

    // Allow the process to exit even if the timer is pending
    if (this.#refreshTimer.unref) {
      this.#refreshTimer.unref();
    }
  }
}

module.exports = EsProxyCredentials;
