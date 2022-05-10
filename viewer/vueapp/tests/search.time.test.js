'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render, fireEvent } from '@testing-library/vue';
import userEvent from '@testing-library/user-event';
import Time from '../src/components/search/Time.vue';
import '../src/filters.js';
const { userWithSettings } = require('../../../common/vueapp/tests/consts');

console.info = jest.fn(); // don't display console.info messages
console.warn = jest.fn(); // don't display console.warn messages

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/users/UserService');
jest.mock('../src/components/search/FieldService');

const timeRange = 0;
const stopTime = 1626343200;
const startTime = 1626328800;

const store = {
  state: {
    timeRange,
    expression: '',
    shiftKeyHold: false,
    user: userWithSettings,
    focusTimeRange: undefined,
    // startTime = 2021/07/15 06:00:00 UTC, stopTime = 2021/07/15 10:00:00 UTC
    time: { startTime, stopTime }
  },
  mutations: {
    setTime (state, value) {
      if (value.startTime !== undefined) {
        state.time.startTime = value.startTime.toString();
      }
      if (value.stopTime !== undefined) {
        state.time.stopTime = value.stopTime.toString();
      }
    },
    setTimeRange (state, value) {
      state.timeRange = value.toString();
    },
    setExpression (state, value) {
      state.expression = value;
    },
    setFocusTimeRange (state, value) {
      state.focusTimeRange = value;
    }
  }
};

test('search time', async () => {
  const $route = { query: { date: timeRange, stopTime, startTime } };
  const $router = { push: jest.fn() };

  const { getByRole, getByText, getByTitle } = render(Time, {
    store,
    mocks: { $route, $router },
    props: { timezone: 'gmt' } // use gmt/utc! who knows where this is being run
  });

  // start time go to beginning of current/previous ------------------------ //
  await fireEvent.click(getByTitle('Beginning of this day'));
  expect(store.state.timeRange).toBe('0'); // custom time
  expect(store.state.time.startTime).toBe(1626307200);
  getByText('10:00:00'); // sets the delta display time
  await fireEvent.click(getByTitle('Beginning of previous day'));
  expect(store.state.time.startTime).toBe(1626220800);

  // start time go to beginning of next day -------------------------------- //
  await fireEvent.click(getByTitle('Beginning of next day'));
  expect(store.state.time.startTime).toBe(1626307200);

  // stop time go to end of next/current day ------------------------------- //
  await fireEvent.click(getByTitle('End of this day'));
  expect(store.state.time.stopTime).toBe(1626393599);
  await fireEvent.click(getByTitle('End of next day'));
  expect(store.state.time.stopTime).toBe(1626479999);

  // stop time go to end of previous day ----------------------------------- //
  await fireEvent.click(getByTitle('End of previous day'));
  await fireEvent.click(getByTitle('End of previous day'));
  expect(store.state.time.stopTime).toBe(1626307199);
  getByText('Stop time cannot be before start time');

  // update time range ----------------------------------------------------- //
  const timeRangeSelect = getByRole('listbox');
  await userEvent.selectOptions(timeRangeSelect, '1');
  expect(timeRangeSelect.value).toBe('1');
  expect($router.push).toHaveBeenCalledWith({
    query: {
      date: '1',
      expression: '',
      stopTime: undefined,
      startTime: undefined
    }
  });
});
