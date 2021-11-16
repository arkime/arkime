import Vue from 'vue';

/**
 * Rounds a number using Math.round
 *
 * @example
 * '{{ 1234.56 | round(0) }}'
 * this.$options.filters.round(1234.56, 0);
 *
 * @param {number} value  The number to round
 * @param {int} decimals  The number of decimals to preserve, default = 0
 * @returns {decimal}     The number rounded to the requested number of decimal places
 */
export const round = function (value, decimals) {
  if (isNaN(value)) { return 0; }

  if (!value) { value = 0; }

  if (!decimals) { decimals = 0; }

  return Math.round(value * Math.pow(10, decimals)) / Math.pow(10, decimals);
};
Vue.filter('round', round);

/**
 * Adds commas to a number so it's easier to read
 *
 * @example
 * '{{ 123456789 | commaString }}'
 * this.$options.filters.commaString(123456789);
 *
 * @param {int} input The number to add commas to
 * @returns {string}  The number string with commas
 */
export const commaString = function (input) {
  if (isNaN(input)) { return 0; }

  const parts = input.toString().split('.');
  parts[0] = parts[0].replace(/\B(?=(\d{3})+(?!\d))/g, ',');
  return parts.join('.');
};
Vue.filter('commaString', commaString);

/**
 * Rounds a number then adds commas so it's easier to read
 *
 * @example
 * '{{ 123456789 | roundCommaString }}'
 * this.$options.filters.roundCommaString(123456789);
 *
 * @param {int} input     The number to add commas to
 * @param {int} decimals  The number of decimals to preserve, default = 0
 * @returns {string}      The number string with commas
 */
export const roundCommaString = function (input, decimals) {
  if (isNaN(input)) { return 0; }

  if (!decimals) { decimals = 0; }
  return commaString(round(input, decimals).toFixed(decimals));
};
Vue.filter('roundCommaString', roundCommaString);

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
