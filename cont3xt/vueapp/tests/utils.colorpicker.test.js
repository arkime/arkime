'use strict';

import { render, fireEvent } from '@testing-library/vue';
import ColorPicker from '../src/utils/ColorPicker.vue';

// canvas mocks
HTMLCanvasElement.prototype.getContext = () => {
  return {
    fillRect: () => { return; },
    translate: () => { return; }
  };
};
HTMLCanvasElement.prototype.toDataURL = () => {
  return {};
};

test('color picker', async () => {
  const { getByTestId, emitted, getByLabelText } = render(ColorPicker, {
    props: {
      index: 0,
      linkName: 'testLinkName'
    }
  });

  // color picker shows up
  const pickerBtn = getByTestId('picker-btn');
  await fireEvent.click(pickerBtn); // can click button

  // and it displays the picker
  const input = getByTestId('picker');
  const hexInput = getByLabelText(/hex/);
  await fireEvent.update(hexInput, '#B30071'); // can change color

  // escape exits the color picker and emits color selected event to parent
  await fireEvent.keyUp(input, { key: 'Escape', code: 'Escape', keyCode: 27, charCode: 0 });
  expect(emitted()).toHaveProperty('colorSelected');
  expect(emitted().colorSelected[0][0].index).toBe(0);
  expect(emitted().colorSelected[0][0].color).toBe('#B30071');
  expect(emitted().colorSelected[0][0].linkName).toBe('testLinkName');
});
