'use strict';

import { render, fireEvent } from '@testing-library/vue';
import Toast from '../src/components/utils/Toast.vue';

const doneFn = jest.fn(() => {});

const props = {
  message: 'test toast',
  done: doneFn,
  duration: 5000,
  type: 'warning'
};

test('toast', async () => {
  const { getByText, getByTitle, getByRole, updateProps } = render(Toast, { props });

  getByText('test toast');
  getByTitle('warning');

  props.type = 'info';
  await updateProps(props);

  getByTitle('info');

  props.type = 'success';
  await updateProps(props);

  getByTitle('success');

  const closeBtn = getByRole('button');

  expect(doneFn).not.toHaveBeenCalled();

  await fireEvent.click(closeBtn);

  expect(doneFn).toHaveBeenCalled();
});
