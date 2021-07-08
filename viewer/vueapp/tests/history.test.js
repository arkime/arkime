'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import HistoryComponent from '../src/components/history/History.vue';
import HistoryService from '../src/components/history/HistoryService';
import UserService from '../src/components/users/UserService';
import HasPermission from '../src/components/utils/HasPermission.vue';
import '../src/filters.js';
const { histories, userWithSettings } = require('./consts');

console.info = jest.fn(); // ignore tooltip warnings

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);
Vue.directive('has-permission', HasPermission);

jest.mock('../src/components/history/HistoryService');
jest.mock('../src/components/users/UserService');

UserService.hasPermission = jest.fn(() => true);

const store = {
  state: {
    user: userWithSettings,
    time: {
      startTime: 0,
      stopTime: 0
    }
  },
  mutations: {
    setTimeRange (state, value) {}
  }
};

const $route = { query: {} };

test('history page no history', async () => {
  HistoryService.get = jest.fn().mockResolvedValue({
    data: {
      data: [],
      recordsTotal: 0,
      recordsFiltered: 1
    }
  });

  const { getByText } = render(HistoryComponent, {
    store,
    mocks: { $route }
  });

  await waitFor(() => { // displays no results
    getByText('No history or none that matches your search');
  });
});

test('history page', async () => {
  HistoryService.get = jest.fn().mockResolvedValue({
    data: {
      data: histories,
      recordsTotal: 0,
      recordsFiltered: 1
    }
  });
  HistoryService.delete = jest.fn().mockResolvedValue({
    data: {
      success: true,
      text: 'Deleted history item successfully'
    }
  });

  const {
    getByText, getByPlaceholderText, getByTitle, queryByText
  } = render(HistoryComponent, {
    store,
    mocks: { $route }
  });

  // history exists in the table results ----------------------------------- */
  await waitFor(() => {
    getByText('/api/users');
  });

  // can search history ---------------------------------------------------- */
  const searchInput = getByPlaceholderText('Search for history in the table below');

  await fireEvent.update(searchInput, 'test'); // update search input

  await waitFor(() => { // search issued
    expect(HistoryService.get).toHaveBeenCalled();
  });

  // can open history item ------------------------------------------------- */
  const openBtn = getByTitle('Open this query on the users page');
  expect(openBtn.href).toBe('http://localhost/users?id=admin');

  // can delete history ---------------------------------------------------- */
  const deletedHistory = histories[0];
  const deleteBtn = getByTitle('Delete history');
  await fireEvent.click(deleteBtn); // click the delete btn

  expect(HistoryService.delete).toHaveBeenCalled(); // delete was called
  expect(HistoryService.delete).toHaveBeenCalledWith(deletedHistory.id, deletedHistory.index);

  expect(queryByText('/api/users')).not.toBeInTheDocument(); // history removed

  getByText('Deleted history item successfully'); // displays delete success
});
