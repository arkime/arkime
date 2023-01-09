'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, waitFor, fireEvent } from '@testing-library/vue';
import Users from '../Users.vue';
import UserService from '../UserService';
import HasRole from '../HasRole.vue';
import '../vueFilters';
const { users, userWithSettings, roles } = require('./consts');

console.info = jest.fn(); // don't display console.info messages

Vue.use(BootstrapVue);

Vue.directive('has-role', HasRole);

jest.mock('../UserService');

const store = {
  state: {
    user: userWithSettings
  },
  mutations: {
    setUser (state, value) {}
  }
};

const $route = { query: { length: 10 } };

const props = {
  roles,
  parentApp: 'Arkime',
  currentUser: userWithSettings
};

test('users page no users', async () => {
  UserService.searchUsers = jest.fn().mockResolvedValue({ data: [] });
  UserService.searchUsersMin = jest.fn().mockResolvedValue({ data: [] });

  const {
    getByText, getAllByText
  } = render(Users, {
    store,
    props,
    mocks: { $route }
  });

  await waitFor(() => { // displays table but no users
    getAllByText('ID');
    getByText('No users or roles');
  });
});

test('users page user crud', async () => {
  UserService.searchUsers = jest.fn().mockResolvedValue({
    data: [users[0]],
    recordsTotal: 1,
    recordsFiltered: 1
  });
  UserService.searchUsersMin = jest.fn().mockResolvedValue({ data: [users[0]].map(u => ({ userId: u.userId, userName: u.userName })) });
  UserService.createUser = jest.fn().mockResolvedValue({
    text: 'Successfully created the user!'
  });
  UserService.updateUser = jest.fn().mockResolvedValue({
    text: 'Successfully updated the user!'
  });
  UserService.deleteUser = jest.fn().mockResolvedValue({
    text: 'Successfully deleted the user!'
  });

  const {
    getByText, getByTitle, queryByText, getByPlaceholderText,
    getAllByTestId, getAllByTitle
  } = render(Users, {
    store,
    props,
    mocks: { $route }
  });

  // user exists in the table results -------------------------------------- //
  await waitFor(() => { // loads table with user
    getByText('testuserid');
  });

  // can toggle open user detail
  const expandBtn = getByTitle('toggle');
  await fireEvent.click(expandBtn); // click the expand button

  // checkboxes are correctly (un)checked
  const user = users[0];
  const checkboxes = getAllByTestId('checkbox');
  const checkboxLabels = [
    'enabled', 'webEnabled', 'headerAuthEnabled',
    'emailSearch', 'removeEnabled', 'packetSearch',
    'hideStats', 'hideFiles', 'hidePcap', 'disablePcapDownload'
  ];
  const negativeCheckboxes = {
    emailSearch: true,
    packetSearch: true,
    removeEnabled: true
  };

  for (let i = 0; i < checkboxLabels.length; i++) {
    let shouldBeChecked = user[checkboxLabels[i]];
    if (!shouldBeChecked && negativeCheckboxes[checkboxLabels[i]]) {
      shouldBeChecked = true;
    } else if (shouldBeChecked && negativeCheckboxes[checkboxLabels[i]]) {
      shouldBeChecked = false;
    }
    if (shouldBeChecked) {
      expect(checkboxes[i]).toBeChecked();
    } else {
      expect(checkboxes[i]).not.toBeChecked();
    }
  }

  // update the user ------------------------------------------------------- //
  await fireEvent.click(checkboxes[1]); // edit the user

  // shows the save button for the user
  const saveBtn = getByTitle('Save the updated settings for testuserid');
  await fireEvent.click(saveBtn); // click the save button

  expect(UserService.updateUser).toHaveBeenCalled(); // update user was called

  getByText('Successfully updated the user!'); // displays update user success

  // delete the user  ------------------------------------------------------ //
  const deleteBtn = getByTitle('Delete testuserid');
  await fireEvent.click(deleteBtn); // click the delete btn

  // shows the confirm workflow
  const confirmDeleteBtn = getByTitle('Are you sure?');
  await fireEvent.click(confirmDeleteBtn); // click confirm delete

  expect(UserService.deleteUser).toHaveBeenCalled(); // delete user was called

  expect(queryByText('testuserid')).not.toBeInTheDocument(); // user removed

  getByText('Successfully deleted the user!'); // displays delete user success

  // create a new user ----------------------------------------------------- //
  UserService.searchUsers = jest.fn().mockResolvedValue({
    data: [users[1]],
    recordsTotal: 1,
    recordsFiltered: 1
  });

  const openCreateUserBtn = getAllByTitle('Create a new user')[0];
  await fireEvent.click(openCreateUserBtn);

  getByText('Create a New User'); // shows create user form

  const createBtn = getByTitle('Create New User');
  await fireEvent.click(createBtn);

  getByText('ID can not be empty'); // displays create user error

  const userIdInput = getByPlaceholderText('Unique ID');
  await fireEvent.update(userIdInput, 'testuserid2');

  const userNameInput = getByPlaceholderText('Readable name');
  await fireEvent.update(userNameInput, 'testuser2');

  const userPasswordInput = getByPlaceholderText('New password');
  await fireEvent.update(userPasswordInput, 'testuser2pass');

  await fireEvent.click(createBtn); // click create button

  expect(UserService.createUser).toHaveBeenCalled(); // create user was called

  getByText('Successfully created the user!'); // displays create user success

  await waitFor(() => {
    getByText('testuserid2'); // user was added
  });

  // create a new role ----------------------------------------------------- //
  const openCreateRoleBtn = getAllByTitle('Create a new role')[0];
  await fireEvent.click(openCreateRoleBtn);

  getByText('Create a New Role'); // shows create user form

  // can cancel out of form
  const cancelBtn = getByTitle('Cancel');
  await fireEvent.click(cancelBtn);

  // search users ---------------------------------------------------------- //
  const searchInput = getByPlaceholderText('Begin typing to search for users by name, id, or role');

  await fireEvent.update(searchInput, 'testuserid'); // update search input

  await waitFor(() => { // search issued (called 2x because of create above
    expect(UserService.searchUsers).toHaveBeenCalledTimes(2); // and search input)
  });
});
