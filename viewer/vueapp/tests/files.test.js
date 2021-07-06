'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render, waitFor } from '@testing-library/vue';
import Files from '../src/components/files/Files.vue';
import FileService from '../src/components/files/FileService';
import UserService from '../src/components/users/UserService';
import '../src/filters.js';
const { files, userWithSettings } = require('./consts');

console.info = jest.fn(); // ignore tooltip warnings

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/files/FileService');
jest.mock('../src/components/users/UserService');

const store = {
  state: {
    user: userWithSettings
  }
};

const $route = { query: {} };

test('file page no results', async () => {
  UserService.getState = jest.fn().mockResolvedValue({});
  FileService.get = jest.fn().mockResolvedValue({
    data: {
      data: [],
      recordsTotal: 0,
      recordsFiltered: 2
    }
  });

  const { getByText } = render(Files, {
    store,
    mocks: { $route }
  });

  await waitFor(() => { // displays no results
    getByText('No results match your search');
  });
});

test('file page', async () => {
  UserService.getState = jest.fn().mockResolvedValue({});
  FileService.get = jest.fn().mockResolvedValue({
    data: {
      data: files,
      recordsTotal: 2,
      recordsFiltered: 2
    }
  });

  const { getByText } = render(Files, {
    store,
    mocks: { $route }
  });

  await waitFor(() => { // displays no results
    getByText('/path/to/thefile.pcap');
    getByText('/path/to/anotherfile.pcap');
  });
});
