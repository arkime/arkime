'use strict';

const filters = require('../src/filters');
const { fields } = require('../../../common/vueapp/tests/consts');

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

test('readableTimeCompact', () => {
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
  expect(filters.searchFields(null, fields)).toHaveLength(fields.length - 4);
  expect(filters.searchFields('', fields)).toHaveLength(fields.length - 4);
  expect(filters.searchFields('src bytes', fields)[0].exp).toBe('bytes.src');
  expect(filters.searchFields('ip.dst:port', fields)[0].exp).toBe('ip.dst');
  expect(filters.searchFields('Dst IP', fields)[0].exp).toBe('ip.dst');
  expect(filters.searchFields('host', fields)[0].exp).toBe('host.http.tokens');
  expect(filters.searchFields('Hostname Tokens', fields, true)).toHaveLength(0);
  expect(filters.searchFields('filename', fields, false, true)).toHaveLength(0);
  expect(filters.searchFields('info', fields, false, false, true)).toHaveLength(0);
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
