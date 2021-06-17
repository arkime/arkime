const filters = require('../src/filters');

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
  expect(filters.roundCommaString(123456789.987654321, 4)).toBe('123,456,789.9877');
});

test('extractIPv6String', () => {
  expect(filters.extractIPv6String()).toBe(''); // default
  expect(filters.extractIPv6String('2001:4998:ef83:14:8000::100d')).toBe('2001::499:8:ef:83:1:4:80::::100d');
  expect(filters.extractIPv6String('1111:2222:3333:4444:5555:7777::')).toBe('1111::222:2:33:33:4:444::5555::777:7::');
});

test('round', () => {
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
