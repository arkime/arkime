'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import { render, fireEvent } from '@testing-library/vue';
import WelcomeMessage from '../src/components/utils/WelcomeMessage.vue';
import UserService from '../src/components/users/UserService';
const { users } = require('../../../common/vueapp/tests/consts');

console.warn = jest.fn(); // ignore tooltip warnings

Vue.use(BootstrapVue);

jest.mock('../src/components/users/UserService');

UserService.acknowledgeMsg = jest.fn().mockResolvedValue();

const store = {
  state: {
    user: users[0]
  },
  mutations: {
    setUser (state, value) {
      state.user = value;
    }
  }
};

test('welcome message dismissed', async () => {
  const {
    getByText, getByTitle, queryByText
  } = render(WelcomeMessage, { store });

  getByText('Welcome testuser!');

  const disMsgBtn = getByTitle('But see this message again next time');

  await fireEvent.click(disMsgBtn);

  expect(queryByText('Welcome testuser!')).toBeNull();
});

test('welcome message acknowledged', async () => {
  const {
    getByText, getByTitle, queryByText
  } = render(WelcomeMessage, { store });

  getByText('Welcome testuser!');

  const ackMsgBtn = getByTitle('Don\'t show me this message again');

  await fireEvent.click(ackMsgBtn);

  expect(queryByText('Welcome testuser!')).toBeNull();

  expect(store.state.user.msgNum).toBe(1);
});
