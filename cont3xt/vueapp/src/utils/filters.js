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
