'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Notifiers from '../src/components/settings/Notifiers.vue';
import SettingsService from '../src/components/settings/SettingsService';
import HasRole from '../../../common/vueapp/HasRole.vue';
import '../../../common/vueapp/vueFilters';
const {
  userWithSettings, roles, notifiers, notifierTypes
} = require('../../../common/vueapp/tests/consts');

Vue.use(BootstrapVue);

Vue.directive('has-role', HasRole);

jest.mock('../src/components/settings/SettingsService');

const store = {
  state: {
    roles,
    notifiers: [],
    user: userWithSettings
  }
};

SettingsService.deleteNotifier = jest.fn().mockResolvedValue({ text: 'deleteNotifier YAY!' });
SettingsService.testNotifier = jest.fn().mockResolvedValue({ text: 'testNotifier YAY!' });
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

test('notifiers', async () => {
  const {
    getByText, getByDisplayValue, getByPlaceholderText, queryByDisplayValue, emitted
  } = render(Notifiers, { store });

  // create notifier form validation --------------------------------------- //
  let openCreateFormBtn;
  await waitFor(() => { // wait for getNotifierTypes to return
    openCreateFormBtn = getByText('New Slack Notifier');
  });

  await fireEvent.click(openCreateFormBtn);
  getByText('Create New Slack Notifier'); // displays form
  const createBtn = getByText('Create');
  await fireEvent.click(createBtn);
  // required notifier fields
  getByText(`${notifierTypes.slack.fields[0].name} is required`);
  // required name field
  const newNameInput = getByDisplayValue('Slack');
  await fireEvent.update(newNameInput, '');
  await fireEvent.click(createBtn);
  getByText('Your new notifier must have a unique name');

  // can create notifier --------------------------------------------------- //
  await fireEvent.update(newNameInput, 'Specific Slack');
  await fireEvent.update(getByPlaceholderText(notifierTypes.slack.fields[0].description), 'url');
  await fireEvent.click(createBtn);

  const newNotifier = { ...notifiers[0] };
  delete newNotifier.id;
  delete newNotifier.key;
  delete newNotifier.user;
  delete newNotifier.created;
  newNotifier.users = '';
  newNotifier.name = 'Specific Slack';
  newNotifier.fields[0].value = 'url';
  newNotifier.roles = ['arkimeUser', 'parliamentUser'];

  expect(SettingsService.createNotifier).toHaveBeenCalledWith(newNotifier);
  const nameInput = getByDisplayValue('Slack'); // displays the new notifier
  getByDisplayValue('https://slack.webhook.url');
  await waitFor(() => { // create notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][0][0]).toStrictEqual({ msg: 'createNotifier YAY!' });
  });

  // can update notifier --------------------------------------------------- //
  await fireEvent.update(nameInput, 'Slack Update');
  const saveBtn = getByText('Save');
  await fireEvent.click(saveBtn);
  const updatedNotifier = { ...notifiers[0] };
  updatedNotifier.name = 'Slack Update';
  expect(SettingsService.updateNotifier).toHaveBeenCalledWith(notifiers[0].id, updatedNotifier);
  await waitFor(() => { // update notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][1][0]).toStrictEqual({ msg: 'updateNotifier YAY!' });
  });

  // can test notifier ----------------------------------------------------- //
  await fireEvent.click(getByText('Test'));
  expect(SettingsService.testNotifier).toHaveBeenCalledWith(notifiers[0].id);
  await waitFor(() => { // test notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][2][0]).toStrictEqual({ msg: 'testNotifier YAY!' });
  });

  // can delete notifier --------------------------------------------------- //
  await fireEvent.click(getByText('Delete'));
  expect(SettingsService.deleteNotifier).toHaveBeenCalledWith(notifiers[0].id);
  await waitFor(() => { // delete notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][3][0]).toStrictEqual({ msg: 'deleteNotifier YAY!' });
  });
  expect(queryByDisplayValue('Slack')).not.toBeInTheDocument(); // notifier removed
});
