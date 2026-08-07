import moment from 'moment-timezone';

/**
 * Removes time component of an ISO 8601 date string
 * @example
 * removeTime('1996-03-07T21:00:00.000-08:00'); // '1996-03-07'
 *
 * @param {String} dateStringWTime - The time using ISO 8601
 * @returns {String} - Date string without time
 */
export const removeTime = function (dateStringWTime) {
  if (!dateStringWTime) { return ''; }
  return dateStringWTime.split('T')[0];
};

/**
 * Parses ms date to string
 *
 * @example
 * dateString(1524680821);
 *
 * @returns {string} - Our common date string display
 */
export const dateString = function (ms) {
  if (isNaN(ms)) { return 'Invalid date'; }
  const format = /* showMs ? 'YYYY/MM/DD HH:mm:ss.SSS z' : */ 'YYYY/MM/DD HH:mm:ss z';
  return moment.tz(ms, 'utc').format(format);
};

/**
 * Parsing various date strings and converts to our common format
 *
 * @example
 * reDateString('2020-07-10 15:00:00.000');
 *
 * @returns {string} - Our common date string display
 */
export const reDateString = function (str) {
  if (str === undefined) { return 'Invalid date'; }
  const format = /* showMs ? 'YYYY/MM/DD HH:mm:ss.SSS z' : */ 'YYYY/MM/DD HH:mm:ss z';
  return moment(str).format(format);
};

/**
 * Converts milliseconds to a readable time-span
 *
 * @example
 * readableDuration(300000); // '5 minutes'
 *
 * @returns {string} - the readable time period string
 */
export const readableDuration = function (ms) {
  return moment.duration(ms).humanize(false);
};

/**
 * Determines the RIR based on a link.
 * @example
 * baseRIR('https://rdap.arin.net/registry/ip/74.6.136.150'); // 'ARIN'
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
