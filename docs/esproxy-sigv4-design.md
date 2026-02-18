# esproxy: AWS SigV4 Request Signing for OpenSearch

## Summary

Add AWS Signature v4 request signing to esproxy so that Arkime capture sensors
can ship session metadata to a public-facing AWS OpenSearch domain using IAM
authentication — without requiring AWS Direct Connect, VPN tunnels, or IP-based
access policies.

## Motivation

Arkime sensors currently write session metadata to Elasticsearch/OpenSearch
using Basic Auth or API keys. When migrating to AWS-managed OpenSearch, the
standard approach is AWS IAM-based authentication via SigV4-signed HTTP
requests.

Today, getting sensors to talk to AWS OpenSearch requires one of:

1. **AWS Direct Connect** — Requires data center hardware, network team
   involvement, port-hour costs, and an IP-based access policy on the
   OpenSearch domain. Does not provide per-request identity verification.

2. **VPC-based OpenSearch + VPN** — Similar infrastructure overhead with the
   OpenSearch domain locked to a VPC.

3. **OpenSearch with FGAC + Basic Auth** — Works without code changes but
   requires managing internal user credentials separately from your org's
   identity system.

None of these options align with a zero-trust architecture where each request
is authenticated via cryptographic identity.

### Proposed approach

Modify esproxy to obtain temporary AWS credentials (via Athenz ZTS, STS
AssumeRole, instance profile, or environment variables) and sign every outbound
request to OpenSearch with SigV4. Sensors continue authenticating to esproxy
using their existing Basic Auth credentials. esproxy acts as the trust boundary:

```
[Capture Sensor]
  | Basic Auth over HTTPS
  v
[esproxy] — validates sensor identity, whitelists operations
  | SigV4-signed HTTPS
  v
[AWS OpenSearch (public endpoint, IAM auth)]
```

This keeps all existing sensor-side configuration unchanged. Only esproxy needs
new configuration for AWS credential sourcing.

## Current State of esproxy

`viewer/esProxy.js` (~468 lines) is a lightweight Express.js proxy that:

- Accepts requests from sensors with Basic Auth + optional IP validation
- Whitelists specific GET/POST/PUT/DELETE paths (sessions, fields, stats, bulk)
- Validates bulk indexing payloads (only allows sessions/fields indices)
- Forwards requests to Elasticsearch/OpenSearch via raw `http`/`https` modules
- Supports optional "tee" to a secondary cluster
- Auth to upstream ES: Basic Auth (`elasticsearchBasicAuth`) or API Key
  (`elasticsearchAPIKey`) — set as a static `Authorization` header
- Listens on port 7200 by default (`esProxyPort`)

Key function: `doProxyFull()` (line 227) — single code path for all proxied
requests. Constructs an `http.request()` with the upstream URL, sets the
`Authorization` header, copies select headers from the original request, and
pipes the body. This is the injection point for SigV4 signing.

## Design

### Credential Provider

A module that obtains and caches AWS credentials. Supports multiple sources
(checked in order):

1. **Athenz ZTS** — Call ZTS with service identity cert to get temporary AWS
   STS credentials. Requires config: `esProxySigV4AthenzZtsUrl`,
   `esProxySigV4AthenzCert`, `esProxySigV4AthenzKey`, `esProxySigV4AthenzRole`.
2. **STS AssumeRole** — Assume an IAM role using base credentials. Requires
   config: `esProxySigV4RoleArn`.
3. **Environment / instance profile** — Standard AWS SDK credential chain
   (`AWS_ACCESS_KEY_ID`, `AWS_SECRET_ACCESS_KEY`, EC2/ECS metadata). No config
   needed.

Credentials are refreshed automatically before expiry (e.g., refresh at 75% of
TTL, typically ~45 minutes for 1-hour tokens).

### Request Signing

Before forwarding each request in `doProxyFull()`:

1. Read the request method, path, query string, headers, and body
2. Compute SigV4 signature using the current cached credentials
3. Set `Authorization`, `X-Amz-Date`, `X-Amz-Security-Token` (if using
   session credentials), and `X-Amz-Content-Sha256` headers
4. Remove any existing `Authorization` header (from sensor Basic Auth)
5. Forward the signed request to OpenSearch

Use `@aws-sdk/signature-v4` + `@smithy/hash-node` for signing. These are
lightweight packages (~50KB) that don't require the full AWS SDK.

### Configuration

New config options in `config.ini` under the existing `[default]` or
`[esproxy]` section:

```ini
# Enable SigV4 signing for upstream OpenSearch requests
esProxySigV4=true

# AWS region for the OpenSearch domain
esProxySigV4Region=us-east-1

# Service name (default: "es" for OpenSearch)
esProxySigV4Service=es

# --- Credential source options (pick one) ---

# Option A: Athenz ZTS
esProxySigV4AthenzZtsUrl=https://zts.example.com:4443/zts/v1
esProxySigV4AthenzDomain=your.athenz.domain
esProxySigV4AthenzRole=opensearch-writer
esProxySigV4AthenzCert=/path/to/service.cert.pem
esProxySigV4AthenzKey=/path/to/service.key.pem
esProxySigV4AthenzAwsAccount=123456789012

# Option B: STS AssumeRole (uses env/instance creds as base)
esProxySigV4RoleArn=arn:aws:iam::123456789012:role/opensearch-writer

# Option C: Static credentials (for testing only)
esProxySigV4AccessKeyId=AKIA...
esProxySigV4SecretAccessKey=...

# Option D: Default AWS credential chain (no config needed, just set
#           AWS_ACCESS_KEY_ID/AWS_SECRET_ACCESS_KEY env vars or use
#           instance profile / ECS task role)
```

When `esProxySigV4=true`, the existing `elasticsearchBasicAuth` and
`elasticsearchAPIKey` settings are ignored for upstream auth (sensor-side
Basic Auth still works).

### Tee Support

The tee configuration already uses a separate `authHeader`. If the tee target
also needs SigV4, it gets its own config section `[tee]` with the same
`esProxySigV4*` keys.

## Tasks

- [x] **Add AWS signing dependencies** — Add `@aws-sdk/signature-v4`,
      `@smithy/hash-node`, `@smithy/protocol-http` to package.json.
- [x] **Implement credential provider** — Module that obtains, caches, and
      refreshes AWS credentials from the configured source (env/instance
      profile, AssumeRole, or Athenz ZTS).
- [x] **Implement request signer** — Function that takes an HTTP request
      (method, URL, headers, body) and returns SigV4-signed headers.
- [x] **Integrate signing into doProxyFull()** — When SigV4 is enabled,
      sign outbound requests before forwarding. Handle body hashing for
      bulk payloads (possibly chunked/gzip-encoded).
- [x] **Add configuration parsing** — Read `esProxySigV4*` config keys in
      the `ArkimeConfig.loaded()` block.
- [x] **Add Athenz ZTS credential source** — HTTPS call to ZTS to exchange
      service identity for temporary AWS credentials.
- [x] **Handle credential refresh** — Timer-based refresh before expiry.
      Graceful handling if refresh fails (use cached creds until they expire,
      log errors).
- [x] **Add tee SigV4 support** — Separate credential provider for the tee
      target if configured.
- [ ] **Test with local OpenSearch** — Verify basic functionality with a
      local OpenSearch instance + SigV4 auth (or mocked).
- [ ] **Test with AWS OpenSearch** — End-to-end test with a real AWS
      OpenSearch domain.
- [ ] **Update documentation** — Document new config options on arkime.com
      esproxy page.
- [x] **Update CHANGELOG** — Add entry under the appropriate section.

## Risks and Considerations

**Body hashing with gzip** — SigV4 requires hashing the request body. esproxy
already handles gzip-encoded bulk payloads (line 318-330). The body must be
hashed *as sent* (compressed), not decompressed. This should work naturally
since `req.body` contains the raw bytes.

**Large bulk payloads** — Bulk requests can be up to 11MB (line 212). SHA-256
hashing 11MB is fast (~20ms) but should be verified under load. For
`UNSIGNED-PAYLOAD`, AWS OpenSearch would need to allow it — this may not work
with IAM auth.

**Credential refresh during requests** — If credentials expire mid-request,
the request fails. The refresh timer should ensure credentials are always valid
with comfortable margin.

**Clock skew** — SigV4 requires timestamps within 5 minutes of AWS time. The
esproxy host must have NTP configured. This is typically a non-issue but worth
documenting.

**No changes to sensors or capture** — This is purely an esproxy change.
Sensors continue using Basic Auth to talk to esproxy. The capture binary and
viewer are unaffected.

## Infrastructure Dependencies (AWS Baselines)

The target OpenSearch domain must satisfy AWS-6032, AWS-6033, and AWS-6095.
SigV4 signing with a scoped IAM role is the cleanest path to compliance.

- **IAM role** for esproxy with `es:ESHttp*` permissions on the domain
  resource. Do not use `es:*` — that includes domain admin actions.
- **Domain access policy** must list the esproxy role ARN as a trusted
  principal (satisfies AWS-6032 trusted accounts + AWS-6033 resource-based
  policy). Do not use `"Principal": "*"`.
- **Athenz ZTS** must be configured to map the esproxy service identity to
  the IAM role above.
- **Encryption at rest** must be enabled on the domain (AWS-6095) — infra
  team responsibility, no esproxy code impact.
- **GACCO auto-remediation** — verify it won't overwrite the domain access
  policy. Register the domain as manually managed if needed.
- **Optional defense-in-depth** — add `aws:SourceIp` condition to the domain
  policy if esproxy runs from known static IPs.
