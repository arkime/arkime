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
  userSettings, userWithSettings, views, periodicQueries, fields, notifiers,
  notifierTypes, shortcuts, users, fieldsMap
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
    views,
    user: userWithSettings,
    fieldsArr: fields,
    fieldsMap
  },
  mutations: {
    setViews: jest.fn()
  }
};

const $route = {
  query: {
    expression: '',
    process: undefined
  }
};

const $router = {
  push: jest.fn(),
  replace: jest.fn()
};

const newView = { name: 'newview', expression: 'protocols == tls' };
const newPeriodicQuery = {
  ...periodicQueries[0],
  key: 'newuniquekey329084',
  name: 'test query name 2'
};

// setting services
SettingsService.getNotifierTypes = jest.fn().mockResolvedValue(notifierTypes);
SettingsService.getNotifiers = jest.fn().mockResolvedValue(notifiers);
SettingsService.createNotifier = jest.fn().mockResolvedValue({
  text: 'createNotifier YAY!',
  notifier: notifiers[0]
});
SettingsService.updateNotifier = jest.fn().mockResolvedValue({
  text: 'updateNotifier YAY!',
  notifier: {
    ...notifiers[0],
    updated: 1629133294
  }
});
SettingsService.deleteNotifier = jest.fn().mockResolvedValue({ text: 'deleteNotifier YAY!' });
SettingsService.testNotifier = jest.fn().mockResolvedValue({ text: 'testNotifier YAY!' });
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
// view services
UserService.getViews = jest.fn().mockResolvedValue(views);
UserService.updateView = jest.fn().mockResolvedValue({ text: 'updateView YAY!' });
UserService.toggleShareView = jest.fn().mockResolvedValue({ text: 'toggleShareView YAY!' });
UserService.deleteView = jest.fn().mockResolvedValue({ text: 'deleteView YAY!' });
UserService.createView = jest.fn().mockResolvedValue({
  view: newView,
  viewName: 'newview',
  text: 'createView YAY!'
});
// periodic query services
UserService.createCronQuery = jest.fn().mockResolvedValue({
  text: 'createCronQuery YAY!',
  query: newPeriodicQuery
});
UserService.deleteCronQuery = jest.fn().mockResolvedValue({ text: 'deleteCronQuery YAY!' });
UserService.getCronQueries = jest.fn().mockResolvedValue(periodicQueries);
UserService.updateCronQuery = jest.fn().mockResolvedValue({
  text: 'updateCronQuery YAY!',
  query: newPeriodicQuery
});
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
    getByText, getAllByText, getByRole, getAllByRole, getByPlaceholderText,
    getByTitle, getAllByTitle, getByDisplayValue, queryByText
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
  await fireEvent.click(getByText('Views'));
  getAllByText('Views');

  // VIEWS! ///////////////////////////////////////////////////////////////////
  // displays views -------------------------------------------------------- //
  await waitFor(() => { // displays view with buttons
    getByTitle("Copy this views's expression");
  });

  // create view form validation ------------------------------------------- //
  const createViewBtn = getByTitle('Create new view');
  await fireEvent.click(createViewBtn);
  getByText('No view name specified.');
  const viewNameInput = getByPlaceholderText('Enter a new view name (20 chars or less)');
  const newViewName = 'viewname1';
  await fireEvent.update(viewNameInput, newViewName);
  await fireEvent.click(createViewBtn);
  getByText('No view expression specified.');

  // can create a view ----------------------------------------------------- //
  const shareViewCheckbox = getAllByRole('checkbox')[1];
  await fireEvent.click(shareViewCheckbox);
  const viewExpressionInput = getByPlaceholderText('Enter a new view expression');
  const newViewExpression = 'protocols == tls';
  await fireEvent.update(viewExpressionInput, newViewExpression);
  await fireEvent.click(createViewBtn);
  expect(UserService.createView).toHaveBeenCalledWith({
    shared: true,
    name: newViewName,
    expression: newViewExpression
  }, undefined);

  await waitFor(() => { // create view to return
    getByText('createView YAY!'); // displays success
  });
  expect(viewNameInput.value).toBe(''); // clears form
  expect(viewExpressionInput.value).toBe('');
  expect(shareViewCheckbox).not.toBeChecked();

  await waitFor(() => { // displays new view
    expect(getAllByTitle("Copy this views's expression").length).toBe(2);
  });

  // can share a view ------------------------------------------------------ //
  await fireEvent.click(getAllByRole('checkbox')[0]);
  const view = views[Object.keys(views)[0]];
  expect(UserService.toggleShareView).toHaveBeenCalledWith(view, view.user);

  // can update a view ----------------------------------------------------- //
  await fireEvent.update(getByDisplayValue(view.name), 'updated view name');
  await waitFor(() => {
    fireEvent.click(getByTitle('Save changes to this view'));
  });
  expect(UserService.updateView).toHaveBeenCalledWith({
    ...view,
    name: 'updated view name'
  }, undefined);

  // can delete a view ----------------------------------------------------- //
  await fireEvent.click(getByTitle('Delete this view'));
  expect(UserService.deleteView).toHaveBeenCalledWith(newView, undefined);

  // PERIODIC QUERIES! ////////////////////////////////////////////////////////
  // display periodic queries ---------------------------------------------- //
  await fireEvent.click(getByText('Periodic Queries'));
  getAllByText('Periodic Queries');

  // create periodic query form validation --------------------------------- //
  const createQueryBtn = getByTitle('Create new periodic query');
  await fireEvent.click(createQueryBtn);
  getByText('No query name specified.');
  const queryNameInput = getByPlaceholderText('Periodic query name');
  const newQueryName = 'queryname1';
  await fireEvent.update(queryNameInput, newQueryName);
  await fireEvent.click(createQueryBtn);
  getByText('No query expression specified.');
  const queryExpressionInput = getByPlaceholderText('Periodic query expression');
  const newQueryExpression = 'protocols == tls';
  await fireEvent.update(queryExpressionInput, newQueryExpression);
  await fireEvent.click(createQueryBtn);
  getByText('No query tags specified.');

  // can create a periodic query ------------------------------------------- //
  const queryTagsInput = getByPlaceholderText('Comma separated list of tags');
  const newQueryTags = 'tag1,tag2';
  await fireEvent.update(queryTagsInput, newQueryTags);
  await fireEvent.click(createQueryBtn);
  expect(UserService.createCronQuery).toHaveBeenCalledWith({
    enabled: true,
    name: newQueryName,
    query: newQueryExpression,
    action: 'tag',
    tags: newQueryTags,
    since: '0',
    description: ''
  }, undefined);

  await waitFor(() => { // create query to return
    getByText('createCronQuery YAY!'); // displays success
  });
  expect(queryNameInput.value).toBe(''); // clears form
  expect(queryTagsInput.value).toBe('');
  expect(queryExpressionInput.value).toBe('');

  // displays new periodic query
  const newQueryNameInput = getByDisplayValue('test query name 2');

  // can update a periodic query ------------------------------------------- //
  await fireEvent.update(newQueryNameInput, 'test update query name');
  const saveQueryBtn = getByTitle('Save changes to this query');
  await fireEvent.click(saveQueryBtn);
  expect(UserService.updateCronQuery).toHaveBeenCalledWith({
    ...newPeriodicQuery,
    name: 'test update query name'
  }, undefined);

  // can delete periodic query --------------------------------------------- //
  const deleteQueryBtn = getAllByTitle('Delete this periodic query')[1];
  await fireEvent.click(deleteQueryBtn);
  expect(UserService.deleteCronQuery).toHaveBeenCalledWith(newPeriodicQuery.key, undefined);

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
