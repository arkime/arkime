'use strict';

const filters = require('../src/filters');
const commonFilters = require('../../../common/vueFilters');
const { fields } = require('./consts');

test('round', () => {
  expect(commonFilters.round('asdf')).toBe(0); // default
  expect(commonFilters.round(123456)).toBe(123456);
  expect(commonFilters.round(123456.123)).toBe(123456);
  expect(commonFilters.round(123456.789)).toBe(123457);
  expect(commonFilters.round(123456.789, 1)).toBe(123456.8);
  expect(commonFilters.round(123456.789, 2)).toBe(123456.79);
});

test('commaString', () => {
  expect(commonFilters.commaString('asdf')).toBe(0); // default
  expect(commonFilters.commaString(123456)).toBe('123,456');
  expect(commonFilters.commaString(123456789)).toBe('123,456,789');
  expect(commonFilters.commaString(1234567890)).toBe('1,234,567,890');
});

test('roundCommaString', () => {
  expect(commonFilters.roundCommaString('asdf')).toBe(0); // default
  expect(commonFilters.roundCommaString(123456.654321)).toBe('123,457');
  expect(commonFilters.roundCommaString(123456.654321, 2)).toBe('123,456.65');
  expect(commonFilters.roundCommaString(123456.654321, 3)).toBe('123,456.654');
  expect(commonFilters.roundCommaString(123456789.98765, 4)).toBe('123,456,789.9877');
});

test('extractIPv6String', () => {
  expect(filters.extractIPv6String()).toBe(''); // default
  expect(filters.extractIPv6String('2001:4998:ef83:14:8000::100d')).toBe('2001::499:8:ef:83:1:4:80::::100d');
  expect(filters.extractIPv6String('1111:2222:3333:4444:5555:7777::')).toBe('1111::222:2:33:33:4:444::5555::777:7::');
});

test('protocol', () => {
  expect(filters.protocol(0)).toBe(0); // default
  expect(filters.protocol(1)).toBe('icmp');
  expect(filters.protocol(2)).toBe('igmp');
  expect(filters.protocol(6)).toBe('tcp');
  expect(filters.protocol(17)).toBe('udp');
  expect(filters.protocol(47)).toBe('gre');
  expect(filters.protocol(50)).toBe('esp');
  expect(filters.protocol(58)).toBe('icmp6');
  expect(filters.protocol(89)).toBe('ospf');
  expect(filters.protocol(103)).toBe('pim');
  expect(filters.protocol(132)).toBe('sctp');
});

test('humanReadableBits', () => {
  expect(filters.humanReadableBits('asdf')).toBe('0'); // default
  expect(filters.humanReadableBits('1')).toBe('1');
  expect(filters.humanReadableBits('1234')).toBe('1.2K');
  expect(filters.humanReadableBits('1024')).toBe('1.0K');
  expect(filters.humanReadableBits('2048')).toBe('2.0K');
  expect(filters.humanReadableBits('1024000')).toBe('1000K');
  expect(filters.humanReadableBits('2048000')).toBe('2.0M');
  expect(filters.humanReadableBits('1099500000')).toBe('1.0G');
  expect(filters.humanReadableBits('1125899900000')).toBe('1.0T');
  expect(filters.humanReadableBits('1152921500000000')).toBe('1.0P');
});

test('humanReadableBytes', () => {
  expect(filters.humanReadableBytes('asdf')).toBe('0'); // default
  expect(filters.humanReadableBytes('1')).toBe('1Bi');
  expect(filters.humanReadableBytes('1234')).toBe('1.2Ki');
  expect(filters.humanReadableBytes('1024')).toBe('1.0Ki');
  expect(filters.humanReadableBytes('2048')).toBe('2.0Ki');
  expect(filters.humanReadableBytes('1024000')).toBe('1.0Mi');
  expect(filters.humanReadableBytes('2048000')).toBe('2.0Mi');
  expect(filters.humanReadableBytes('1099500000')).toBe('1.0Gi');
  expect(filters.humanReadableBytes('1125899900000')).toBe('1.0Ti');
  expect(filters.humanReadableBytes('1152921500000000')).toBe('1.0Pi');
});

test('humanReadableNumber', () => {
  expect(commonFilters.humanReadableNumber('a')).toBe('0 ');
  expect(commonFilters.humanReadableNumber(1)).toBe('1 ');
  expect(commonFilters.humanReadableNumber(1000)).toBe('1.0k');
  expect(commonFilters.humanReadableNumber(10000)).toBe('10k');
  expect(commonFilters.humanReadableNumber(1000000)).toBe('1.0M');
  expect(commonFilters.humanReadableNumber(20000000)).toBe('20M');
  expect(commonFilters.humanReadableNumber(1000000000)).toBe('1.0G');
  expect(commonFilters.humanReadableNumber(321000000000)).toBe('321G');
  expect(commonFilters.humanReadableNumber(4000000000000)).toBe('4.0T');
  expect(commonFilters.humanReadableNumber(80000000000000)).toBe('80T');
  expect(commonFilters.humanReadableNumber(9000000000000000)).toBe('9.0P');
  expect(commonFilters.humanReadableNumber(987600000000000000)).toBe('988P');
});

test('timezoneDateString', () => {
  expect(filters.timezoneDateString('a')).toBe('Invalid date');
  expect(filters.timezoneDateString(0, 'gmt')).toBe('1970/01/01 00:00:00 UTC');
  expect(filters.timezoneDateString(1624024589000, 'gmt')).toBe('2021/06/18 13:56:29 UTC');
  expect(filters.timezoneDateString(1234567898765, 'gmt', true)).toBe('2009/02/13 23:31:38.765 UTC');
});

test('readableTime', () => {
  expect(filters.readableTime('a')).toBe('?');
  expect(filters.readableTime(0)).toBe('0');
  expect(filters.readableTime(100)).toBe('0');
  expect(filters.readableTime(60000)).toBe('00:01:00');
  expect(filters.readableTime(240000)).toBe('00:04:00');
  expect(filters.readableTime(3600000)).toBe('01:00:00');
  expect(filters.readableTime(18000000)).toBe('05:00:00');
  expect(filters.readableTime(86400000)).toBe('1 day ');
  expect(filters.readableTime(432000000)).toBe('5 days ');
  expect(filters.readableTime(450067000)).toBe('5 days 05:01:07');
});

test('readableTime', () => {
  expect(filters.readableTimeCompact('a')).toBe('?');
  expect(filters.readableTimeCompact(0)).toBe('0h');
  expect(filters.readableTimeCompact(100)).toBe('0h');
  expect(filters.readableTimeCompact(60000)).toBe('0h');
  expect(filters.readableTimeCompact(240000)).toBe('0h');
  expect(filters.readableTimeCompact(3600000)).toBe('1h');
  expect(filters.readableTimeCompact(18000000)).toBe('5h');
  expect(filters.readableTimeCompact(86400000)).toBe('1d 0h');
  expect(filters.readableTimeCompact(432000000)).toBe('5d 0h');
  expect(filters.readableTimeCompact(450067000)).toBe('5d 5h');
});

test('searchFields', () => {
  expect(filters.searchFields(null, fields).length).toBe(fields.length - 4);
  expect(filters.searchFields('', fields).length).toBe(fields.length - 4);
  expect(filters.searchFields('src bytes', fields)[0].exp).toBe('bytes.src');
  expect(filters.searchFields('ip.dst:port', fields)[0].exp).toBe('ip.dst');
  expect(filters.searchFields('Dst IP', fields)[0].exp).toBe('ip.dst');
  expect(filters.searchFields('host', fields)[0].exp).toBe('host.http.tokens');
  expect(filters.searchFields('Hostname Tokens', fields, true).length).toBe(0);
  expect(filters.searchFields('filename', fields, false, true).length).toBe(0);
  expect(filters.searchFields('info', fields, false, false, true).length).toBe(0);
});

test('buildExpression', () => {
  expect(filters.buildExpression('ip.dst', '10.0.0.1', '==')).toBe('ip.dst == 10.0.0.1');
  expect(filters.buildExpression('exp', 'EXISTS!', '==')).toBe('exp == EXISTS!');
  // eslint-disable-next-line no-useless-escape
  expect(filters.buildExpression('http.hasheader.dst.value', 'h3-28=":4433"; ma=3600', '!=')).toBe('http.hasheader.dst.value != "h3-28=\\\":4433\\\"; ma=3600"');
  expect(filters.buildExpression('http.hasheader.src.value', 'gzip, deflate', '==')).toBe('http.hasheader.src.value == "gzip, deflate"');
  expect(filters.buildExpression('asn.dst', 'long string with spaces in it', '==')).toBe('asn.dst == "long string with spaces in it"');
});

test('searchCluster', () => {
  expect(filters.searchCluster('ES1', ['ES1', 'ES2', 'ES3'])[0]).toContain('ES1');
  expect(filters.searchCluster('ES', ['ES1', 'ES2', 'ES3'])).toEqual(['ES1', 'ES2', 'ES3']);
});
