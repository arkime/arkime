'use strict';

import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import KeyboardShortcuts from '../KeyboardShortcuts';

test('keyboard shortcuts help', async () => {
  const { getByTitle, getByTestId } = render(KeyboardShortcuts, {
    props: { shortcutsClass: 'test-shortcuts-class' }
  });

  // displays help button if shift is held
  const target = getByTestId('shortcut-test');
  await fireEvent.keyDown(target, { key: 'Shift', code: 'Shift', keyCode: 16, charCode: 0 });
  const displayHelpBtn = getByTitle('Display shortcuts help');

  // hides help button if shift is not held
  await fireEvent.keyUp(target, { key: 'Shift', code: 'Shift', keyCode: 16, charCode: 0 });
  expect(displayHelpBtn).not.toBeInTheDocument();

  // can click display help button to show content
  await fireEvent.keyDown(target, { key: 'Shift', code: 'Shift', keyCode: 16, charCode: 0 });
  await fireEvent.click(displayHelpBtn);
  const closeBtn = getByTitle('Close shortcuts help');

  // shortcuts content applies prop class
  expect(getByTestId('shortcuts-help-content')).toHaveClass('test-shortcuts-class');

  // clicking close button closes the help
  await fireEvent.click(closeBtn);
  expect(closeBtn).not.toBeInTheDocument();
});
