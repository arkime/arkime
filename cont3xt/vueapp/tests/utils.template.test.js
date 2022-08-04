'use-strict';

import { applyTemplate } from '../src/utils/applyTemplate';

console.warn = jest.fn(); // don't display console.warn messages

test('applyTemplate', () => {
  const data = {
    value: 'hello',
    other: { val1: 1, val2: '2' }
  };
  const value = data.value;
  const accessibleVars = { value, data };
  const invalid = 'INVALID_CONT3XT_TEMPLATE';
  expect(applyTemplate('<value> <data.value> (<data.other.val1>,<data.other.val2>)', accessibleVars))
    .toBe('hello hello (1,2)');
  expect(applyTemplate('<data.other.val1> <data.other.val3> <data.other.val4.more>', accessibleVars))
    .toBe('1 undefined undefined');
  expect(applyTemplate('&lt;<data.other.val1>&gt; &lt<value>gt;', accessibleVars))
    .toBe('<1> &lthellogt;');
  expect(applyTemplate('<<value>>', accessibleVars)).toBe(invalid);
  expect(applyTemplate('<value<>>', accessibleVars)).toBe(invalid);
  expect(applyTemplate('<>value>', accessibleVars)).toBe(invalid);
  expect(applyTemplate('<va>lue>', accessibleVars)).toBe(invalid);
  expect(applyTemplate('<<value>', accessibleVars)).toBe(invalid);
  expect(applyTemplate('><value>', accessibleVars)).toBe(invalid);
  expect(applyTemplate('<value><', accessibleVars)).toBe(invalid);
  expect(applyTemplate('blah blah > blah', accessibleVars)).toBe(invalid);
  expect(applyTemplate('< blah blah blah', accessibleVars)).toBe(invalid);
});
