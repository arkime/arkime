'use strict';

const filters = require('../vueFilters');

test('round', () => {
  expect(filters.round('asdf')).toBe(0); // default
  expect(filters.round(123456)).toBe(123456);
  expect(filters.round(123456.123)).toBe(123456);
  expect(filters.round(123456.789)).toBe(123457);
  expect(filters.round(123456.789, 1)).toBe(123456.8);
  expect(filters.round(123456.789, 2)).toBe(123456.79);
});

test('commaString', () => {
  expect(filters.commaString('asdf')).toBe(0); // default
  expect(filters.commaString(123456)).toBe('123,456');
  expect(filters.commaString(123456789)).toBe('123,456,789');
  expect(filters.commaString(1234567890)).toBe('1,234,567,890');
});

test('roundCommaString', () => {
  expect(filters.roundCommaString('asdf')).toBe(0); // default
  expect(filters.roundCommaString(123456.654321)).toBe('123,457');
  expect(filters.roundCommaString(123456.654321, 2)).toBe('123,456.65');
  expect(filters.roundCommaString(123456.654321, 3)).toBe('123,456.654');
  expect(filters.roundCommaString(123456789.98765, 4)).toBe('123,456,789.9877');
});

test('humanReadableNumber', () => {
  expect(filters.humanReadableNumber('a')).toBe('0 ');
  expect(filters.humanReadableNumber(1)).toBe('1 ');
  expect(filters.humanReadableNumber(1000)).toBe('1.0k');
  expect(filters.humanReadableNumber(10000)).toBe('10k');
  expect(filters.humanReadableNumber(1000000)).toBe('1.0M');
  expect(filters.humanReadableNumber(20000000)).toBe('20M');
  expect(filters.humanReadableNumber(1000000000)).toBe('1.0G');
  expect(filters.humanReadableNumber(321000000000)).toBe('321G');
  expect(filters.humanReadableNumber(4000000000000)).toBe('4.0T');
  expect(filters.humanReadableNumber(80000000000000)).toBe('80T');
  expect(filters.humanReadableNumber(9000000000000000)).toBe('9.0P');
  expect(filters.humanReadableNumber(987600000000000000)).toBe('988P');
});

test('timezoneDateString', () => {
  expect(filters.timezoneDateString('a')).toBe('Invalid date');
  expect(filters.timezoneDateString(0, 'gmt')).toBe('1970/01/01 00:00:00 UTC');
  expect(filters.timezoneDateString(1624024589000, 'gmt')).toBe('2021/06/18 13:56:29 UTC');
  expect(filters.timezoneDateString(1234567898765, 'gmt', true)).toBe('2009/02/13 23:31:38.765 UTC');
});
