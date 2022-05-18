'use strict';

const filters = require('../src/utils/filters');

test('removeTime', () => {
  expect(filters.removeTime()).toBe(''); // default
  expect(filters.removeTime('1996-03-07T21:00:00.000-08:00')).toBe('1996-03-07');
  expect(filters.removeTime('2022-03-14T09:56:21.123-05:00')).toBe('2022-03-14');
});

test('dateString', () => {
  expect(filters.dateString('a')).toBe('Invalid date');
  expect(filters.dateString(0)).toBe('1970/01/01 00:00:00 UTC');
  expect(filters.dateString(1624024589000)).toBe('2021/06/18 13:56:29 UTC');
  expect(filters.dateString(1234567898765)).toBe('2009/02/13 23:31:38 UTC');
});

test('reDateString', () => {
  expect(filters.reDateString()).toBe('Invalid date');
  expect(filters.reDateString('2020-07-10 15:00:00.000')).toBe('2020/07/10 15:00:00 ');
  expect(filters.reDateString('2022-W19')).toBe('2022/05/09 00:00:00 ');
  expect(filters.reDateString('2022-W19-5')).toBe('2022/05/13 00:00:00 ');
});

test('baseRIR', () => {
  expect(filters.baseRIR()).toBe('unknown rir');
  expect(filters.baseRIR('asdf')).toBe('unknown rir');
  expect(filters.baseRIR('https://rdap.arin.net/registry/ip/74.6.136.150')).toBe('ARIN');
  expect(filters.baseRIR('https://ripe.net/registry/ip/39.1.59.965')).toBe('RIPE');
  expect(filters.baseRIR('https://apnic.net')).toBe('APNIC');
  expect(filters.baseRIR('https://lacnic.net')).toBe('LACNIC');
  expect(filters.baseRIR('https://afrinic.net')).toBe('AFRINIC');
});
