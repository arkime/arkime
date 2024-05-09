'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, waitFor, fireEvent } from '@testing-library/vue';
import Settings from '../src/components/settings/Settings.vue';
import SettingsService from '../src/components/settings/SettingsService';
import UserService from '../src/components/users/UserService';
import HasPermission from '../src/components/utils/HasPermission.vue';
import HasRole from '../../../common/vueapp/HasRole.vue';
import Utils from '../src/components/utils/utils';
import '../src/filters.js';
import '../../../common/vueapp/vueFilters';
const {
  userSettings, userWithSettings, fields, users, fieldsMap, notifiers
} = require('../../../common/vueapp/tests/consts');

console.info = jest.fn(); // ignore vue dev mode info

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

Vue.directive('has-permission', HasPermission);
Vue.directive('has-role', HasRole);

Vue.prototype.$constants = {
  MULTIVIEWER: false
};

jest.mock('../src/components/users/UserService');
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
SettingsService.getNotifiers = jest.fn().mockResolvedValue(notifiers);
// user services
UserService.getCurrent = jest.fn().mockResolvedValue(userWithSettings);
UserService.getSettings = jest.fn().mockResolvedValue(userSettings);
UserService.saveSettings = jest.fn().mockResolvedValue({ text: 'saveSettings YAY!' });
UserService.resetSettings = jest.fn().mockResolvedValue({ text: 'resetSettings YAY!' });
UserService.changePassword = jest.fn().mockResolvedValue({ text: 'changePassword YAY!' });
// field config services
UserService.deleteLayout = jest.fn().mockResolvedValue({ text: 'deleteLayout YAY!' });
UserService.getState = jest.fn().mockResolvedValue({
  data: {
    visibleHeaders: ['firstPacket', 'lastPacket']
  }
});

test('settings - self', async () => {
  const {
    getByText, getAllByText, getByRole
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
  await fireEvent.click(getByText('Column Layout'));
  getAllByText('Column Layout');
});

test('settings - session column layout', async () => {
  UserService.getLayout = jest.fn().mockResolvedValue([{ ...Utils.getDefaultTableState(), name: 'dupe default' }]);

  const {
    getByText, getAllByText, getByTitle, queryByText
  } = render(Settings, {
    store,
    mocks: { $route, $router }
  });

  // display custom session's table column configurations ------------------ //
  await waitFor(async () => { // wait for loading
    await fireEvent.click(getByText('Column Layout'));
    getAllByText('Column Layout');
    getByText('dupe default');
  });

  // can delete custom column configuration -------------------------------- //
  await fireEvent.click(getByTitle('Delete this custom column layout'));
  expect(UserService.deleteLayout).toHaveBeenCalledWith('sessionstable', 'dupe default', undefined);
  expect(queryByText('dupe default')).not.toBeInTheDocument(); // removes config
  getByText('deleteLayout YAY!'); // displays success
});

test('settings - session info field layout', async () => {
  UserService.getLayout = jest.fn().mockResolvedValue([{ fields: 'destination.ip:100,source.ip:100', name: 'test info field config' }]);

  const {
    getByText, getAllByText, getByTitle, queryByText
  } = render(Settings, {
    store,
    mocks: { $route, $router }
  });

  // display custom session's table column configurations ------------------ //
  await waitFor(async () => { // wait for loading
    await fireEvent.click(getByText('Info Field Layout'));
    getAllByText('Info Field Layout');
    getByText('test info field config');
  });

  // can delete custom column configuration -------------------------------- //
  await fireEvent.click(getByTitle('Delete this custom info field column layout'));
  expect(UserService.deleteLayout).toHaveBeenCalledWith('sessionsinfofields', 'test info field config', undefined);
  expect(queryByText('test info field config')).not.toBeInTheDocument(); // removes config
  getByText('deleteLayout YAY!'); // displays success
});

test('settings - spiview layout', async () => {
  UserService.getLayout = jest.fn().mockResolvedValue([{ fields: 'destination.ip:100,source.ip:100', name: 'test spiview field config' }]);

  const {
    getByText, getAllByText, getByTitle, queryByText
  } = render(Settings, {
    store,
    mocks: { $route, $router }
  });

  // display custom spiview fields configurations -------------------------- //
  await waitFor(async () => { // wait for loading
    await fireEvent.click(getByText('SPI View Layout'));
    getAllByText('SPI View Layout');
    getByText('test spiview field config');
  });

  // can delete custom column configuration -------------------------------- //
  await fireEvent.click(getByTitle('Delete this custom spiview field layout'));
  expect(UserService.deleteLayout).toHaveBeenCalledWith('spiview', 'test spiview field config', undefined);
  expect(queryByText('test spiview field config')).not.toBeInTheDocument(); // removes config
  getByText('deleteLayout YAY!'); // displays success
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
