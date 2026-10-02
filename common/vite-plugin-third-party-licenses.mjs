// Vite plugin that records every npm package whose code ended up in the
// production bundle, with its license text, as JSON. release/notice.txt.pl
// folds these into NOTICE.txt. license-checker cannot do this job: the Vue
// apps are built from devDependencies that vite inlines, and license-checker
// only sees what npm would install at runtime.
//
// SPDX-License-Identifier: Apache-2.0

import fs from 'node:fs';
import path from 'node:path';

const NM = '/node_modules/';

function packageRoot (id) {
  // strip rollup/vite virtual-module markers and query strings
  const clean = id.replace(/\\/g, '/').replace(/^\0/, '').replace(/\?.*$/, '');
  const i = clean.lastIndexOf(NM);
  if (i < 0) { return undefined; }
  const rest = clean.slice(i + NM.length).split('/');
  const name = rest[0].startsWith('@') ? rest.slice(0, 2).join('/') : rest[0];
  if (!name) { return undefined; }
  return clean.slice(0, i + NM.length) + name;
}

function licenseText (dir) {
  let names;
  try { names = fs.readdirSync(dir); } catch { return undefined; }
  const match = names.find(n => /^(licen[cs]e|copying)(\.|$)/i.test(n)) ??
                names.find(n => /^(licen[cs]e|copying)/i.test(n));
  if (!match) { return undefined; }
  try { return fs.readFileSync(path.join(dir, match), 'utf8'); } catch { return undefined; }
}

function field (v) {
  if (v === undefined || v === null) { return undefined; }
  if (typeof v === 'string') { return v; }
  return v.url ?? v.type ?? v.name;
}

export default function thirdPartyLicenses ({ file }) {
  return {
    name: 'arkime-third-party-licenses',
    apply: 'build',
    generateBundle (_options, bundle) {
      const seen = new Map();
      for (const chunk of Object.values(bundle)) {
        if (chunk.type !== 'chunk') { continue; }
        const ids = [...(chunk.moduleIds ?? []), ...Object.keys(chunk.modules ?? {})];
        for (const id of ids) {
          const root = packageRoot(id);
          if (!root || seen.has(root)) { continue; }
          let pkg;
          try { pkg = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8')); } catch { continue; }
          seen.set(root, {
            name: pkg.name,
            version: pkg.version,
            license: field(pkg.license) ?? (Array.isArray(pkg.licenses) ? pkg.licenses.map(field).join(' OR ') : 'UNKNOWN'),
            repository: field(pkg.repository) ?? pkg.homepage ?? '',
            author: field(pkg.author),
            licenseText: licenseText(root)
          });
        }
      }
      const deps = [...seen.values()].sort((a, b) => a.name.localeCompare(b.name) || a.version.localeCompare(b.version));
      fs.mkdirSync(path.dirname(file), { recursive: true });
      fs.writeFileSync(file, JSON.stringify(deps, null, 2) + '\n');
      this.info?.(`wrote ${deps.length} third-party packages to ${file}`);
    }
  };
}
