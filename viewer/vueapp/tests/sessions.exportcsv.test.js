'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import ExportCsv from '../src/components/sessions/ExportCsv.vue';
import SessionsService from '../src/components/sessions/SessionsService';
import { sessions, fieldsMap } from './consts';

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/sessions/SessionsService');

const props = {
  sessions,
  start: 0,
  numVisible: 1,
  numMatching: 1,
  done: jest.fn(),
  applyTo: 'matching',
  fields: [fieldsMap['ip.src'], fieldsMap['ip.dst']]
};

test('export csv', async () => {
  SessionsService.exportCsv = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByText, getByTitle, getByPlaceholderText } = render(ExportCsv, {
    mocks: { $route },
    props
  });

  // maps to fields in exportFields input ---------------------------------- //
  fireEvent.click(getByTitle('Change the fields that are exported'));
  let fieldsInput;
  await waitFor(async () => {
    fieldsInput = getByPlaceholderText('Comma separated list of fields (in database field format - see help page)');
    expect(fieldsInput.value).toBe('source.ip,destination.ip');
    await fireEvent.update(fieldsInput, '');
  });

  // export csv has errors for missing fields ------------------------------ //
  const exportBtn = getByTitle('Export CSV');
  await fireEvent.click(exportBtn);
  await waitFor(() => {
    getByText('No fields to export. Make sure the sessions table has columns.');
  });

  const filenameInput = getByPlaceholderText('Enter a filename');
  await fireEvent.update(filenameInput, '');
  await fireEvent.update(fieldsInput, 'source.ip,destination.ip');
  await fireEvent.click(exportBtn);
  await waitFor(() => {
    getByText('No filename specified.');
  });

  // calls export service -------------------------------------------------- //
  fireEvent.update(filenameInput, 'sessions.csv');
  fireEvent.click(exportBtn);
  expect(SessionsService.exportCsv).toHaveBeenCalledWith({
    start: props.start,
    applyTo: props.applyTo,
    filename: filenameInput.value,
    segments: 'no',
    sessions,
    numVisible: 1,
    numMatching: 1,
    fields: fieldsInput.value
  }, {});
});
