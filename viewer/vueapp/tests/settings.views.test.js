
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
    emitted, getByText, getByTitle, getAllByTitle, queryByText, getByPlaceholderText
  } = render(Views, {
    store,
    mocks: { $route }
  });

  // displays views -------------------------------------------------------- //
  await waitFor(() => { // displays views with buttons
    getAllByTitle("Copy this views's expression");
  });

  // create view form validation ------------------------------------------- //
  const openCreateFormBtn = getByText('New View');
  await fireEvent.click(openCreateFormBtn);
  getByText('Create New View'); // displays form
  const createViewBtn = getByText('Create');
  await fireEvent.click(createViewBtn);
  getByText('View name required');
  const viewNameInput = getByPlaceholderText('View name (20 chars or less)');
  const newViewName = 'viewname1';
  await fireEvent.update(viewNameInput, newViewName);
  await fireEvent.click(createViewBtn);
  getByText('View expression required');

  // can create a view ----------------------------------------------------- //
  const viewExpressionInput = getByPlaceholderText('View expression');
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
  const updateBtn = getAllByTitle('Update this view')[0];
  await fireEvent.click(updateBtn);
  getByText('Edit View'); // displays edit form
  // and fills out view values
  expect(viewNameInput.value).toBe(views[0].name);
  await fireEvent.update(viewNameInput, 'updated view name');
  const saveBtn = getByText('Save');
  await fireEvent.click(saveBtn);
  delete updatedView.user;
  expect(SettingsService.updateView).toHaveBeenCalledWith({ ...updatedView }, undefined);
  getByText('updated view name'); // updates the view in the table

  // can delete a view ----------------------------------------------------- //
  await fireEvent.click(getAllByTitle('Delete this view')[0]);
  expect(SettingsService.deleteView).toHaveBeenCalledWith('1', undefined);
  expect(queryByText('updated view name')).not.toBeInTheDocument(); // view removed

  // can click see all button ---------------------------------------------- //
  const seeAllBtn = getByTitle('See all the views that exist for all users (you can because you are an ADMIN!)');
  await fireEvent.click(seeAllBtn);
  expect(SettingsService.getViews).toHaveBeenCalledWith({
    start: 0,
    all: true,
    length: 50,
    desc: false,
    sort: 'name'
  });
  await waitFor(() => { // updates the title of the button
    getByTitle('Just show the views created by you and shared with you');
  });
});
