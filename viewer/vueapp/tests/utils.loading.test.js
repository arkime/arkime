'use strict';

import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import LoadingComponent from '../src/components/utils/Loading.vue';
const { userWithSettings } = require('../../../common/vueapp/tests/consts');

const props = { canCancel: false };
const store = { state: { user: userWithSettings } };

test('loading', async () => {
  const {
    getByText, queryByText, getByTitle, updateProps, emitted
  } = render(LoadingComponent, { props, store });

  getByText('I\'m hootin');

  expect(queryByText('cancel')).toBeNull(); // can't see dropdown yet

  await updateProps({ canCancel: true });

  const cancelBtn = getByText('cancel'); // can use cancel button
  await fireEvent.click(cancelBtn);
  expect(emitted()).toHaveProperty('cancel');

  const image = getByTitle('Arkime Logo'); // logo bounces on click
  await fireEvent.click(image);
  expect(image).toHaveClass('bouncing');
});

test('loading - watching', async () => {
  store.state.user.settings.shiftyEyes = true;
  const { getByTitle } = render(LoadingComponent, { props, store });

  getByTitle("I'm watching you");
});
