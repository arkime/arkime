import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Users from '../src/components/users/Users.vue';
import UserService from '../src/components/users/UserService';
import FocusInput from '../src/components/utils/FocusInput.vue';
import '../src/filters.js';
const { users } = require('./consts');

console.info = jest.fn(); // don't display console.info messages

Vue.use(BootstrapVue);

Vue.directive('focus-input', FocusInput);

jest.mock('../src/components/users/UserService');

const store = {
  state: {
    user: { settings: { timezone: 'gmt', ms: false, userId: 'admin' } }
  }
};

const $route = { query: { length: 10 } };

test('users page no users', async () => {
  UserService.getUsers = jest.fn().mockResolvedValue({ data: { data: [] } });

  const {
    getByText
  } = render(Users, {
    store,
    mocks: { $route }
  });

  await waitFor(() => { // displays no results indication
    getByText('No users match your search');
  });
});

test('users page user crud', async () => {
  UserService.getUsers = jest.fn().mockResolvedValue({
    data: {
      data: [users[0]],
      recordsTotal: 1,
      recordsFiltered: 1
    }
  });
  UserService.createUser = jest.fn().mockResolvedValue({
    data: { text: 'Successfully created the user!' }
  });
  UserService.updateUser = jest.fn().mockResolvedValue({
    data: { text: 'Successfully updated the user!' }
  });
  UserService.deleteUser = jest.fn().mockResolvedValue({
    data: { text: 'Successfully deleted the user!' }
  });

  const {
    getByText, getAllByRole, getByTitle, queryByText,
    getByLabelText, getByPlaceholderText
  } = render(Users, {
    store,
    mocks: { $route }
  });

  // user exists in the table results -------------------------------------- //
  await waitFor(() => { // loads table with user
    getByText('testuserid');
  });

  // checkboxes are correctly (un)checked
  const user = users[0];
  const checkboxes = getAllByRole('checkbox');
  const checkboxLabels = [
    'enabled', 'createEnabled', 'webEnabled', 'headerAuthEnabled',
    'emailSearch', 'removeEnabled', 'packetSearch'
  ];
  for (let i = 0; i < checkboxLabels.length; i++) {
    if (user[checkboxLabels[i]]) {
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

  expect(UserService.deleteUser).toHaveBeenCalled(); // delete user was called

  expect(queryByText('testuserid')).not.toBeInTheDocument(); // user removed

  getByText('Successfully deleted the user!'); // displays delete user success

  // create a new user ----------------------------------------------------- //
  UserService.getUsers = jest.fn().mockResolvedValue({
    data: {
      data: [users[1]],
      recordsTotal: 1,
      recordsFiltered: 1
    }
  });

  const createBtn = getByTitle('Create new user');
  await fireEvent.click(createBtn);

  getByText('User ID can not be empty'); // displays create user error

  const userIdInput = getByLabelText(/User ID/i);
  await fireEvent.update(userIdInput, 'testuserid2');

  const userNameInput = getByLabelText(/User Name/i);
  await fireEvent.update(userNameInput, 'testuser2');

  const userPasswordInput = getByLabelText(/Password/i);
  await fireEvent.update(userPasswordInput, 'testuser2pass');

  await fireEvent.click(createBtn); // click create button

  expect(UserService.createUser).toHaveBeenCalled(); // create user was called

  await waitFor(() => {
    getByText('testuserid2'); // user was added
  });

  getByText('Successfully created the user!'); // displays create user success

  // search users ---------------------------------------------------------- //
  const searchInput = getByPlaceholderText('Begin typing to search for users by name');

  await fireEvent.update(searchInput, 'testuserid'); // update search input

  await waitFor(() => { // search issued (called 2x because of create above
    expect(UserService.getUsers).toHaveBeenCalledTimes(2); // and search input)
  });
});
