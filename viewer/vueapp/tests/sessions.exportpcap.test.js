'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import ExportPcap from '../src/components/sessions/ExportPcap.vue';
import SessionsService from '../src/components/sessions/SessionsService';
import { sessions } from '../../../common/vueapp/tests/consts';

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/sessions/SessionsService');

const props = {
  sessions,
  start: 0,
  numVisible: 1,
  numMatching: 1,
  done: jest.fn(),
  applyTo: 'matching'
};

test('sessions - export pcap', async () => {
  SessionsService.exportPcap = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByText, getByTitle, getByPlaceholderText } = render(ExportPcap, {
    mocks: { $route },
    props
  });

  // export pcap has errors for missing fields ----------------------------- //
  const exportBtn = getByTitle('Export PCAP');
  const filenameInput = getByPlaceholderText('Enter a filename');
  await fireEvent.update(filenameInput, '');
  await fireEvent.click(exportBtn);
  await waitFor(() => {
    getByText('No filename specified.');
  });

  // calls export service -------------------------------------------------- //
  fireEvent.update(filenameInput, 'sessions.pcap');
  fireEvent.click(exportBtn);
  expect(SessionsService.exportPcap).toHaveBeenCalledWith({
    start: props.start,
    applyTo: props.applyTo,
    filename: filenameInput.value,
    segments: 'no',
    sessions,
    numVisible: 1,
    numMatching: 1
  }, {});
});
