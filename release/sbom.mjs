#!/usr/bin/env node
// Writes a CycloneDX 1.5 SBOM for an Arkime build to stdout.
//
// Run from the repo root after 'make install', it needs the
// vueapp/third-party-licenses.json files that 'npm run bundle' writes.
// Covers the four places third-party code ends up in a package:
//   - server npm dependencies, from package-lock.json and cont3xt/package-lock.json
//   - browser npm dependencies vite inlined into the four Vue apps
//   - libraries built into capture (static yara/librdkafka on Amazon Linux,
//     the vendored C sources)
//   - the bundled Node.js runtime, CyberChef and the vendored viewer scripts
// Libraries capture links from the OS are declared package dependencies,
// not shipped, so they are not listed here.
//
// SPDX-License-Identifier: Apache-2.0

import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { execSync } from 'node:child_process';

const ROOT = process.cwd();
const APPS = ['viewer', 'cont3xt', 'parliament', 'wiseService'];

// SPDX ids we are confident about; anything else is recorded as a license name
const SPDX_IDS = new Set(['MIT', 'MIT-0', 'ISC', '0BSD', 'BSD-2-Clause', 'BSD-3-Clause', 'Apache-2.0', 'MPL-2.0',
  'LGPL-2.1', 'LGPL-2.1-only', 'LGPL-2.1-or-later', 'LGPL-3.0', 'GPL-2.0', 'GPL-3.0', 'CC0-1.0', 'CC-BY-3.0', 'CC-BY-4.0',
  'Unlicense', 'BlueOak-1.0.0', 'Python-2.0', 'WTFPL', 'Zlib', 'Artistic-2.0', 'AFL-2.1', 'PSF-2.0', 'BSD-3-Clause-Clear']);

function licenses (str) {
  if (!str) { return undefined; }
  str = String(str).trim();
  if (/\b(AND|OR|WITH)\b|[()]/.test(str)) { return [{ expression: str }]; }
  if (SPDX_IDS.has(str)) { return [{ license: { id: str } }]; }
  return [{ license: { name: str } }];
}

function purlNpm (name, version) {
  const [scope, base] = name.startsWith('@') ? name.split('/') : [undefined, name];
  return `pkg:npm/${scope ? encodeURIComponent(scope) + '/' : ''}${base}${version ? '@' + version : ''}`;
}

function hashes (integrity) {
  if (!integrity) { return undefined; }
  const out = [];
  for (const part of integrity.split(/\s+/)) {
    const m = /^sha(256|384|512)-(.+)$/.exec(part);
    if (m) { out.push({ alg: `SHA-${m[1]}`, content: Buffer.from(m[2], 'base64').toString('hex') }); }
  }
  return out.length ? out : undefined;
}

function readJson (file) { return JSON.parse(fs.readFileSync(file, 'utf8')); }

function grab (file, re, what) {
  const m = re.exec(fs.readFileSync(file, 'utf8'));
  if (!m) { throw new Error(`could not find ${what} in ${file}`); }
  return m[1];
}

// ---------------------------------------------------------------------------
// npm lock files: walk the production dependency graph the way npm resolves it
// ---------------------------------------------------------------------------
const components = new Map();   // bom-ref -> component
const dependsOn = new Map();    // bom-ref -> Set(bom-ref)
const rootDeps = new Set();

function addComponent (c) {
  const existing = components.get(c['bom-ref']);
  if (!existing) { components.set(c['bom-ref'], c); return c; }
  // another source may know things the first did not
  for (const f of ['licenses', 'hashes', 'externalReferences']) {
    if (existing[f] === undefined && c[f] !== undefined) { existing[f] = c[f]; }
  }
  // merge properties from another source
  for (const p of c.properties ?? []) {
    existing.properties ??= [];
    const same = existing.properties.find(q => q.name === p.name);
    if (!same) { existing.properties.push(p); } else { same.value = [...new Set([...same.value.split(' '), ...p.value.split(' ')])].join(' '); }
  }
  return existing;
}

function addEdge (from, to) {
  if (!dependsOn.has(from)) { dependsOn.set(from, new Set()); }
  dependsOn.get(from).add(to);
}

// the lock only carries a license when npm freshly resolved the package from the
// registry, so fall back to the installed package.json (present after make install)
function packageLicense (dir) {
  let pkg;
  try { pkg = readJson(path.join(dir, 'package.json')); } catch { return undefined; }
  if (typeof pkg.license === 'string' && !/^(UNKNOWN|SEE LICENSE IN)/i.test(pkg.license)) { return pkg.license; }
  if (pkg.license?.type) { return pkg.license.type; }
  if (Array.isArray(pkg.licenses)) {
    const types = pkg.licenses.map(l => typeof l === 'string' ? l : l.type).filter(Boolean);
    return types.length > 1 ? `(${types.join(' OR ')})` : types[0];
  }
  return licenseFileId(dir);
}

// last resort: recognise the common license texts
function licenseFileId (dir) {
  let names;
  try { names = fs.readdirSync(dir); } catch { return undefined; }
  const file = names.find(n => /^(licen[cs]e|copying)/i.test(n));
  if (!file) { return undefined; }
  let text;
  try { text = fs.readFileSync(path.join(dir, file), 'utf8'); } catch { return undefined; }
  return licenseFromText(text);
}

function licenseFromText (text) {
  if (!text) { return undefined; }
  text = text.slice(0, 2000);
  if (/Apache License,?\s+Version 2\.0/i.test(text)) { return 'Apache-2.0'; }
  if (/Permission is hereby granted, free of charge/i.test(text)) { return 'MIT'; }
  if (/Permission to use, copy, modify, and\/or distribute this software for any purpose/i.test(text)) { return 'ISC'; }
  if (/Redistribution and use in source and binary forms/i.test(text)) {
    return /Neither the name|may not be used to endorse/i.test(text) ? 'BSD-3-Clause' : 'BSD-2-Clause';
  }
  return undefined;
}

function lockGraph (lockFile, label) {
  const lock = readJson(lockFile);
  const lockDir = path.dirname(lockFile);
  const pk = lock.packages;
  const root = pk[''];

  // npm node resolution: nearest node_modules/<name> walking up from the requiring package
  function resolve (fromKey, name) {
    let base = fromKey;
    for (;;) {
      const candidate = (base ? base + '/' : '') + 'node_modules/' + name;
      if (pk[candidate]) { return candidate; }
      if (!base) { return undefined; }
      const i = base.lastIndexOf('/node_modules/');
      base = i < 0 ? '' : base.slice(0, i);
    }
  }

  const refOf = new Map();
  function visit (key) {
    if (refOf.has(key)) { return refOf.get(key); }
    const e = pk[key];
    if (!e || e.dev) { return undefined; }
    const name = e.name ?? key.slice(key.lastIndexOf('node_modules/') + 'node_modules/'.length);
    const ref = purlNpm(name, e.version);
    refOf.set(key, ref);
    let lic = e.license;
    if (!lic || /^(UNKNOWN|SEE LICENSE IN)/i.test(lic)) { lic = packageLicense(path.join(lockDir, key)) ?? licenseFileId(path.join(lockDir, key)) ?? lic; }
    const c = {
      type: 'library',
      'bom-ref': ref,
      name,
      version: e.version,
      purl: ref,
      licenses: licenses(lic),
      hashes: hashes(e.integrity),
      externalReferences: e.resolved ? [{ type: 'distribution', url: e.resolved }] : undefined,
      properties: [{ name: 'arkime:lockfile', value: label }]
    };
    addComponent(c);
    const wanted = { ...(e.dependencies ?? {}), ...(e.optionalDependencies ?? {}), ...(e.peerDependencies ?? {}) };
    for (const dep of Object.keys(wanted)) {
      const k = resolve(key, dep);
      if (!k) { continue; }           // optional/peer not installed
      const r = visit(k);
      if (r) { addEdge(ref, r); }
    }
    return ref;
  }

  const top = new Set();
  for (const dep of Object.keys(root.dependencies ?? {})) {
    const k = resolve('', dep);
    const r = k && visit(k);
    if (r) { top.add(r); }
  }
  return top;
}

// ---------------------------------------------------------------------------
// gather
// ---------------------------------------------------------------------------
const version = grab(path.join(ROOT, 'common/version.js'), /exports\.version\s*=\s*"([^"]+)"/, 'version');
let build;
try { build = execSync('git describe --tags', { cwd: ROOT, stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim(); } catch { /* no git */ }

for (const r of lockGraph(path.join(ROOT, 'package-lock.json'), 'root')) { rootDeps.add(r); }
for (const r of lockGraph(path.join(ROOT, 'cont3xt/package-lock.json'), 'cont3xt')) { rootDeps.add(r); }

// what vite put into the browser bundles
for (const app of APPS) {
  const file = path.join(ROOT, app, 'vueapp/third-party-licenses.json');
  if (!fs.existsSync(file)) {
    console.error(`sbom.mjs: missing ${file}, run 'npm run bundle:min' in ${app} first`);
    process.exit(1);
  }
  for (const d of readJson(file)) {
    const ref = purlNpm(d.name, d.version);
    let lic = d.license;
    if (!lic || /^(UNKNOWN|SEE LICENSE IN)/i.test(lic)) { lic = licenseFromText(d.licenseText) ?? lic; }
    addComponent({
      type: 'library',
      'bom-ref': ref,
      name: d.name,
      version: d.version,
      purl: ref,
      licenses: licenses(lic),
      externalReferences: d.repository ? [{ type: 'vcs', url: d.repository }] : undefined,
      properties: [{ name: 'arkime:bundled-into', value: app }]
    });
    rootDeps.add(ref);
  }
}

// capture and the other bundled pieces, versions pulled from where the build pins them
const easybutton = path.join(ROOT, 'easybutton-build.sh');
const YARA = grab(easybutton, /^YARA=(\S+)/m, 'YARA');
const KAFKA = grab(easybutton, /^KAFKA=(\S+)/m, 'KAFKA');
const NODE = grab(easybutton, /^NODE=(\S+)/m, 'NODE');
const CYBERCHEF = grab(path.join(ROOT, 'viewer/Makefile.in'), /^CYBERCHEF_VERSION=(\S+)/m, 'CYBERCHEF_VERSION');
const hp = path.join(ROOT, 'capture/thirdparty/http_parser.h');
const HTTP_PARSER = `${grab(hp, /HTTP_PARSER_VERSION_MAJOR (\d+)/, 'major')}.${grab(hp, /HTTP_PARSER_VERSION_MINOR (\d+)/, 'minor')}`;
const xx = path.join(ROOT, 'capture/thirdparty/xxhash.h');
const XXHASH = `${grab(xx, /XXH_VERSION_MAJOR\s+(\d+)/, 'major')}.${grab(xx, /XXH_VERSION_MINOR\s+(\d+)/, 'minor')}.${grab(xx, /XXH_VERSION_RELEASE\s+(\d+)/, 'release')}`;
const d3v = grab(path.join(ROOT, 'viewer/public/d3.min.js'), /version:"([0-9.]+)"/, 'd3 version');
const cubismv = grab(path.join(ROOT, 'viewer/public/cubism.v1.min.js'), /version:"([0-9.]+)"/, 'cubism version');

const extra = [
  // static on Amazon Linux only, every other platform links the OS package
  { type: 'library', name: 'yara', version: YARA, purl: `pkg:github/VirusTotal/yara@v${YARA}`, licenses: licenses('BSD-3-Clause'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'static' }, { name: 'arkime:platforms', value: 'al2023 al2027' }] },
  { type: 'library', name: 'librdkafka', version: KAFKA, purl: `pkg:github/confluentinc/librdkafka@v${KAFKA}`, licenses: licenses('BSD-2-Clause'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'static' }, { name: 'arkime:platforms', value: 'al2023 al2027' }] },
  // vendored C sources compiled into capture
  { type: 'library', name: 'http_parser', version: HTTP_PARSER, purl: `pkg:github/nodejs/http-parser@v${HTTP_PARSER}`, licenses: licenses('MIT'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'source' }] },
  { type: 'library', name: 'xxHash', version: XXHASH, purl: `pkg:github/Cyan4973/xxHash@v${XXHASH}`, licenses: licenses('BSD-2-Clause'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'source' }] },
  { type: 'library', name: 'js0n', purl: 'pkg:github/quartzjer/js0n', licenses: licenses('Public Domain'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'source' }] },
  { type: 'library', name: 'patricia', purl: 'pkg:github/plonka/perl-Net-Patricia', licenses: licenses('BSD-3-Clause'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'source' }] },
  { type: 'library', name: 'nfdump mkpath', purl: 'pkg:github/phaag/nfdump', licenses: licenses('BSD-3-Clause'),
    properties: [{ name: 'arkime:component', value: 'capture' }, { name: 'arkime:linkage', value: 'source' }] },
  // bundled runtime and viewer assets
  { type: 'platform', name: 'node', version: NODE, purl: `pkg:github/nodejs/node@v${NODE}`, licenses: licenses('MIT'),
    properties: [{ name: 'arkime:component', value: 'runtime' }] },
  { type: 'application', name: 'CyberChef', version: CYBERCHEF, purl: `pkg:github/gchq/CyberChef@v${CYBERCHEF}`, licenses: licenses('Apache-2.0'),
    properties: [{ name: 'arkime:component', value: 'viewer' }] },
  { type: 'library', name: 'd3', version: d3v, purl: purlNpm('d3', d3v), licenses: licenses('BSD-3-Clause'),
    properties: [{ name: 'arkime:component', value: 'viewer' }, { name: 'arkime:file', value: 'viewer/public/d3.min.js' }] },
  { type: 'library', name: 'cubism', version: cubismv, purl: purlNpm('cubism', cubismv), licenses: licenses('Apache-2.0'),
    properties: [{ name: 'arkime:component', value: 'viewer' }, { name: 'arkime:file', value: 'viewer/public/cubism.v1.min.js' }] },
  { type: 'library', name: 'highlight.js', purl: purlNpm('highlight.js'), licenses: licenses('BSD-3-Clause'),
    properties: [{ name: 'arkime:component', value: 'viewer' }, { name: 'arkime:file', value: 'viewer/public/highlight.min.js' }] }
];
for (const c of extra) { c['bom-ref'] = c.purl; addComponent(c); rootDeps.add(c.purl); }

// ---------------------------------------------------------------------------
// emit
// ---------------------------------------------------------------------------
const rootRef = `pkg:generic/arkime@${version}`;
const bom = {
  bomFormat: 'CycloneDX',
  specVersion: '1.5',
  serialNumber: `urn:uuid:${crypto.randomUUID()}`,
  version: 1,
  metadata: {
    timestamp: new Date().toISOString(),
    tools: { components: [{ type: 'application', name: 'arkime release/sbom.mjs', version }] },
    component: {
      type: 'application',
      'bom-ref': rootRef,
      name: 'arkime',
      version,
      purl: rootRef,
      licenses: licenses('Apache-2.0'),
      externalReferences: [{ type: 'website', url: 'https://arkime.com' }, { type: 'vcs', url: 'https://github.com/arkime/arkime' }],
      properties: build ? [{ name: 'arkime:build', value: build }] : undefined
    }
  },
  components: [...components.values()].sort((a, b) => a['bom-ref'].localeCompare(b['bom-ref'])),
  dependencies: [
    { ref: rootRef, dependsOn: [...rootDeps].sort() },
    ...[...dependsOn.entries()].sort(([a], [b]) => a.localeCompare(b)).map(([ref, set]) => ({ ref, dependsOn: [...set].sort() }))
  ]
};

// drop undefined fields so the JSON is clean
process.stdout.write(JSON.stringify(bom, (k, v) => v === undefined ? undefined : v, 2) + '\n');
