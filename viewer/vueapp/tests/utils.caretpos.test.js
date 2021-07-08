'use strict';

import { render, fireEvent } from '@testing-library/vue';
import caretPos from '../src/components/utils/CaretPos.vue';
import caretPosComponent from './components/caretpos.test.vue';

test('input cursor position', async () => {
  const { getByPlaceholderText, findByText } = render(caretPosComponent, {}, vue =>
    vue.directive('caretPos', caretPos)
  );

  const input = getByPlaceholderText('text');

  await fireEvent.update(input, 'XYZ'); // NOTE: update does not trigger events
  // need to fire keydown event for CarotPos.vue to fire eventlistener function
  await fireEvent.keyDown(input, { key: 'X', code: 'KeyX', keyCode: 88, charCode: 0 });
  await fireEvent.keyDown(input, { key: 'Y', code: 'KeyY', keyCode: 89, charCode: 0 });
  await fireEvent.keyDown(input, { key: 'Z', code: 'KeyZ', keyCode: 90, charCode: 0 });

  expect(input.value).toBe('XYZ');
  findByText('caret position is 3');

  await fireEvent.keyDown(input, { key: 'ArrowLeft', code: 'ArrowLeft', keyCode: 37, charCode: 0 });
  findByText('caret position is 2');

  await fireEvent.keyDown(input, { key: 'ArrowLeft', code: 'ArrowLeft', keyCode: 37, charCode: 0 });
  findByText('caret position is 1');

  await fireEvent.keyDown(input, { key: 'ArrowRight', code: 'ArrowRight', keyCode: 39, charCode: 0 });
  findByText('caret position is 2');
});
