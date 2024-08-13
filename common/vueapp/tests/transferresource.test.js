'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, fireEvent } from '@testing-library/vue';
import TransferResourceTest from './components/transferresource.test.vue';

Vue.use(BootstrapVue);

test('transfer resource modal', async () => {
  const {
    getByTitle, emitted, getByPlaceholderText
  } = render(TransferResourceTest);

  // can open modal
  const transferResourceButton = getByTitle('Transfer Resource');
  await fireEvent.click(transferResourceButton);

  // can't click transfer with no user id filled in
  const transferButton = getByTitle('Transfer Ownership');
  await fireEvent.click(transferButton);
  expect(emitted()).not.toHaveProperty('transfer-resource');

  // can click transfer with user id filled in and it emits the user id to the parent
  const userIdInput = getByPlaceholderText('Enter a single user\'s ID');
  await fireEvent.update(userIdInput, 'test_id');
  expect(userIdInput.value).toBe('test_id');
  await fireEvent.click(transferButton);
  expect(emitted()).toHaveProperty('transfer-resource');
  expect(emitted()['transfer-resource'][0][0]).toStrictEqual({ userId: 'test_id' });

  // can click cancel to close modal
  const cancelButton = getByTitle('Cancel');
  await fireEvent.click(cancelButton);
  expect(emitted()).toHaveProperty('transfer-resource');
  expect(emitted()['transfer-resource'][1][0]).toStrictEqual({ userId: undefined });
});
