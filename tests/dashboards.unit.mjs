/*
Copyright Yahoo Inc.
SPDX-License-Identifier: Apache-2.0

The shipped dashboards (release/dashboards/*.dashboard.json) are generated
from release/dashboards/generate.py; this checks the committed files are what
the generator writes, so an edit to one without the other is caught. Run with
`npm run test:unit`. Skipped where python3 isn't installed.
*/
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.join(path.dirname(fileURLToPath(import.meta.url)), '..');
const SHIPPED = path.join(ROOT, 'release', 'dashboards');
const GENERATOR = path.join(SHIPPED, 'generate.py');

const dashboardFiles = (dir) => fs.readdirSync(dir).filter(f => f.endsWith('.dashboard.json')).sort();

const python = spawnSync('python3', ['--version']);
const havePython = !python.error && python.status === 0;

test('the shipped dashboards match generate.py', { skip: !havePython && 'python3 not installed' }, () => {
  const out = fs.mkdtempSync(path.join(os.tmpdir(), 'arkime-dashboards-'));
  try {
    const run = spawnSync('python3', ['-I', GENERATOR, out], { cwd: ROOT, encoding: 'utf8' });
    assert.equal(run.status, 0, run.stderr);

    const generated = dashboardFiles(out);
    assert.deepEqual(dashboardFiles(SHIPPED), generated, 'the same set of files');
    for (const f of generated) {
      assert.equal(fs.readFileSync(path.join(SHIPPED, f), 'utf8'), fs.readFileSync(path.join(out, f), 'utf8'), `${f} is what the generator writes`);
    }
  } finally {
    fs.rmSync(out, { recursive: true, force: true });
  }
});

test('every shipped dashboard is a v1 dashboard with a name and widgets', () => {
  for (const f of dashboardFiles(SHIPPED)) {
    const dash = JSON.parse(fs.readFileSync(path.join(SHIPPED, f), 'utf8'));
    assert.ok(typeof dash.name === 'string' && dash.name.trim(), `${f} has a name`);
    assert.equal(dash.data?.version, 1, `${f} is version 1`);
    assert.ok(Array.isArray(dash.data.widgets) && dash.data.widgets.length, `${f} has widgets`);
    const ids = dash.data.widgets.map(w => w.id);
    assert.equal(new Set(ids).size, ids.length, `${f} widget ids are unique`);
  }
});
