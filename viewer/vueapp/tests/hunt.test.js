'use strict';

import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Hunt from '../src/components/hunt/Hunt.vue';
import HuntService from '../src/components/hunt/HuntService';
import SettingsService from '../src/components/settings/SettingsService';
import SessionsService from '../src/components/sessions/SessionsService';
import UserService from '../src/components/users/UserService';
import ConfigService from '../src/components/utils/ConfigService';
import '../src/filters.js';
import '../../../common/vueapp/vueFilters';
const { roles, hunts, userWithSettings } = require('../../../common/vueapp/tests/consts');

console.info = jest.fn(); // ignore tooltip warnings

global.$ = global.jQuery = $;

Vue.use(VueAxios, axios);
Vue.use(BootstrapVue);

Vue.prototype.$constants = {
  HUNTWARN: 10000,
  HUNTLIMIT: 1000000,
  ANONYMOUS_MODE: false
};

jest.mock('../src/components/hunt/HuntService');
jest.mock('../src/components/users/UserService');
jest.mock('../src/components/sessions/SessionsService');
jest.mock('../src/components/settings/SettingsService');

HuntService.userHasHuntRole = jest.fn((user, hunt) => {
  if (hunt.roles.length) {
    for (const role of hunt.roles) {
      if (user.roles.indexOf(role) > -1) {
        return true;
      }
    }
  }

  return false;
});
HuntService.canEditHunt = jest.fn((user, hunt) => {
  const userRoles = user.roles || [];
  return user.userId === hunt.userId || userRoles.includes('arkimeAdmin');
});
HuntService.canViewHunt = jest.fn((user, hunt) => {
  const huntUsers = hunt.users || [];
  const userRoles = user.roles || [];

  return user.userId === hunt.userId ||
    userRoles.includes('arkimeAdmin') ||
    huntUsers.includes(user.userId) ||
    HuntService.userHasHuntRole(user, hunt);
});
HuntService.isShared = jest.fn((user, hunt) => {
  const huntUsers = hunt.users || [];
  return HuntService.userHasHuntRole(user, hunt) || huntUsers.includes(user.userId);
});

const store = {
  state: {
    user: userWithSettings,
    expression: '',
    timeRange: -1,
    time: { startTime: 0, stopTime: 0 },
    esCluster: {
      availableCluster: {
        active: [],
        inactive: []
      },
      selectedCluster: []
    },
    roles
  },
  mutations: {
    setAvailableCluster () {},
    setSelectedCluster () {},
    setFocusSearch () {},
    setViews () {},
    setTime: jest.fn(),
    setTimeRange: jest.fn(),
    setExpression: jest.fn(),
    setIssueSearch: jest.fn(),
    toggleHideViz: jest.fn(),
    toggleStickyViz: jest.fn()
  }
};

const $route = { query: {}, path: 'http://localhost:8123/arkime/hunt' };

beforeEach(() => {
  ConfigService.getClusters = jest.fn().mockResolvedValue({
    data: { active: [], inactive: [] }
  });
  UserService.getViews = jest.fn().mockResolvedValue({});
  UserService.getState = jest.fn().mockResolvedValue({ data: {} });
  SettingsService.getNotifiers = jest.fn().mockResolvedValue({ data: {} });
});

test('hunt page no results', async () => {
  SessionsService.get = jest.fn().mockResolvedValue({
    data: {
      data: [],
      recordsTotal: 1000001,
      recordsFiltered: 1000001
    }
  });
  HuntService.get = jest.fn().mockResolvedValue({
    data: {
      data: [],
      recordsTotal: 0,
      recordsFiltered: 0
    }
  });

  const { getByText } = render(Hunt, {
    store,
    mocks: { $route }
  });

  await waitFor(() => { // displays no results
    getByText('There are currently no packet search jobs in the history.');
  });
});

test('hunt page form cancel', async () => {
  SessionsService.get = jest.fn().mockResolvedValue({
    data: {
      data: [],
      recordsTotal: 1000001,
      recordsFiltered: 1000001
    }
  });
  HuntService.get = jest.fn().mockResolvedValue({
    data: {
      data: hunts,
      recordsTotal: 0,
      recordsFiltered: 0
    }
  });

  const {
    getAllByText, getByText, getByTitle, getByPlaceholderText, queryByPlaceholderText
  } = render(Hunt, {
    store,
    mocks: { $route }
  });

  // hunt tables display results  ------------------------------------------ //
  await waitFor(() => { getAllByText(hunts[0].id); });

  for (const hunt of hunts) {
    getAllByText(hunt.id);
    getAllByText(hunt.name);
    getAllByText(hunt.search);
  }

  // form is closed to start ----------------------------------------------- //
  expect(queryByPlaceholderText('Name your packet search job')).not.toBeInTheDocument();

  // form can be opened ---------------------------------------------------- //
  const openFormBtn = getByTitle('Open a form to create a new hunt');
  fireEvent.click(openFormBtn); // open the create hunt form

  let huntNameInput, huntSearchInput;

  await waitFor(() => { // form is open
    huntNameInput = getByPlaceholderText('Name your packet search job');
    huntSearchInput = getByPlaceholderText('Search packets for');
  });

  // form can be canceled -------------------------------------------------- //
  fireEvent.update(huntNameInput, 'coconut hunt');
  fireEvent.update(huntSearchInput, 'coconut');

  expect(huntNameInput.value).toBe('coconut hunt');
  expect(huntSearchInput.value).toBe('coconut');

  const cancelCreateBtn = getByTitle('Cancel creating this hunt');
  fireEvent.click(cancelCreateBtn); // cancel creating the hunt

  await waitFor(() => { // cancel form hides form
    expect(queryByPlaceholderText('Name your packet search job')).not.toBeInTheDocument();
  });

  fireEvent.click(openFormBtn); // open the create hunt form

  await waitFor(() => { // form is open
    huntNameInput = getByPlaceholderText('Name your packet search job');
    huntSearchInput = getByPlaceholderText('Search packets for');
  });

  // cancel form clears inputs
  expect(huntNameInput.value).toBe('');
  expect(huntSearchInput.value).toBe('');

  // can't create a hunt with too many sessions ---------------------------- //
  const createHuntBtn = getByTitle('Create this hunt');
  fireEvent.click(createHuntBtn); // click create hunt

  await waitFor(() => {
    getByText('This hunt applies to too many sessions. Narrow down your session search to less than 1000000 first.');
  });
});

test('hunt page create hunt and form validation', async () => {
  SessionsService.get = jest.fn().mockResolvedValue({
    data: {
      data: [],
      recordsTotal: 100,
      recordsFiltered: 100
    }
  });
  HuntService.get = jest.fn().mockResolvedValue({
    data: {
      data: hunts,
      recordsTotal: 0,
      recordsFiltered: 0
    }
  });
  HuntService.create = jest.fn().mockResolvedValue({
    data: { success: true, text: 'yay' }
  });
  HuntService.cleanup = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });
  HuntService.delete = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });
  HuntService.cancel = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });
  HuntService.pause = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });
  HuntService.play = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });
  HuntService.addUsers = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });
  HuntService.removeUser = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });

  const {
    getByText, queryByText, getByTitle, getAllByTitle, getAllByRole,
    getByPlaceholderText, queryByPlaceholderText
  } = render(Hunt, {
    store,
    mocks: { $route }
  });

  // can't create a hunt without required fields --------------------------- //
  const openFormBtn = getByTitle('Open a form to create a new hunt');
  fireEvent.click(openFormBtn); // open the create hunt form

  let createHuntBtn;
  await waitFor(() => {
    createHuntBtn = getByTitle('Create this hunt');
  });

  fireEvent.click(createHuntBtn); // click create hunt

  await waitFor(() => { // can't create hunt without a name
    getByText('Hunt name required');
  });

  let huntNameInput = getByPlaceholderText('Name your packet search job');
  fireEvent.update(huntNameInput, 'coconut hunt');

  await waitFor(() => { // can't create hunt without a name
    getByText('Hunt name required');
  });

  let huntSearchInput = getByPlaceholderText('Search packets for');

  fireEvent.click(createHuntBtn); // click create hunt

  await waitFor(() => { // can't create hunt without search text
    getByText('Hunt search text required');
  });

  fireEvent.update(huntSearchInput, 'coconut');

  const srcCheckbox = getAllByRole('checkbox')[0];
  const dstCheckbox = getAllByRole('checkbox')[1];
  fireEvent.click(srcCheckbox);
  fireEvent.click(dstCheckbox);

  fireEvent.click(createHuntBtn); // click create hunt

  await waitFor(() => { // can't create hunt without searching src and/or dst
    getByText('The hunt must search source or destination packets (or both)');
  });

  // can create hunt ------------------------------------------------------- //
  fireEvent.click(srcCheckbox);
  fireEvent.click(dstCheckbox);

  fireEvent.click(createHuntBtn); // click create hunt

  await waitFor(() => { // create issued and form closed
    expect(HuntService.create).toHaveBeenCalledWith(expect.objectContaining({
      name: 'coconut hunt',
      size: 50,
      search: 'coconut',
      searchType: 'ascii',
      type: 'reassembled',
      src: true,
      dst: true,
      totalSessions: 100,
      notifier: undefined,
      users: ''
    })); // NOTE: excludes query because cancelId is random
    expect(queryByPlaceholderText('Name your packet search job')).not.toBeInTheDocument();
  });

  // can cleanup sessions -------------------------------------------------- //
  const cleanupBtn = getAllByTitle('Remove the hunt name and ID fields from the matched sessions.')[0];
  await fireEvent.click(cleanupBtn);
  expect(HuntService.cleanup).toHaveBeenCalledWith(hunts[0].id);

  // can delete hunts ------------------------------------------------------ //
  const id = hunts[1].id;
  const deleteBtn = getAllByTitle('Remove this hunt')[1];
  await fireEvent.click(deleteBtn);
  expect(HuntService.delete).toHaveBeenCalledWith(id);
  expect(queryByText(id)).toBeNull(); // removed from the list

  // can cancel hunts ------------------------------------------------------ //
  const cancelBtn = getAllByTitle('Cancel this hunt. It can be viewed in the history after the cancelation is complete.')[1];
  await fireEvent.click(cancelBtn);
  expect(HuntService.cancel).toHaveBeenCalledWith(hunts[1].id);

  // can pause hunts ------------------------------------------------------- //
  const pauseBtn = getAllByTitle('Pause this hunt')[1];
  await fireEvent.click(pauseBtn);
  expect(HuntService.pause).toHaveBeenCalledWith(hunts[2].id);

  // can play hunts -------------------------------------------------------- //
  const playBtn = getAllByTitle('Play this hunt')[1];
  await fireEvent.click(playBtn);
  expect(HuntService.play).toHaveBeenCalledWith(hunts[3].id);

  // can add users to hunts ------------------------------------------------ //
  const toggleBtn = getAllByRole('button')[0];
  await fireEvent.click(toggleBtn); // toggle the hunt detail row open
  const removeUserBtn = getAllByTitle("Remove this user's access from this hunt")[0];
  const showAddUsersBtn = getAllByTitle('Share this hunt with user(s)')[0];
  await fireEvent.click(showAddUsersBtn); // toggle show add users button
  const addUsersInput = getByPlaceholderText('Comma separated list of user IDs');
  await fireEvent.update(addUsersInput, 'user1,user2');
  const addUsersBtn = getByTitle('Give these users access to this hunt');
  await fireEvent.click(addUsersBtn); // click to add the users
  expect(HuntService.addUsers).toHaveBeenCalledWith(hunts[0].id, 'user1,user2');

  // can remove user from hunts -------------------------------------------- //
  await fireEvent.click(removeUserBtn);
  expect(HuntService.removeUser).toHaveBeenCalledWith(hunts[0].id, 'test1');

  // can rerun hunts ------------------------------------------------------- //
  const rerunBtn = getAllByTitle('Rerun this hunt using the current time frame and search criteria.')[0];
  await fireEvent.click(rerunBtn);

  await waitFor(() => {
    huntNameInput = getByPlaceholderText('Name your packet search job');
    expect(huntNameInput.value).toBe(hunts[0].name);
    huntSearchInput = getByPlaceholderText('Search packets for');
    expect(huntSearchInput.value).toBe(hunts[0].search);
  });

  // can rerun hunts ------------------------------------------------------- //
  const repeatBtn = getAllByTitle('Repeat this hunt using its time frame and search criteria.')[1];
  await fireEvent.click(repeatBtn);

  await waitFor(() => { // calls rerun job and issues state mutations
    huntNameInput = getByPlaceholderText('Name your packet search job');
    expect(huntNameInput.value).toBe(hunts[1].name);
    huntSearchInput = getByPlaceholderText('Search packets for');
    expect(huntSearchInput.value).toBe(hunts[1].search);

    expect(store.mutations.setTime).toHaveBeenCalledWith(store.state, {
      stopTime: hunts[1].query.stopTime,
      startTime: hunts[1].query.startTime
    });
    expect(store.mutations.setTimeRange).toHaveBeenLastCalledWith(store.state, '0');
    expect(store.mutations.setIssueSearch).toHaveBeenCalledWith(store.state, true);
    expect(store.mutations.setExpression).toHaveBeenCalledWith(store.state, hunts[1].query.expression);
  });
});

test('hunt update', async () => {
  HuntService.get = jest.fn().mockResolvedValue({
    data: {
      data: hunts,
      recordsTotal: 0,
      recordsFiltered: 0
    }
  });
  HuntService.updateHunt = jest.fn().mockResolvedValue({
    data: { succes: true, text: 'yay' }
  });

  const {
    getAllByText, getByTitle, getAllByTitle, getAllByRole,
    getByPlaceholderText, queryByPlaceholderText
  } = render(Hunt, {
    store,
    mocks: { $route }
  });

  const toggleBtn = getAllByRole('button')[0];
  await fireEvent.click(toggleBtn); // toggle the hunt detail row open

  // can update hunt description ------------------------------------------- //
  const editDescBtn = getAllByTitle('Edit description')[0];
  await fireEvent.click(editDescBtn);
  const descInput = getByPlaceholderText('Update the description');
  fireEvent.update(descInput, 'amazing description');
  const cancelBtn = getByTitle('Cancel hunt description update');
  await fireEvent.click(cancelBtn); // cancel button hides input
  expect(queryByPlaceholderText('Update the description')).not.toBeInTheDocument();
  await fireEvent.click(editDescBtn);
  fireEvent.update(descInput, 'amazing description');
  const saveBtn = getByTitle('Save hunt description');
  fireEvent.click(saveBtn);

  expect(HuntService.updateHunt).toHaveBeenCalledWith(hunts[0].id, {
    description: 'amazing description',
    roles: ['arkimeUser']
  });

  // can add roles to hunts ------------------------------------------------ //
  const roleDropdown = getAllByText('arkimeUser')[0];
  await fireEvent.click(roleDropdown); // click to open the dropdown
  const roleCheckbox = getAllByRole('checkbox')[3];
  await fireEvent.click(roleCheckbox);

  expect(HuntService.updateHunt).toHaveBeenCalledWith(hunts[0].id, {
    description: 'amazing description',
    roles: ['arkimeUser', 'cont3xtUser']
  });
});

test('role can not see hunt details', async () => {
  HuntService.get = jest.fn().mockResolvedValue({
    data: {
      data: hunts,
      recordsTotal: 0,
      recordsFiltered: 0
    }
  });

  store.state.user.roles = ['uselessRole'];

  const {
    queryByTitle, getAllByText, queryByText
  } = render(Hunt, {
    store,
    mocks: { $route }
  });

  // need to wait for table to render
  await waitFor(() => { getAllByText(hunts[0].name); });

  // no toggle detail buttons
  expect(queryByTitle('toggle')).not.toBeInTheDocument();

  // no open hunt matches button
  expect(queryByTitle('Open results in a new Sessions tab.')).not.toBeInTheDocument();

  for (const hunt of hunts) { // secret fields hidden
    expect(queryByText(hunt.id)).not.toBeInTheDocument();
  }
});

test('role can see but not edit hunt', async () => {
  HuntService.get = jest.fn().mockResolvedValue({
    data: {
      data: hunts,
      recordsTotal: 0,
      recordsFiltered: 0
    }
  });

  store.state.user.roles = ['arkimeUser'];

  const {
    queryByTitle, getAllByText, queryByText, getAllByRole
  } = render(Hunt, {
    store,
    mocks: { $route }
  });

  // need to wait for table to render
  await waitFor(() => { getAllByText(hunts[0].name); });

  // no remove button
  expect(queryByTitle('Remove this hunt')).not.toBeInTheDocument();

  // can view details
  for (const hunt of hunts) {
    getAllByText(hunt.id);
  }

  // can toggle open details
  const toggleBtn = getAllByRole('button')[0];
  await fireEvent.click(toggleBtn); // toggle the hunt detail row open

  // but can't edit anything
  expect(queryByText('arkimeUser')).not.toBeInTheDocument();
  expect(queryByTitle('Edit description')).not.toBeInTheDocument();
  expect(queryByTitle('Share this hunt with user(s)')).not.toBeInTheDocument();
});
