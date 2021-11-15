import Vue from 'vue';

/**
 * Removes time component of an ISO 8601 date string
 * @example
 * '{{ "1996-03-07T21:00:00.000-08:00" | removeTime }}'
 * this.$options.filters.removeTime('1996-03-07T21:00:00.000-08:00');
 *
 * @param {String} dateString - The time using ISO 8601
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
 * @returns {string} - Readable string without time
 */
export const dateString = function (ms) {
  if (isNaN(ms)) { return 'Invalid date'; }
  const date = new Date(ms);
  return date.toString();
};
Vue.filter('dateString', dateString);

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
  const RIRs = ['arin', 'ripe', 'apnic', 'lacnic', 'afrinic'];

  for (const rir of RIRs) {
    if (rirLink.includes(rir)) {
      return rir.toUpperCase();
    }
  };

  return 'unknown rir';
};
Vue.filter('baseRIR', baseRIR);

/**
 * Modifies a number to display the <=4 char human readable version of bytes
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ 1524680821 | humanReadableNumber }}'
 * this.$options.filters.humanReadableNumber(1524680821);
 *
 * @param {int} num   The number to make human readable
 * @returns {string}  The <=4 char human readable number
 */
export const humanReadableNumber = function (num) {
  if (isNaN(num)) { return '0 '; }

  let i = 0;
  const units = [' ', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  while (num >= 1000) {
    num = num / 1000;
    i++;
  }

  if (i === 0 || num >= 10) {
    return num.toFixed(0) + units[i];
  } else {
    return num.toFixed(1) + units[i];
  }
};
Vue.filter('humanReadableNumber', humanReadableNumber);
