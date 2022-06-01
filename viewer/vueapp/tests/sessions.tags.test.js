'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import Tags from '../src/components/sessions/Tags.vue';
import SessionsService from '../src/components/sessions/SessionsService';
import { sessions } from '../../../common/vueapp/tests/consts';

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/sessions/SessionsService');

test('sessions - add tags', async () => {
  const props = {
    sessions,
    start: 0,
    add: true,
    numVisible: 1,
    numMatching: 1,
    done: jest.fn(),
    applyTo: 'matching'
  };

  SessionsService.tag = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByTitle, getByPlaceholderText, getByText } = render(Tags, {
    mocks: { $route },
    props
  });

  // has errors for missing tags input ------------------------------------- //
  const addBtn = getByTitle('Add Tags');
  const tagInput = getByPlaceholderText('Enter a comma separated list of tags');
  await fireEvent.click(addBtn);
  getByText('No tag(s) specified.');

  // can update tag input and it sends it to tag func  --------------------- //
  await fireEvent.update(tagInput, 'tag1,tag2');

  // calls remove service -------------------------------------------------- //
  await fireEvent.click(addBtn);
  expect(SessionsService.tag).toHaveBeenCalledWith(true, {
    start: props.start,
    applyTo: props.applyTo,
    segments: 'no',
    sessions,
    numVisible: 1,
    numMatching: 1,
    tags: 'tag1,tag2'
  }, {});
});

test('sessions - remove tags', async () => {
  const props = {
    sessions,
    start: 0,
    numVisible: 1,
    numMatching: 1,
    done: jest.fn(),
    applyTo: 'matching'
  };

  SessionsService.tag = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {} };

  const { getByTitle, getByPlaceholderText, getByText } = render(Tags, {
    mocks: { $route },
    props
  });

  // has errors for missing tags input ------------------------------------- //
  const addBtn = getByTitle('Remove Tags');
  const tagInput = getByPlaceholderText('Enter a comma separated list of tags');
  await fireEvent.click(addBtn);
  getByText('No tag(s) specified.');

  // can update tag input and it sends it to tag func  --------------------- //
  await fireEvent.update(tagInput, 'tag1,tag2');

  // calls remove service -------------------------------------------------- //
  await fireEvent.click(addBtn);
  expect(SessionsService.tag).toHaveBeenCalledWith(false, {
    start: props.start,
    applyTo: props.applyTo,
    segments: 'no',
    sessions,
    numVisible: 1,
    numMatching: 1,
    tags: 'tag1,tag2'
  }, {});
});
