'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Notifiers from '../Notifiers.vue';
import HasRole from '../HasRole.vue';
import '../vueFilters';
const {
  userWithSettings, roles, notifiers, notifierTypes
} = require('./consts');

Vue.use(BootstrapVue);

Vue.directive('has-role', HasRole);

const store = {
  state: {
    roles,
    notifiers: [],
    user: userWithSettings
  },
  mutations: {
    setNotifiers: jest.fn()
  }
};

test('notifiers - parliament', async () => {
  fetch.mockResponseOnce(JSON.stringify(notifierTypes)); // mock call to api/notifierTypes
  fetch.mockResponseOnce(JSON.stringify([])); // mock call to api/notifiers

  const {
    getByText, getByTitle, getByDisplayValue, getByPlaceholderText, queryByDisplayValue, emitted, updateProps
  } = render(Notifiers, {
    store,
    props: {
      helpText: 'display me!',
      parentApp: 'parliament'
    }
  });

  // displays help text ---------------------------------------------------- //
  getByText('display me!');

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
  // mock api/notifier call
  fetch.mockResponseOnce(JSON.stringify({ success: true, notifier: notifiers[0], text: 'YAY!' }));
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

  const nameInput = getByDisplayValue('Specific Slack'); // displays the new notifier
  await waitFor(() => { // create notifier returns and tells parent to display message
    getByDisplayValue('https://slack.webhook.url');
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][0][0]).toStrictEqual({ msg: 'YAY!' });
  });

  // can update notifier --------------------------------------------------- //
  const updatedNotifier = { ...notifiers[0] };
  updatedNotifier.name = 'Slack Update';
  // mock api/notifier/:id call
  fetch.mockResponseOnce(JSON.stringify({ success: true, notifier: updatedNotifier, text: 'YAY2!' }));
  await fireEvent.update(nameInput, 'Slack Update');
  const saveBtn = getByText('Save');
  await fireEvent.click(saveBtn);
  await waitFor(() => { // update notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][1][0]).toStrictEqual({ msg: 'YAY2!' });
  });

  // can test notifier ----------------------------------------------------- //
  // mock api/notifier/:id/test
  fetch.mockResponseOnce(JSON.stringify({ success: true, text: 'YAY3!' }));
  await fireEvent.click(getByText('Test'));
  await waitFor(() => { // test notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][2][0]).toStrictEqual({ msg: 'YAY3!' });
  });

  // parliament has parliament only sections ------------------------------ //
  const notifyArea = getByText('Notify on');
  const notifyToggle = getByTitle('Turn this notifier on');

  // help text get's updated ----------------------------------------------- //
  await updateProps({
    helpText: 'HELLO!',
    parentApp: 'arkime'
  });

  getByText('HELLO!');

  // viewer doesn't parliament only sections ------------------------------- //
  expect(notifyArea).not.toBeInTheDocument();
  expect(notifyToggle).not.toBeInTheDocument();

  // can delete notifier --------------------------------------------------- //
  // mock api/notifier/:id delete
  fetch.mockResponseOnce(JSON.stringify({ success: true, text: 'YAY4!' }));
  await fireEvent.click(getByText('Delete'));
  await waitFor(() => { // delete notifier returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][3][0]).toStrictEqual({ msg: 'YAY4!' });
  });
  expect(queryByDisplayValue('Slack')).not.toBeInTheDocument(); // notifier removed
});


