import Vue from 'vue';
import moment from 'moment-timezone';

/**
 * Removes time component of an ISO 8601 date string
 * @example
 * '{{ "1996-03-07T21:00:00.000-08:00" | removeTime }}'
 * this.$options.filters.removeTime('1996-03-07T21:00:00.000-08:00');
 *
 * @param {String} dateStringWTime - The time using ISO 8601
 * @returns {String} - Date string without time
 */
export const removeTime = function (dateStringWTime) {
  if (!dateStringWTime) { return ''; }
  return dateStringWTime.split('T')[0];
};
Vue.filter('removeTime', removeTime);

/**
 * Parses ms date to string
 *
 * @example
 * '{{ 1524680821 | dateString }}'
 * this.$options.filters.dateString(1524680821);
 *
 * @returns {string} - Our common date string display
 */
export const dateString = function (ms) {
  if (isNaN(ms)) { return 'Invalid date'; }
  const format = /* showMs ? 'YYYY/MM/DD HH:mm:ss.SSS z' : */ 'YYYY/MM/DD HH:mm:ss z';
  return moment.tz(ms, 'utc').format(format);
};
Vue.filter('dateString', dateString);

/**
 * Parsing various date strings and converts to our common format
 *
 * @example
 * '{{ 2020-07-10 15:00:00.000 | reDateString }}'
 * this.$options.filters.dateString('2020-07-10 15:00:00.000');
 *
 * @returns {string} - Our common date string display
 */
export const reDateString = function (str) {
  if (str === undefined) { return 'Invalid date'; }
  const format = /* showMs ? 'YYYY/MM/DD HH:mm:ss.SSS z' : */ 'YYYY/MM/DD HH:mm:ss z';
  return moment(str).format(format);
};
Vue.filter('reDateString', reDateString);

/**
 * Converts milliseconds to a readable time-span
 *
 * @example
 * '{{ 300000 | 5 minutes }}'
 * this.$options.filters.readableDuration(300000);
 *
 * @returns {string} - the readable time period string
 */
export const readableDuration = function (ms) {
  return moment.duration(ms).humanize(false);
};
Vue.filter('readableDuration', readableDuration);

/**
 * Determines the RIR based on a link.
 * @example
 * '{{ "https://rdap.arin.net/registry/ip/74.6.136.150" | rirLink }}'
 * this.$options.filters.rirLink('https://rdap.arin.net/registry/ip/74.6.136.150');
 *
 * @param {String} rirLink - The link that contains the RIR
 * @returns {String} - RIR string (ARIN|RIPE|APNIC|LACNIC|AFRINIC)
 */
export const baseRIR = function (rirLink) {
  if (!rirLink) { return 'unknown rir'; }

  const RIRs = ['arin', 'ripe', 'apnic', 'lacnic', 'afrinic'];

  for (const rir of RIRs) {
    if (rirLink.includes(rir)) {
      return rir.toUpperCase();
    }
  }

  return 'unknown rir';
};
Vue.filter('baseRIR', baseRIR);
