'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, waitFor, fireEvent } from '@testing-library/vue';
import Shortcuts from '../src/components/settings/Shortcuts.vue';
import SettingsService from '../src/components/settings/SettingsService';
import '../../../common/vueapp/vueFilters';
const {
  shortcuts, userWithSettings, roles
} = require('../../../common/vueapp/tests/consts');

Vue.use(BootstrapVue);

Vue.prototype.$constants = {
  HASUSERSES: false
};

jest.mock('../src/components/settings/SettingsService');

const store = {
  state: {
    roles,
    user: userWithSettings
  }
};

const $route = { query: {} };

const $router = {
  push: jest.fn()
};

SettingsService.getShortcuts = jest.fn().mockResolvedValue({
  data: shortcuts,
  recordsTotal: 1,
  recordsFiltered: 1
});
SettingsService.updateShortcut = jest.fn().mockResolvedValue({
  text: 'updateShortcut YAY!',
  shortcut: shortcuts[0]
});
SettingsService.deleteShortcut = jest.fn().mockResolvedValue({ text: 'deleteShortcut YAY!' });
SettingsService.createShortcut = jest.fn().mockResolvedValue({ text: 'createShortcut YAY!' });

test('shortcuts', async () => {
  const {
    getByText, getByTitle, getByPlaceholderText, getAllByTitle, emitted, queryByText
  } = render(Shortcuts, {
    store,
    mocks: { $route, $router }
  });

  // renders initial shortcuts --------------------------------------------- //
  await waitFor(() => { // wait for shortcuts fetch to return
    getByText(shortcuts[0].name);
  });

  // create shortcut form validation --------------------------------------- //
  const openCreateFormBtn = getByText('New Shortcut');
  await fireEvent.click(openCreateFormBtn);
  getByText('Create New Shortcut'); // displays form
  const createBtn = getByText('Create');
  await fireEvent.click(createBtn);
  getByText('Shortcut name required');
  const nameInput = getByPlaceholderText('MY_ARKIME_VAR');
  const newName = 'MY_VAR';
  await fireEvent.update(nameInput, newName);
  await fireEvent.click(createBtn);
  getByText('Shortcut value(s) required');

  // can create a shortcut ------------------------------------------------- //
  const valuesInput = getByPlaceholderText('Enter a comma or newline separated list of values');
  const newValues = 'a,b,c';
  await fireEvent.update(valuesInput, newValues);
  await fireEvent.click(createBtn);
  expect(SettingsService.createShortcut).toHaveBeenCalledWith({
    users: '',
    roles: [],
    editRoles: [],
    name: newName,
    type: 'string',
    description: '',
    value: newValues
  });

  await waitFor(() => { // create shortcut returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][0][0]).toStrictEqual({ msg: 'createShortcut YAY!' });
  });

  expect(nameInput.value).toBe(''); // clears form
  expect(valuesInput.value).toBe('');

  shortcuts.push({
    description: '',
    id: 'asdfghjklpoiuytrewq',
    name: 'MY_VAR',
    type: 'string',
    userId: 'testuserid',
    value: 'a\nb\nc\n',
    users: '',
    roles: [],
    editRoles: []
  });

  SettingsService.getShortcuts = jest.fn().mockResolvedValue({
    data: shortcuts,
    recordsTotal: 2,
    recordsFiltered: 2
  });

  await waitFor(() => { // wait for shortcuts fetch to return
    getByText(shortcuts[1].name); // displays new shortcut
  });

  // can edit a shortcut --------------------------------------------------- //
  const updateBtn = getAllByTitle('Update this shortcut')[1];
  await fireEvent.click(updateBtn);
  getByText('Edit Shortcut'); // displays edit form
  // and fills out shortcut values
  expect(nameInput.value).toBe(shortcuts[1].name);
  await fireEvent.update(nameInput, 'NEW_NAME');
  await fireEvent.update(valuesInput, 'silly,value');

  const newShortcut = {
    users: '',
    roles: [],
    editRoles: [],
    type: 'string',
    description: '',
    name: 'NEW_NAME',
    value: 'silly,value'
  };

  SettingsService.updateShortcut = jest.fn().mockResolvedValue({
    text: 'updateShortcut YAY!',
    shortcut: { ...newShortcut }
  });

  const saveBtn = getByText('Save');
  await fireEvent.click(saveBtn);

  expect(SettingsService.updateShortcut).toHaveBeenCalledWith(shortcuts[1].id, newShortcut);
  await waitFor(() => { // update shortcut returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][1][0]).toStrictEqual({ msg: 'updateShortcut YAY!' });
  });
  getByText('NEW_NAME'); // displays updated shortcut
  expect(queryByText('MY_VAR')).not.toBeInTheDocument(); // replaced old shortcut

  // can delete shortcut --------------------------------------------------- //
  const deleteBtn = getAllByTitle('Delete this shortcut')[1];
  await fireEvent.click(deleteBtn);
  expect(SettingsService.deleteShortcut).toHaveBeenCalledWith('asdfghjklpoiuytrewq');
  await waitFor(() => { // delete shortcut returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][2][0]).toStrictEqual({ msg: 'deleteShortcut YAY!' });
  });
  expect(queryByText('NEW_NAME')).not.toBeInTheDocument(); // query removed

  // can click see all button ---------------------------------------------- //
  const seeAllBtn = getByTitle('See all the shortcuts that exist for all users (you can because you are an ADMIN!)');
  await fireEvent.click(seeAllBtn);
  expect(SettingsService.getShortcuts).toHaveBeenCalledWith({
    start: 0,
    all: true,
    length: 50,
    desc: false,
    sort: 'name'
  });
  await waitFor(() => { // updates the title of the button
    getByTitle('Just show the shortcuts created by you and shared with you');
  });
});
