'use strict';

import '@testing-library/jest-dom';
import { render } from '@testing-library/vue';
import Focus from '../Focus.vue';
import FocusComponent from './components/focus.test.vue';

test('focus element', async () => {
  const { getByTestId, updateProps } = render(FocusComponent, {
    props: { focus: false }
  }, vue => vue.directive('Focus', Focus));

  const input = getByTestId('focus-test');
  expect(input).not.toHaveFocus();

  await updateProps({ focus: true });

  expect(input).toHaveFocus();
});
