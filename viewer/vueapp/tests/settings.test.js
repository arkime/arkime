'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, waitFor, fireEvent } from '@testing-library/vue';
import Settings from '../src/components/settings/Settings.vue';
import UserService from '../src/components/users/UserService';
import ConfigService from '../src/components/utils/ConfigService';
import SettingsService from '../src/components/settings/SettingsService';
import HasPermission from '../src/components/utils/HasPermission.vue';
import HasRole from '../../../common/vueapp/HasRole.vue';
import Utils from '../src/components/utils/utils';
import '../src/filters.js';
import '../../../common/vueapp/vueFilters';
const {
  userSettings, userWithSettings, fields, shortcuts, users, fieldsMap
} = require('../../../common/vueapp/tests/consts');

console.info = jest.fn(); // ignore vue dev mode info

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

Vue.directive('has-permission', HasPermission);
Vue.directive('has-role', HasRole);

Vue.prototype.$constants = {
  MOLOCH_MULTIVIEWER: false
};

jest.mock('../src/components/users/UserService');
jest.mock('../src/components/utils/ConfigService');
jest.mock('../src/components/settings/SettingsService');

const store = {
  state: {
    user: userWithSettings,
    fieldsArr: fields,
    fieldsMap
  }
};

const $route = {
  query: {
    expression: ''
  }
};

const $router = {
  push: jest.fn(),
  replace: jest.fn()
};

// setting services
SettingsService.getShortcuts = jest.fn().mockResolvedValue(shortcuts);
SettingsService.createShortcut = jest.fn().mockResolvedValue({ text: 'createShortcut YAY!' });
SettingsService.updateShortcut = jest.fn().mockResolvedValue({
  text: 'updateShortcut YAY!',
  shortcut: shortcuts[0]
});
SettingsService.deleteShortcut = jest.fn().mockResolvedValue({ text: 'deleteShortcut YAY!' });
// user services
UserService.getCurrent = jest.fn().mockResolvedValue(userWithSettings);
UserService.getSettings = jest.fn().mockResolvedValue(userSettings);
UserService.saveSettings = jest.fn().mockResolvedValue({ text: 'saveSettings YAY!' });
UserService.resetSettings = jest.fn().mockResolvedValue({ text: 'resetSettings YAY!' });
UserService.changePassword = jest.fn().mockResolvedValue({ text: 'changePassword YAY!' });
// field config services
UserService.getColumnConfigs = jest.fn().mockResolvedValue([{
  ...Utils.getDefaultTableState(),
  name: 'dupe default'
}]);
UserService.deleteColumnConfig = jest.fn().mockResolvedValue({ text: 'deleteColumnConfig YAY!' });
UserService.getSpiviewFields = jest.fn().mockResolvedValue([{
  fields: 'destination.ip:100,source.ip:100',
  name: 'test spiview field config'
}]);
UserService.deleteSpiviewFieldConfig = jest.fn().mockResolvedValue({ text: 'deleteSpiviewFieldConfig YAY!' });
UserService.getState = jest.fn().mockResolvedValue({
  data: {
    visibleHeaders: ['firstPacket', 'lastPacket']
  }
});
// config service
ConfigService.getMolochClusters = jest.fn().mockResolvedValue({
  test2: { name: 'Test2', url: 'http://localhost:8124' }
});

test('settings - self', async () => {
  const {
    getByText, getAllByText, getByRole, getByTitle, queryByText
  } = render(Settings, {
    store,
    mocks: { $route, $router }
  });

  // renders --------------------------------------------------------------- //
  await waitFor(() => { // wait for loading
    getByText(/for testuserid/); // displaying the settings for self (this.store.state.user)
    getAllByText('General'); // displays general section and nav
  });

  // can save settings ----------------------------------------------------- //
  await fireEvent.click(getByRole('checkbox'));
  await waitFor(() => { // calls save user settings no user id
    expect(UserService.saveSettings).toHaveBeenCalledWith(expect.objectContaining({
      ...userSettings,
      ms: true
    }), undefined);
  });
  getByText('saveSettings YAY!'); // displays success

  // can change tabs ------------------------------------------------------- //
  await fireEvent.click(getByText('Periodic Queries'));
  getAllByText('Periodic Queries');

  // CUSTOM SESSIONS COLUMN CONFIGURATIONS ////////////////////////////////////
  // display custom session's table column configurations ------------------ //
  await fireEvent.click(getByText('Column Configs'));
  getAllByText('Column Configs');
  getByText('dupe default');

  // can delete custom column configuration -------------------------------- //
  await fireEvent.click(getByTitle('Delete this custom column configuration'));
  expect(UserService.deleteColumnConfig).toHaveBeenCalledWith('dupe default', undefined);
  expect(queryByText('dupe default')).not.toBeInTheDocument(); // removes config
  getByText('deleteColumnConfig YAY!'); // displays success

  // CUSTOM SPIVIEW FIELDS CONFIGURATIONS /////////////////////////////////////
  // display custom spiview fields configurations -------------------------- //
  await fireEvent.click(getByText('SPI View Configs'));
  getAllByText('SPI View Configs');
  getByText('test spiview field config');

  // can delete custom column configuration -------------------------------- //
  await fireEvent.click(getByTitle('Delete this custom spiview field configuration'));
  expect(UserService.deleteSpiviewFieldConfig).toHaveBeenCalledWith('test spiview field config', undefined);
  expect(queryByText('test spiview field config')).not.toBeInTheDocument(); // removes config
  getByText('deleteSpiviewFieldConfig YAY!'); // displays success
});

test('settings - admin editing another', async () => {
  $route.query.userId = 'testuserid2';

  const {
    getByText, getByRole
  } = render(Settings, {
    store,
    mocks: { $route, $router }
  });

  await waitFor(async () => { // wait for loading
    getByText(/for testuserid2/); // displaying the settings for other user ($route.query.userId)
    await fireEvent.click(getByRole('checkbox'));
  });

  await waitFor(() => { // calls save user settings with other user's id
    expect(UserService.saveSettings).toHaveBeenCalledWith(expect.objectContaining({
      ...userSettings,
      ms: true
    }), $route.query.userId);
  });
});

test('settings - non-adming editing another', async () => {
  const nonAdminUserWithSettings = {
    ...users[1], // non-admin user
    settings: { ...userSettings }
  };
  UserService.getCurrent = jest.fn().mockResolvedValue(nonAdminUserWithSettings);
  store.state.user = nonAdminUserWithSettings;
  $route.query.userId = 'testuserid1';

  const {
    queryByText, getByText
  } = render(Settings, {
    store,
    mocks: { $route, $router }
  });

  await waitFor(() => { // wait for loading
    // userId removed since a non-admin user can't edit someone else's settings
    expect($router.push).toHaveBeenCalledWith({
      hash: undefined,
      query: {
        expression: '',
        userId: undefined
      }
    });
  });

  // doesn't display the settings for other user because this user is not admin
  expect(queryByText(/for testuserid1/)).not.toBeInTheDocument();
  // can still see their own settings though
  getByText(/for testuserid2/);
});
