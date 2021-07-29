'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent } from '@testing-library/vue';
import ModifyView from '../src/components/sessions/ModifyView.vue';
import UserService from '../src/components/users/UserService';
import utils from '../src/components/utils/utils';

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

jest.mock('../src/components/users/UserService');

const store = {
  getters: {
    sessionsTableState: jest.fn(() => {
      return utils.getDefaultTableState();
    })
  },
  mutations: {
    addViews: jest.fn(),
    updateViews: jest.fn()
  }
};

test('sessions - create view', async () => {
  const props = {
    done: jest.fn(),
    initialExpression: ''
  };

  UserService.createView = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: {}, name: 'Sessions' };

  const {
    getByTitle, getByPlaceholderText, getByText, getByLabelText, emitted
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
  const newView = {
    name: nameInput.value,
    expression: expressionInput.value,
    sessionsColConfig: utils.getDefaultTableState()
  };
  await fireEvent.click(saveBtn);
  expect(store.mutations.addViews).toHaveBeenCalledWith({}, newView);
  expect(emitted()).toHaveProperty('setView');
  expect(emitted().setView[0][0]).toBe(newView.name);
  expect(UserService.createView).toHaveBeenCalledWith(newView);
});

test('sessions - save columns not available', async () => {
  const props = {
    done: jest.fn(),
    initialExpression: ''
  };

  UserService.createView = jest.fn().mockResolvedValue({ text: 'yay!' });

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

  UserService.updateView = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: { view: 'edit me' }, name: 'Sessions' };

  const {
    getByTitle, getByPlaceholderText, emitted
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
    name: nameInput.value,
    key: props.editView.name,
    expression: expressionInput.value
  };
  await fireEvent.click(saveBtn);
  expect(UserService.updateView).toHaveBeenCalledWith(updatedView, undefined);
  expect(store.mutations.updateViews).toHaveBeenCalledWith({}, updatedView);
  expect(emitted()).toHaveProperty('setView');
  expect(emitted().setView[0][0]).toBe(updatedView.name);
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

  UserService.updateView = jest.fn().mockResolvedValue({ text: 'yay!' });

  const $route = { query: { view: 'different view' }, name: 'Sessions' };

  const {
    getByTitle, getByPlaceholderText, emitted
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
    name: nameInput.value,
    key: props.editView.name,
    expression: expressionInput.value
  };
  await fireEvent.click(saveBtn);
  expect(UserService.updateView).toHaveBeenCalledWith(updatedView, undefined);
  expect(store.mutations.updateViews).toHaveBeenCalledWith({}, updatedView);
  expect(emitted()).not.toHaveProperty('setView');
});
