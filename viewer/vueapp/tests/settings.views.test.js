
import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, waitFor, fireEvent } from '@testing-library/vue';
import SettingsService from '../src/components/settings/SettingsService';
import Views from '../src/components/settings/Views';
import '../../../common/vueapp/vueFilters';
const { roles, views, userWithSettings } = require('../../../common/vueapp/tests/consts');

Vue.use(BootstrapVue);

jest.mock('../src/components/settings/SettingsService');

const store = {
  state: {
    roles,
    views,
    user: userWithSettings
  },
  mutations: {
    setViews: jest.fn()
  }
};

const $route = {
  query: {
    expression: ''
  }
};

const newView = { name: 'newview', expression: 'protocols == tls', users: '', roles: [], id: '4' };
const updatedView = { ...views[0], name: 'updated view name' };

// view services
SettingsService.getViews = jest.fn().mockResolvedValue(views);
SettingsService.deleteView = jest.fn().mockResolvedValue({
  success: true,
  text: 'deleteView YAY!'
});
SettingsService.updateView = jest.fn().mockResolvedValue({
  success: true,
  view: updatedView,
  text: 'updateView YAY!'
});
SettingsService.createView = jest.fn().mockResolvedValue({
  succes: true,
  view: newView,
  text: 'createView YAY!'
});

test('views', async () => {
  const {
    emitted, getByText, getByTitle, getAllByTitle, queryByText, getByDisplayValue, getByPlaceholderText
  } = render(Views, {
    store,
    mocks: { $route }
  });

  // displays views -------------------------------------------------------- //
  await waitFor(() => { // displays views with buttons
    getAllByTitle("Copy this views's expression");
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
  const viewExpressionInput = getByPlaceholderText('Enter a new view expression');
  const newViewExpression = 'protocols == tls';
  await fireEvent.update(viewExpressionInput, newViewExpression);
  await fireEvent.click(createViewBtn);
  expect(SettingsService.createView).toHaveBeenCalledWith({
    roles: [],
    users: '',
    name: newViewName,
    expression: newViewExpression
  }, undefined);

  await waitFor(() => { // create view returns and tells parent to display message
    expect(emitted()).toHaveProperty('display-message');
    expect(emitted()['display-message'][0][0]).toStrictEqual({ msg: 'createView YAY!' });
  });
  expect(viewNameInput.value).toBe(''); // clears form
  expect(viewExpressionInput.value).toBe('');

  await waitFor(() => { // displays new view
    expect(getAllByTitle("Copy this views's expression").length).toBe(4);
  });

  // can update a view ----------------------------------------------------- //
  await fireEvent.update(getByDisplayValue('test view 1'), 'updated view name');
  await waitFor(() => {
    fireEvent.click(getByTitle('Save changes to this view'));
  });
  expect(SettingsService.updateView).toHaveBeenCalledWith({
    ...updatedView,
    changed: true
  }, undefined);

  // can delete a view ----------------------------------------------------- //
  await fireEvent.click(getAllByTitle('Delete this view')[0]);
  expect(SettingsService.deleteView).toHaveBeenCalledWith('2', undefined);
  expect(queryByText('test view 2')).not.toBeInTheDocument(); // view removed
});
