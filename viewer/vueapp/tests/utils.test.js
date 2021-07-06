'use strict';

import Utils from '../src/components/utils/utils';

test('findFactors', () => {
  expect(Utils.findFactors(1)).toStrictEqual([]);
  expect(Utils.findFactors(12)).toStrictEqual([1, 2, 3, 4, 6]);
  expect(Utils.findFactors(20)).toStrictEqual([1, 2, 4, 5, 10]);
});

test('createRandomString', () => {
  expect(Utils.createRandomString()).not.toBe(Utils.createRandomString());
});
