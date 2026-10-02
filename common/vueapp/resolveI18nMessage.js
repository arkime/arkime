/**
 * Copyright Yahoo Inc.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Resolves a translated message from an object with i18n fields.
 * Falls back to `text`, `message`, the string itself, or '' when nothing resolves.
 *
 * @param {Object|string} obj - A string, or an object with optional i18n, i18nParams, text, or message fields
 * @param {Function} t - The i18n translation function (e.g., this.$t)
 * @returns {string} The resolved message, or '' if none could be resolved
 */
export function resolveMessage (obj, t) {
  if (obj?.i18n && t) {
    const translated = t(obj.i18n, obj.i18nParams || {});
    if (translated !== obj.i18n) { return translated; }
  }
  if (typeof obj === 'string') { return obj; }
  return obj?.text || obj?.message || '';
}
