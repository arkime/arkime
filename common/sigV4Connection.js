/******************************************************************************/
/* sigV4Connection.js -- @elastic/elasticsearch v7 Connection subclass factory
 *                       that signs outbound requests with AWS SigV4. Used by
 *                       the viewer (and optionally cont3xt) when talking
 *                       directly to AWS Managed OpenSearch with IAM auth.
 *
 *                       Exported as a factory because @elastic/elasticsearch
 *                       constructs Connection instances internally and does
 *                       not forward unknown opts to the constructor — closing
 *                       over credentials/region/service is the clean way to
 *                       give each Client its own signing context.
 *
 *                       The credentials provider must expose
 *                       getCachedCredentials() returning an object with
 *                       accessKeyId, secretAccessKey, and optionally
 *                       sessionToken. See viewer/esProxyCredentials.js for
 *                       the reference implementation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

'use strict';

const aws4 = require('aws4');
const { Connection } = require('@elastic/elasticsearch');

// ============================================================================
function createSigV4Connection (credentials, region, service = 'es') {
  if (!credentials) {
    throw new Error('createSigV4Connection: credentials provider is required');
  }
  if (!region) {
    throw new Error('createSigV4Connection: region is required');
  }

  class SigV4Connection extends Connection {
    buildRequestObject (params) {
      const req = super.buildRequestObject(params);

      const creds = credentials.getCachedCredentials();
      if (!creds) {
        // initialize() should have been awaited before traffic flows; if we
        // land here OpenSearch will reject with 403, which surfaces the
        // misconfiguration clearly rather than silently sending unsigned.
        return req;
      }

      if (!req.headers) { req.headers = {}; }
      // aws4 signs based on the Host header; @elastic/elasticsearch sets
      // Host from url.hostname downstream, so mirror that here so the
      // signature matches what's actually put on the wire.
      req.headers.host = req.hostname;
      req.service = service;
      req.region = region;

      aws4.sign(req, {
        accessKeyId: creds.accessKeyId,
        secretAccessKey: creds.secretAccessKey,
        sessionToken: creds.sessionToken
      });

      return req;
    }
  }

  return SigV4Connection;
}

module.exports = createSigV4Connection;
