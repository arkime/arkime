'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import ModifyView from '../src/components/sessions/ModifyView.vue';
import SettingsService from '../src/components/settings/SettingsService';
import utils from '../src/components/utils/utils';
const { roles } = require('../../../common/vueapp/tests/consts');

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/settings/SettingsService');

const store = {
  state: {
    roles
  },
  getters: {
    sessionsTableState: jest.fn(() => {
      return utils.getDefaultTableState();
    })
  },
  mutations: {
    addView: jest.fn(),
    updateViews: jest.fn()
  }
};

SettingsService.getViews = jest.fn();

test('sessions - create view', async () => {
  const props = {
    done: jest.fn(),
    initialExpression: ''
  };

  const newView = {
    roles: [],
    users: '',
    name: 'view name 1',
    expression: 'ip.src == 10.0.0.1',
    sessionsColConfig: utils.getDefaultTableState()
  };

  SettingsService.createView = jest.fn().mockResolvedValue({
    text: 'yay!',
    success: true,
    view: { ...newView, id: '10' }
  });

  const $route = { query: {}, name: 'Sessions' };

  const {
    getByTitle, getByPlaceholderText, getByText, getByLabelText
  } = render(ModifyView, {
    mocks: { $route },
    props,
    store
  });

  // has all fields -------------------------------------------------------- //
  const saveBtn = getByTitle('Create View');
  const nameInput = getByPlaceholderText('Enter a (short) view name');
  const expressionInput = getByPlaceholderText('Enter a query expression');
  const saveColsCheckbox = getByLabelText('Save Columns');

  // requires name input value --------------------------------------------- //
  await fireEvent.click(saveBtn);
  getByText('No view name specified.');
  await fireEvent.update(nameInput, 'view name 1');

  // requires expression input value --------------------------------------- //
  await fireEvent.click(saveBtn);
  getByText('No expression specified.');
  await fireEvent.update(expressionInput, 'ip.src == 10.0.0.1');

  // can save columns with view -------------------------------------------- //
  await fireEvent.click(saveColsCheckbox);
  expect(saveColsCheckbox).toBeChecked();

  // calls create view ----------------------------------------------------- //
  await fireEvent.click(saveBtn);
  expect(SettingsService.createView).toHaveBeenCalledWith(newView, undefined);
  expect(store.mutations.addView).toHaveBeenCalledWith(store.state, { ...newView, id: '10' });
});

test('sessions - save columns not available', async () => {
  const props = {
    done: jest.fn(),
    initialExpression: ''
  };

  SettingsService.createView = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {}, name: 'SpiGraph' };

  const {
    queryByLabelText
  } = render(ModifyView, {
    mocks: { $route },
    props,
    store
  });

  expect(queryByLabelText('Save Columns')).not.toBeInTheDocument();
});

test('sessions - update applied view', async () => {
  const props = {
    done: jest.fn(),
    initialExpression: '',
    editView: {
      name: 'edit me',
      expression: 'ip.dst == 10.0.0.2'
    }
  };

  SettingsService.updateView = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: { view: 'edit me' }, name: 'Sessions' };

  const {
    getByTitle, getByPlaceholderText
  } = render(ModifyView, {
    mocks: { $route },
    props,
    store
  });

  // has all fields -------------------------------------------------------- //
  const saveBtn = getByTitle('Save View');
  const nameInput = getByPlaceholderText('Enter a (short) view name');
  const expressionInput = getByPlaceholderText('Enter a query expression');

  // has name input value -------------------------------------------------- //
  expect(nameInput.value).toBe(props.editView.name);
  await fireEvent.update(nameInput, 'updated name');

  // has expression input value -------------------------------------------- //
  expect(expressionInput.value).toBe(props.editView.expression);
  await fireEvent.update(expressionInput, 'ip.dst == 10.0.0.3');

  // calls create view ----------------------------------------------------- //
  const updatedView = {
    roles: [],
    users: '',
    name: nameInput.value,
    expression: expressionInput.value
  };
  await fireEvent.click(saveBtn);
  expect(SettingsService.updateView).toHaveBeenCalledWith(updatedView, undefined);
  expect(SettingsService.getViews).toHaveBeenCalled();
});

test('sessions - update unapplied view', async () => {
  const props = {
    done: jest.fn(),
    initialExpression: '',
    editView: {
      name: 'edit me',
      expression: 'ip.dst == 10.0.0.2'
    }
  };

  SettingsService.updateView = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: { view: 'different view' }, name: 'Sessions' };

  const {
    getByTitle, getByPlaceholderText
  } = render(ModifyView, {
    mocks: { $route },
    props,
    store
  });

  // has all fields -------------------------------------------------------- //
  const saveBtn = getByTitle('Save View');
  const nameInput = getByPlaceholderText('Enter a (short) view name');
  const expressionInput = getByPlaceholderText('Enter a query expression');

  // has name input value -------------------------------------------------- //
  await fireEvent.update(nameInput, 'updated name');
  await fireEvent.update(expressionInput, 'ip.dst == 10.0.0.3');

  // calls create view ----------------------------------------------------- //
  const updatedView = {
    roles: [],
    users: '',
    name: nameInput.value,
    expression: expressionInput.value
  };
  await fireEvent.click(saveBtn);
  expect(SettingsService.updateView).toHaveBeenCalledWith(updatedView, undefined);
  expect(SettingsService.getViews).toHaveBeenCalled();
});
