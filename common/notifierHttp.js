/******************************************************************************/
/* notifierHttp.js  -- shared HTTP POST for notifiers that call a web API
 *
 * Not named notifier.*.js on purpose, Notifier.initialize loads every one of
 * those as a notifier type.
 *
 * Copyright Andy Wick
 *
 * SPDX-License-Identifier: Apache-2.0
 */
'use strict';

const TIMEOUT_MS = 10000;

/**
 * POST an alert and call cb exactly once, with {} on success or
 * { errors: { [type]: message } } on failure.
 * @param {string} type - notifier type, the key in errors
 * @param {string} label - used in log lines and the timeout error, like "Teams webhook"
 * @param {string} url - where to POST
 * @param {object} options - fetch headers and body
 * @param {function} checkResponse - async (response) => undefined when the API accepted the alert, else an error string
 * @param {function} [cb]
 * @ignore
 */
exports.postAlert = async function (type, label, url, options, checkResponse, cb) {
  let error;
  try {
    const response = await fetch(url, {
      method: 'POST',
      headers: options.headers,
      body: options.body,
      signal: AbortSignal.timeout(TIMEOUT_MS)
    });
    error = await checkResponse(response);
  } catch (err) {
    error = err.name === 'TimeoutError' ? `${label} request timed out` : (err.message || String(err));
  }

  if (error) { console.error(`${label} error:`, error); }
  if (!cb) { return; }

  // Outside the try so a throwing cb isn't called again with an error, and
  // caught so it can't become an unhandled rejection that stops the process
  try {
    cb(error ? { errors: { [type]: error } } : {});
  } catch (err) {
    console.error(`${label} alert callback error:`, err);
  }
};
