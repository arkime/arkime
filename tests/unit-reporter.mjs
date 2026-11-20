/*
Copyright Andy Wick
SPDX-License-Identifier: Apache-2.0
*/

// A node --test reporter that prints one line per test file, like the perl
// tests.pl output, instead of one line per test:
//
//   tests/arkimeDb.unit.mjs ............. ok     812 ms
//   tests/dbCh.unit.mjs ................. not ok  95 ms
//
// A failing file is followed by each failed test with its error and any
// output the file printed. Files print as they finish, so with concurrency
// the order varies from run to run.

import path from 'node:path';
import util from 'node:util';

// pad names to the longest test file given on the command line
const files = process.argv.filter(a => a.endsWith('.mjs') && !a.endsWith('unit-reporter.mjs'));
const width = Math.max(30, ...files.map(f => path.relative(process.cwd(), path.resolve(f)).length)) + 4;

const indent = (text, by) => String(text).replace(/^/gm, ' '.repeat(by));

function errorText (error) {
  // node wraps a test's own error as the cause of a test failure error
  const err = error?.cause ?? error;
  return util.inspect(err, { depth: 4, colors: false });
}

export default async function * unitReporter (source) {
  const perFile = new Map(); // file -> { names: [], failures: [], output: [], done }
  // output events name the file as given on the command line, the rest by
  // its full path, so key everything by the full path
  const fileOf = (file) => {
    const key = path.resolve(file);
    if (!perFile.has(key)) { perFile.set(key, { names: [], failures: [], output: [], done: false }); }
    return perFile.get(key);
  };

  function fileReport (file, ok, ms) {
    const f = fileOf(file);
    f.done = true;
    const fileName = path.relative(process.cwd(), file);
    const dots = ' ' + '.'.repeat(Math.max(2, width - fileName.length)) + ' ';
    const time = ms === undefined ? '' : `${Math.round(ms)} ms`.padStart(ok ? 9 : 5);
    let out = `${fileName}${dots}${ok ? 'ok' : 'not ok'}${time}\n`;
    for (const { name: testName, error } of f.failures) {
      out += `  ✖ ${testName}\n`;
      if (error) { out += `${indent(errorText(error), 4)}\n`; }
    }
    if (!ok && f.output.length) {
      out += `  output:\n${indent(f.output.join('').trimEnd(), 4)}\n`;
    }
    return out;
  }

  for await (const { type, data } of source) {
    switch (type) {
    case 'test:start':
      if (data.file) { fileOf(data.file).names[data.nesting] = data.name; }
      break;
    case 'test:stdout':
    case 'test:stderr':
      if (data.file) { fileOf(data.file).output.push(data.message); }
      break;
    case 'test:fail': {
      if (!data.file) { break; }
      const failureType = data.details?.error?.failureType;
      // a suite, or the file itself, fails when a test in it does; that test
      // is already listed, so only keep failures of their own (a hook, a crash)
      if (failureType === 'subtestsFailed') { break; }
      const f = fileOf(data.file);
      if (data.nesting === 0 && path.resolve(data.name) === path.resolve(data.file)) {
        // the file itself failed, say it threw while loading; its output has the error
        f.failures.push({ name: 'the file failed to run, see its output' });
      } else {
        f.failures.push({ name: [...f.names.slice(0, data.nesting), data.name].join(' > '), error: data.details?.error });
      }
      break;
    }
    case 'test:summary':
      if (data.file) {
        yield fileReport(data.file, data.success && !fileOf(data.file).failures.length, data.duration_ms);
        break;
      }
      // the run's summary: report any file that never got its own summary
      // (it crashed before running), then the totals
      for (const [file, f] of perFile) {
        if (!f.done && f.failures.length) { yield fileReport(file, false); }
      }
      {
        const c = data.counts;
        const failedFiles = [...perFile.values()].filter(f => f.failures.length).length;
        const extra = [c.skipped && `${c.skipped} skipped`, c.todo && `${c.todo} todo`, c.cancelled && `${c.cancelled} cancelled`].filter(Boolean).join(', ');
        yield (data.success ? 'All tests successful.\n' : `Failed ${failedFiles}/${perFile.size} test files, ${c.failed}/${c.tests} tests.\n`) +
          `Files=${perFile.size}, Tests=${c.tests}${extra ? ` (${extra})` : ''}, ${(data.duration_ms / 1000).toFixed(1)} secs\n` +
          `Result: ${data.success ? 'PASS' : 'FAIL'}\n`;
      }
      break;
    }
  }
}
