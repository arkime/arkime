import Vue from 'vue';
import moment from 'moment-timezone';

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

/**
 * Parses date to string and applies the selected timezone
 *
 * @example
 * '{{ 1524680821 | timezoneDateString("local", false) }}'
 * this.$options.filters.timezoneDateString(1524680821, "local", false);
 *
 * @param {int} ms           The time in milliseconds from epoch
 * @param {string} timezone  The timezone to use ('gmt', 'local', or 'localtz'), default = 'local'
 * @returns {string}         The date formatted and converted to the requested timezone
 */
export const timezoneDateString = function (ms, timezone, showMs) {
  if (isNaN(ms)) { return 'Invalid date'; }

  let format = showMs ? 'YYYY/MM/DD HH:mm:ss.SSS z' : 'YYYY/MM/DD HH:mm:ss z';

  if (timezone === 'gmt') {
    return moment.tz(ms, 'utc').format(format);
  } else if (timezone === 'localtz') {
    return moment.tz(ms, Intl.DateTimeFormat().resolvedOptions().timeZone).format(format);
  }

  format = showMs ? 'YYYY/MM/DD HH:mm:ss.SSS' : 'YYYY/MM/DD HH:mm:ss';
  return moment(ms).format(format);
};
Vue.filter('timezoneDateString', timezoneDateString);

// used in parseSeconds to determine the unit of relative time
function str2format (str) {
  if (str.match(/^(s|sec|secs|second|seconds)$/)) {
    return 'seconds';
  } else if (str.match(/^(m|min|mins|minute|minutes)$/)) {
    return 'minutes';
  } else if (str.match(/^(h|hr|hrs|hour|hours)$/)) {
    return 'hours';
  } else if (str.match(/^(d|day|days)$/)) {
    return 'days';
  } else if (str.match(/^(w|week|weeks)\d*$/)) {
    return 'weeks';
  } else if (str.match(/^(M|mon|mons|month|months)$/)) {
    return 'months';
  } else if (str.match(/^(q|qtr|qtrs|quarter|quarters)$/)) {
    return 'quarters';
  } else if (str.match(/^(y|yr|yrs|year|years)$/)) {
    return 'years';
  }

  return undefined;
}

/**
 * Parses the current time to seconds based on a relative time
 * (+/- seconds/minutes/hours/days/weeks/months/quarters/years)
 *
 * @example
 * '{{ -5h | parseSeconds }}'
 * this.$options.filters.parseSeconds('-5d');
 *
 * @param {string} str The relative time string
 */
export const parseSeconds = function (str) {
  let m, n;
  if ((m = str.match(/^([+-])(\d*)([a-z]*)([@]*)([a-z0-9]*)/))) {
    const d = moment();
    const format = str2format(m[3]);
    const snap = str2format(m[5]);

    if (m[2] === '') {
      m[2] = 1;
    }

    if (snap) {
      d.startOf(snap);
      if ((n = m[5].match(/^(w|week|weeks)(\d+)$/))) {
        d.day(n[2]);
      }
    }

    d.add((m[1] === '-' ? -1 : 1) * m[2], format);
    return d.unix();
  }

  if ((m = str.match(/^@([a-z0-9]+)/))) {
    const d = moment();
    const snap = str2format(m[1]);

    d.startOf(snap);
    if ((n = m[1].match(/^(w|week|weeks)(\d+)$/))) {
      d.day(n[2]);
    }
    return d.unix();
  }

  return moment(str, ['YYYY/MM/DDTHH:mm:ss', 'YYYY/MM/DDTHH:mm:ssZ', moment.ISO_8601]).unix();
};
Vue.filter('parseSeconds', parseSeconds);
