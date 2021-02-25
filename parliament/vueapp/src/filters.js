import Vue from 'vue';

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
 * Modifies a number to display the <=4 char human readable version of bits
 * Modified http://stackoverflow.com/questions/10420352/converting-file-size-in-bytes-to-human-readable
 *
 * @example
 * '{{ 1524680821 | humanReadableBits }}'
 * this.$options.filters.humanReadableBytes(1524680821);
 *
 * @param {int} fileSizeInBytes The number to make human readable
 * @returns {string}            The <=4 char human readable number
 */
Vue.filter('humanReadableBits', (fileSizeInBytes) => {
  fileSizeInBytes = parseInt(fileSizeInBytes);
  let i = 0;
  const bitUnits = ['', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y'];
  fileSizeInBytes *= 8;
  while (fileSizeInBytes >= 1024) {
    fileSizeInBytes = fileSizeInBytes / 1024;
    i++;
  }

  if (i === 0 || fileSizeInBytes >= 10) {
    return fileSizeInBytes.toFixed(0) + bitUnits[i];
  } else {
    return fileSizeInBytes.toFixed(1) + bitUnits[i];
  }
});

/**
 * Displays the proper human readable value for an issue
 *
 * @example
 * '{{ issue.value | issueValue(issue.type) }}'
 * this.options.$filters.issueValue(issue.value, issue.type);
 *
 * @param {string, number} input The issues value to be made human readable
 * @param {string} type The type of issue
 * @returns {string}  A human readable value
 */
Vue.filter('issueValue', (input, type) => {
  let result = input;

  if (input === undefined) { return ''; }

  if (type === 'esDropped') {
    result = Vue.options.filters.commaString(input);
  } else if (type === 'outOfDate') {
    result = Vue.options.filters.moment(input, 'YYYY/MM/DD HH:mm:ss');
  }

  return result;
});
