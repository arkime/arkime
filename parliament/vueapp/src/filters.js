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
