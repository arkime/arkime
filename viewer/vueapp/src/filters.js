import Vue from 'vue';
import moment from 'moment-timezone';

/**
 * Adds commas to a number so it's easier to read
 *
 * @example
 * '{{ 123456789 | commaString }}'
 * this.options.$filters.commaString(123456789);
 *
 * @param {int} input The number to add commas to
 * @returns {string}  The number string with commas
 */
Vue.filter('commaString', (input) => {
  if (isNaN(input)) { return 0; }
  return input.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ',');
});

/**
 * Parses ipv6
 *
 * @example
 * '{{ ipv6 | extractIPv6String }}'
 * this.options.$filters.extractIPv6String(ipv6);
 *
 * @param {int} ipv6  The ipv6 value
 * @returns {string}  The human understandable ipv6 string
 */
Vue.filter('extractIPv6String', (ipv6) => {
  if (!ipv6) { return ''; }

  ipv6 = ipv6.toString();

  let ip = ipv6.match(/.{1,4}/g).join(':').replace(/:0{1,3}/g, ':').replace(/^0000:/, '0:');
  [/(^|:)0:0:0:0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0:0($|:)/,
    /(^|:)0:0:0:0($|:)/,
    /(^|:)0:0:0($|:)/,
    /(^|:)0:0($|:)/]
    .every(function (re) {
      if (ipv6.match(re)) {
        ip = ipv6.replace(re, '::');
        return false;
      }
      return true;
    });

  return ip;
});

/**
 * Displays the protocol string instead of number code
 *
 * @example
 * '{{ 1 | protocol }}'
 * this.options.$filters.protocol(1);
 *
 * @param {int} protocolCode  The protocol code
 * @returns {string}          The human understandable protocol string
 */
Vue.filter('protocol', (protocolCode) => {
  let lookup = { 1: 'icmp', 6: 'tcp', 17: 'udp', 47: 'gre', 50: 'esp', 58: 'icmp6', 132: 'sctp' };

  let result = lookup[protocolCode];
  if (!result) { result = protocolCode; }
  return result;
});

/**
 * Modifies a number to display the <=4 char human readable version of bytes
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ 1524680821 | humanReadableBytes }}'
 * this.options.$filters.humanReadableBytes(1524680821);
 *
 * @param {int} fileSizeInBytes The number to make human readable
 * @returns {string}            The <=4 char human readable number
 */
Vue.filter('humanReadableBytes', (fileSizeInBytes) => {
  fileSizeInBytes = parseInt(fileSizeInBytes);
  let i = 0;
  let byteUnits = ['Bi', 'Ki', 'Mi', 'Gi', 'Ti', 'Pi', 'Ei', 'Zi', 'Yi'];
  while (fileSizeInBytes >= 1000) {
    fileSizeInBytes = fileSizeInBytes / 1024;
    i++;
  }

  if (i === 0 || fileSizeInBytes >= 10) {
    return fileSizeInBytes.toFixed(0) + byteUnits[i];
  } else {
    return fileSizeInBytes.toFixed(1) + byteUnits[i];
  }
});

/**
 * Modifies a number to display the <=4 char human readable version of bytes
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ 1524680821 | humanReadableNumber }}'
 * this.options.$filters.humanReadableNumber(1524680821);
 *
 * @param {int} num   The number to make human readable
 * @returns {string}  The <=4 char human readable number
 */
Vue.filter('humanReadableNumber', (num) => {
  let i = 0;
  let units = [' ', 'k', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  while (num >= 1000) {
    num = num / 1000;
    i++;
  }

  if (i === 0 || num >= 10) {
    return num.toFixed(0) + units[i];
  } else {
    return num.toFixed(1) + units[i];
  }
});

/**
 * Parses date to string and applies the selected timezone
 *
 * @example
 * '{{ 1524680821 | timezoneDateString("local", "YYYY/MM/DD HH:mm:ss z") }}'
 * this.options.$filters.timezoneDateString(1524680821, "local", "YYYY/MM/DD HH:mm:ss z");
 *
 * @param {int} seconds      The time in seconds from epoch
 * @param {string} timezone  The timezone to use ('gmt' or 'local'), default = 'local'
 * @param {string} format    The format to display the date string, default = 'YYYY/MM/DD HH:mm:ss z'
 * @returns {string}         The date formatted and converted to the requested timezone
 */
Vue.filter('timezoneDateString', (seconds, timezone, format) => {
  if (!format) { format = 'YYYY/MM/DD HH:mm:ss z'; }

  if (timezone === 'gmt') {
    return moment.tz(1000 * seconds, 'gmt').format(format);
  }

  return moment(1000 * seconds).format(format);
});

/**
 * Rounds a number using Math.round
 *
 * @example
 * '{{ 1234.56 | round(0) }}'
 * this.options.$filters.round(1234.56, 0);
 *
 * @param {number} value  The number to round
 * @param {int} decimals  The number of decimals to preserve, default = 0
 * @returns {decimal}     The number rounded to the requested number of decimal places
 */
Vue.filter('round', function (value, decimals) {
  if (!value) { value = 0; }

  if (!decimals) { decimals = 0; }

  return Math.round(value * Math.pow(10, decimals)) / Math.pow(10, decimals);
});

/**
 * Turns milliseconds into a human readable time range
 *
 * @example
 * '{{ 1524680821790 | readableTime }}'
 * this.options.$filters.timezoneDateString(1524680821790);
 *
 * @param {int} ms    The time in ms from epoch
 * @returns {string}  The human readable time range
 *                    Output example: 1 day 10:42:01
 */
Vue.filter('readableTime', function (ms) {
  if (isNaN(ms)) { return '?'; }

  let seconds = parseInt((ms / 1000) % 60);
  let minutes = parseInt((ms / (1000 * 60)) % 60);
  let hours = parseInt((ms / (1000 * 60 * 60)) % 24);
  let days = parseInt((ms / (1000 * 60 * 60 * 24)));

  let result = '';

  if (days) {
    result += days + ' day';
    if (days > 1) {
      result += 's ';
    } else {
      result += ' ';
    }
  }

  if (hours || minutes || seconds) {
    result += (hours < 10) ? '0' + hours : hours;
    result += ':';
    result += (minutes < 10) ? '0' + minutes : minutes;
    result += ':';
    result += (seconds < 10) ? '0' + seconds : seconds;
  }

  return result || '0';
});
