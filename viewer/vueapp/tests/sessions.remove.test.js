'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import Remove from '../src/components/sessions/Remove.vue';
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

test('sessions - remove', async () => {
  SessionsService.remove = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByTitle, getByLabelText } = render(Remove, {
    mocks: { $route },
    props
  });

  // scrub pcap checkbox checked on inital load  --------------------------- //
  const removeBtn = getByTitle('Remove Data');
  const scrubPcapCheckbox = getByLabelText('Scrub PCAP');
  expect(scrubPcapCheckbox).toBeChecked(); // checked on init

  // calls remove service -------------------------------------------------- //
  await fireEvent.click(removeBtn);
  expect(SessionsService.remove).toHaveBeenCalledWith({
    start: props.start,
    applyTo: props.applyTo,
    segments: 'no',
    sessions,
    numVisible: 1,
    numMatching: 1,
    removePcap: true,
    removeSpi: false
  }, {});
});
