'use strict';

import Vue from 'vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, waitFor } from '@testing-library/vue';
import Users from '../src/components/users/Users.vue';
import UserService from '../src/components/users/UserService';
import HasRole from '../../../common/vueapp/HasRole.vue';
import '../src/filters.js';
import '../../../common/vueapp/vueFilters';
const { userWithSettings } = require('./consts');

global.$ = global.jQuery = $;

console.info = jest.fn(); // don't display console.info messages

Vue.use(BootstrapVue);

Vue.prototype.$constants = {
  MOLOCH_TMP_ROLES_SUPPORT: false
};

Vue.directive('has-role', HasRole);

jest.mock('../src/components/users/UserService');

const store = {
  state: {
    user: userWithSettings
  },
  mutations: {
    setUser (state, value) {}
  }
};

const $route = { query: { length: 10 } };

test('users page no users', async () => {
  UserService.searchUsers = jest.fn().mockResolvedValue({ data: { data: [] } });

  const {
    getByText, getAllByText
  } = render(Users, {
    store,
    mocks: { $route }
  });

  await waitFor(() => { // displays table but no users
    getAllByText('User ID');
    getByText('No users');
  });
});

// TODO - fix these tests b-table always shows no users
// test('users page user crud', async () => {
//   UserService.searchUsers = jest.fn().mockResolvedValue({
//     data: {
//       data: [users[0]],
//       recordsTotal: 1,
//       recordsFiltered: 1
//     }
//   });
//   UserService.createUser = jest.fn().mockResolvedValue({
//     data: { text: 'Successfully created the user!' }
//   });
//   UserService.updateUser = jest.fn().mockResolvedValue({
//     data: { text: 'Successfully updated the user!' }
//   });
//   UserService.deleteUser = jest.fn().mockResolvedValue({
//     data: { text: 'Successfully deleted the user!' }
//   });
//   UserService.getRoles = jest.fn().mockResolvedValue({
//     data: []
//   });
//
//   const {
//     getByText, getAllByRole, getByTitle, queryByText, getByLabelText,
//     getByPlaceholderText
//   } = render(Users, {
//     store,
//     mocks: { $route }
//   });
//
//   // user exists in the table results -------------------------------------- //
//   await waitFor(() => { // loads table with user
//     getByText('testuserid');
//   });
//
//   // checkboxes are correctly (un)checked
//   const user = users[0];
//   const checkboxes = getAllByRole('checkbox');
//   const checkboxLabels = [
//     'enabled', 'createEnabled', 'webEnabled', 'headerAuthEnabled',
//     'emailSearch', 'removeEnabled', 'packetSearch'
//   ];
//   for (let i = 0; i < checkboxLabels.length; i++) {
//     if (user[checkboxLabels[i]]) {
//       expect(checkboxes[i]).toBeChecked();
//     } else {
//       expect(checkboxes[i]).not.toBeChecked();
//     }
//   }
//
//   // update the user ------------------------------------------------------- //
//   await fireEvent.click(checkboxes[1]); // edit the user
//
//   // shows the save button for the user
//   const saveBtn = getByTitle('Save the updated settings for testuserid');
//   await fireEvent.click(saveBtn); // click the save button
//
//   expect(UserService.updateUser).toHaveBeenCalled(); // update user was called
//
//   getByText('Successfully updated the user!'); // displays update user success
//
//   // delete the user  ------------------------------------------------------ //
//   const deleteBtn = getByTitle('Delete testuserid');
//   await fireEvent.click(deleteBtn); // click the delete btn
//
//   expect(UserService.deleteUser).toHaveBeenCalled(); // delete user was called
//
//   expect(queryByText('testuserid')).not.toBeInTheDocument(); // user removed
//
//   getByText('Successfully deleted the user!'); // displays delete user success
//
//   // create a new user ----------------------------------------------------- //
//   UserService.searchUsers = jest.fn().mockResolvedValue({
//     data: {
//       data: [users[1]],
//       recordsTotal: 1,
//       recordsFiltered: 1
//     }
//   });
//
//   const createBtn = getByTitle('Create new user');
//   await fireEvent.click(createBtn);
//
//   getByText('User ID can not be empty'); // displays create user error
//
//   const userIdInput = getByLabelText(/User ID/i);
//   await fireEvent.update(userIdInput, 'testuserid2');
//
//   const userNameInput = getByLabelText(/User Name/i);
//   await fireEvent.update(userNameInput, 'testuser2');
//
//   const userPasswordInput = getByLabelText(/Password/i);
//   await fireEvent.update(userPasswordInput, 'testuser2pass');
//
//   await fireEvent.click(createBtn); // click create button
//
//   expect(UserService.createUser).toHaveBeenCalled(); // create user was called
//
//   await waitFor(() => {
//     getByText('testuserid2'); // user was added
//   });
//
//   getByText('Successfully created the user!'); // displays create user success
//
//   // search users ---------------------------------------------------------- //
//   const searchInput = getByPlaceholderText('Begin typing to search for users by name');
//
//   await fireEvent.update(searchInput, 'testuserid'); // update search input
//
//   await waitFor(() => { // search issued (called 2x because of create above
//     expect(UserService.searchUsers).toHaveBeenCalledTimes(2); // and search input)
//   });
// });
