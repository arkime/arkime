/******************************************************************************/
/* esProxyCredentials.js -- AWS credential provider for esproxy SigV4 signing.
 *                          Supports fetching temporary credentials from an
 *                          external URL (mTLS), STS AssumeRole, static creds,
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
    if (this.#config.credentialUrl) {
      return 'url';
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
  // Synchronous read of cached credentials. Returns null if initialize() has
  // not yet completed. Intended for hot paths that cannot await (e.g. the
  // @elastic/elasticsearch Connection.buildRequestObject hook). Callers must
  // await initialize() during startup; the background refresh timer keeps
  // #credentials fresh thereafter.
  getCachedCredentials () {
    return this.#credentials;
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
    case 'url':
      return this.#fetchUrl();
    case 'sts':
      return this.#fetchSts();
    case 'static':
      return this.#fetchStatic();
    case 'default':
      return this.#fetchDefault();
    }
  }

  // --------------------------------------------------------------------------
  // Fetch temporary AWS credentials from an external HTTPS endpoint using mTLS.
  // The endpoint must return JSON with accessKeyId, secretAccessKey, and
  // optionally sessionToken and expiration fields (camelCase or PascalCase).
  // --------------------------------------------------------------------------
  async #fetchUrl () {
    const { credentialUrl, credentialCert, credentialKey } = this.#config;

    const url = new URL(credentialUrl);
    const options = {
      method: 'GET',
      hostname: url.hostname,
      port: url.port || 443,
      path: url.pathname + url.search,
      rejectUnauthorized: true
    };

    if (credentialCert && credentialKey) {
      options.cert = fs.readFileSync(credentialCert);
      options.key = fs.readFileSync(credentialKey);
    }

    const body = await new Promise((resolve, reject) => {
      const req = https.request(options, (res) => {
        let data = '';
        res.on('data', (chunk) => { data += chunk; });
        res.on('end', () => {
          if (res.statusCode < 200 || res.statusCode >= 300) {
            reject(new Error(`Credential URL returned status ${res.statusCode}: ${data}`));
            return;
          }
          resolve(data);
        });
      });
      req.on('error', reject);
      req.end();
    });

    const json = JSON.parse(body);
    // Support responses with creds at top level or wrapped in a Credentials object
    const creds = json.Credentials || json.credentials || json;
    if (!creds || (!creds.accessKeyId && !creds.AccessKeyId)) {
      throw new Error('Credential URL response missing credentials: ' + body.substring(0, 200));
    }

    // Handle both PascalCase and camelCase field names
    this.#credentials = {
      accessKeyId: creds.AccessKeyId || creds.accessKeyId,
      secretAccessKey: creds.SecretAccessKey || creds.secretAccessKey,
      sessionToken: creds.SessionToken || creds.sessionToken
    };

    const expiration = creds.Expiration || creds.expiration;
    if (expiration) {
      this.#scheduleRefresh(new Date(expiration));
    }

    console.log('esProxyCredentials: refreshed credentials from URL');
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
      sessionToken: this.#config.sessionToken
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
