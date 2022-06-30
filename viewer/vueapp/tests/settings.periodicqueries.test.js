'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import PeriodicQueries from '../src/components/settings/PeriodicQueries.vue';
import SettingsService from '../src/components/settings/SettingsService';
import '../../../common/vueapp/vueFilters';
const {
  userWithSettings, periodicQueries, roles, notifiers
} = require('../../../common/vueapp/tests/consts');

Vue.use(BootstrapVue);
jest.mock('../src/components/settings/SettingsService');

const store = {
  state: {
    roles,
    notifiers,
    remoteClusters: {},
    user: userWithSettings
  }
};

const $route = {
  query: {
    expression: '',
    process: undefined
  }
};

const $router = {
  replace: jest.fn()
};

const newPeriodicQuery = {
  ...periodicQueries[0],
  key: 'newuniquekey329084',
  name: 'test query name 2'
};

SettingsService.createCronQuery = jest.fn().mockResolvedValue({
  text: 'createCronQuery YAY!',
  query: newPeriodicQuery
});
SettingsService.updateCronQuery = jest.fn().mockResolvedValue({
  text: 'updateCronQuery YAY!',
  query: {
    ...newPeriodicQuery,
    name: 'test update query name'
  }
});
SettingsService.deleteCronQuery = jest.fn().mockResolvedValue({ text: 'deleteCronQuery YAY!' });
SettingsService.getCronQueries = jest.fn().mockResolvedValue(periodicQueries);

test('periodic queries', async () => {
  const {
    getByText, queryByText, getByTitle, getAllByTitle, getByDisplayValue, getByPlaceholderText, emitted
  } = render(PeriodicQueries, {
    store,
    mocks: { $route, $router }
  });

  // create periodic query form validation --------------------------------- //
  const openCreateFormBtn = getByText('New Periodic Query');
  await fireEvent.click(openCreateFormBtn);
  getByText('Create New Periodic Query'); // displays form
  const createQueryBtn = getByText('Create');
  await fireEvent.click(createQueryBtn);
  getByText('No query name specified.');
  const queryNameInput = getByPlaceholderText('Periodic query name (20 chars or less)');
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
  expect(SettingsService.createCronQuery).toHaveBeenCalledWith({
    enabled: true,
    name: newQueryName,
    query: newQueryExpression,
    action: 'tag',
    tags: newQueryTags,
    since: '0',
    description: '',
    users: '',
    roles: []
  }, undefined);

  await waitFor(() => { // create query returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][0][0]).toStrictEqual({ msg: 'createCronQuery YAY!' });
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
  expect(SettingsService.updateCronQuery).toHaveBeenCalledWith({
    ...newPeriodicQuery,
    name: 'test update query name'
  }, undefined);
  await waitFor(() => { // update query returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][1][0]).toStrictEqual({ msg: 'updateCronQuery YAY!' });
  });
  getByDisplayValue('test update query name'); // displays updated query

  // can delete periodic query --------------------------------------------- //
  const deleteQueryBtn = getAllByTitle('Delete this periodic query')[1];
  await fireEvent.click(deleteQueryBtn);
  expect(SettingsService.deleteCronQuery).toHaveBeenCalledWith(newPeriodicQuery.key, undefined);
  await waitFor(() => { // delete query returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][2][0]).toStrictEqual({ msg: 'deleteCronQuery YAY!' });
  });
  expect(queryByText('test update query name')).not.toBeInTheDocument(); // query removed
});
