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
export const removeTime = function (dateString) {
  return dateString.split('T')[0];
};
Vue.filter('removeTime', removeTime);

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
