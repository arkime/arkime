/**
 * Resolves a translated message from an object with i18n fields.
 * Falls back to `text`, `message`, or String(obj) when no translation exists.
 *
 * @param {Object} obj - An object with optional i18n, i18nParams, text, or message fields
 * @param {Function} t - The i18n translation function (e.g., this.$t)
 * @returns {string} The resolved message
 */
export function resolveMessage (obj, t) {
  if (obj?.i18n && t) {
    const translated = t(obj.i18n, obj.i18nParams || {});
    if (translated !== obj.i18n) { return translated; }
  }
  return obj?.text || obj?.message || String(obj);
}
