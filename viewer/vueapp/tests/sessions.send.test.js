'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import Send from '../src/components/sessions/Send.vue';
import SessionsService from '../src/components/sessions/SessionsService';
import { sessions } from './consts';

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

test('sessions - send', async () => {
  SessionsService.send = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByTitle, getByPlaceholderText, getByText } = render(Send, {
    mocks: { $route },
    props
  });

  // has all fields and info displayed on init ----------------------------- //
  const sendBtn = getByTitle('Send Session(s)');
  const tagInput = getByPlaceholderText('Enter a comma separated list of tags');
  getByText('This will send the SPI and PCAP data to the remote Arkime instance.');

  // can update tag input and it sends it to send func  -------------------- //
  await fireEvent.update(tagInput, 'tag1,tag2');

  // calls remove service -------------------------------------------------- //
  await fireEvent.click(sendBtn);
  expect(SessionsService.send).toHaveBeenCalledWith({
    start: props.start,
    applyTo: props.applyTo,
    segments: 'no',
    sessions,
    numVisible: 1,
    numMatching: 1,
    tags: 'tag1,tag2'
  }, {});
});
