'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Intersection from '../src/components/sessions/Intersection.vue';
import SessionsService from '../src/components/sessions/SessionsService';
import { sessions, fieldsMap } from './consts';

console.info = jest.fn(); // ignore vue devtool messages

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

test('sessions - intersection', async () => {
  SessionsService.viewIntersection = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByText, getByTitle, getByPlaceholderText } = render(Intersection, {
    mocks: { $route },
    props
  });

  // maps to fields in exportFields input ---------------------------------- //
  let fieldsInput;
  await waitFor(async () => {
    fieldsInput = getByPlaceholderText('Comma separated list of fields (in expression field format - see help page)');
    expect(fieldsInput.value).toBe('ip.src,ip.dst');
    await fireEvent.update(fieldsInput, '');
  });

  // intersection has errors for missing fields ---------------------------- //
  const exportBtn = getByTitle('Export Intersection');
  await fireEvent.click(exportBtn);
  await waitFor(() => {
    getByText('No fields to display. Make sure there is a comma separated list of field expression values, please consult the help page for field expression values (click the owl, then the fields section).');
  });

  // calls export service -------------------------------------------------- //
  await fireEvent.update(fieldsInput, 'ip.src,ip.dst');
  await fireEvent.click(exportBtn);
  expect(SessionsService.viewIntersection).toHaveBeenCalledWith({
    counts: 1,
    exp: 'ip.src,ip.dst',
    sort: 'count'
  }, {});

//   cluster: undefined
// counts: 1
// date: "-1"
// exp: "ip.src,country.src,port.src,ip.dst,country.dst,port.dst,packets,databytes,bytes,node"
// expression: undefined
// sort: "count"
// startTime: undefined
// stopTime: undefined
// view: undefined
});
