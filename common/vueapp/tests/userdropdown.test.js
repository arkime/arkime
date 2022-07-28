'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import UserDropdown from '../UserDropdown';
import UserService from '../UserService';

Vue.use(BootstrapVue);

jest.mock('../UserService');

const userInfo = [
  { userId: 'testuser1', userName: 'Test User 1' },
  { userId: 'testuser2', userName: 'Test User 2' },
  { userId: 'testuser3', userName: 'Test User 3' },
  { userId: 'odduser', userName: 'Odd User' }
];

const userInfoWithRoles = [
  { userId: 'testuser1', userName: 'Test User 1', hasRole: true },
  { userId: 'testuser2', userName: 'Test User 2', hasRole: false },
  { userId: 'testuser3', userName: 'Test User 3', hasRole: false },
  { userId: 'odduser', userName: 'Odd User', hasRole: true }
];

const filteredUserInfoWithRoles = [
  { userId: 'testuser1', userName: 'Test User 1', hasRole: true },
  { userId: 'testuser2', userName: 'Test User 2', hasRole: false },
  { userId: 'testuser3', userName: 'Test User 3', hasRole: false }
];

test('users dropdown render, update, and emit', async () => {
  UserService.searchUsersMin = jest.fn().mockResolvedValue({ data: userInfo });

  const {
    getByTestId, getByLabelText, getByText, emitted
  } = render(UserDropdown, {
    props: {
      roleId: 'role:test'
    }
  });
  // displays displayText prop by default as dropdown button text
  const userDropdown = getByTestId('user-dropdown');
  await fireEvent.click(userDropdown); // click to open the dropdown

  let userCheckboxes;
  await waitFor(() => {
    userCheckboxes = userInfo.map(({ userId, userName }) => {
      return getByLabelText(`${userName} (${userId})`); // should show all user options
    });
    // all should start un-checked
    for (const userCheckbox of userCheckboxes) {
      expect(userCheckbox).not.toBeChecked();
    }
  });

  const firstCheckbox = userCheckboxes[0];
  const secondCheckbox = userCheckboxes[1];

  // click first checkbox -- should check
  await fireEvent.click(firstCheckbox);
  expect(firstCheckbox).toBeChecked();

  getByText('testuser1'); // default dropdown display text

  // click second checkbox -- should check
  await fireEvent.click(secondCheckbox);
  expect(secondCheckbox).toBeChecked();

  getByText('testuser1, testuser2'); // default dropdown display text

  // click first checkbox (again) -- should un-check
  await fireEvent.click(firstCheckbox);
  expect(firstCheckbox).not.toBeChecked();

  getByText('testuser2'); // default dropdown display text

  // emits users update event with new role and prop id
  expect(emitted()).toHaveProperty('selected-users-updated');
  expect(emitted()['selected-users-updated'][0][0]).toEqual({
    newSelection: ['testuser1'],
    changedUser: {
      userId: 'testuser1',
      newState: true
    }
  });
  expect(emitted()['selected-users-updated'][0][1]).toEqual('role:test');
  expect(emitted()['selected-users-updated'][1][0]).toEqual({
    newSelection: ['testuser1', 'testuser2'],
    changedUser: {
      userId: 'testuser2',
      newState: true
    }
  });
  expect(emitted()['selected-users-updated'][2][0]).toEqual({
    newSelection: ['testuser2'],
    changedUser: {
      userId: 'testuser1',
      newState: false
    }
  });

  // listing users should have only been done once (to initialize), since no filter was entered
  expect(UserService.searchUsersMin).toHaveBeenCalledTimes(1);
});

test('users dropdown role-prefill and filter', async () => {
  UserService.searchUsersMin = jest.fn().mockImplementation((query) => {
    // return correct set of data depending on the given searchTerm
    const filtered = query.filter === 'testuser';
    return Promise.resolve({ data: filtered ? filteredUserInfoWithRoles : userInfoWithRoles });
  });

  const {
    getByText, getByLabelText, getByPlaceholderText
  } = render(UserDropdown, {
    props: {
      initializeSelectionWithRole: true
    }
  });
  // displays displayText prop by default as dropdown button text
  let userDropdown;
  await waitFor(() => {
    userDropdown = getByText('odduser, testuser1');
  });
  await fireEvent.click(userDropdown); // click to open the dropdown

  await waitFor(() => {
    for (const { userId, userName, hasRole } of userInfoWithRoles) {
      const userCheckbox = getByLabelText(`${userName} (${userId})`); // should show all user options
      // should be checked depending on value of hasRole
      if (hasRole) {
        expect(userCheckbox).toBeChecked();
      } else {
        expect(userCheckbox).not.toBeChecked();
      }
    }
  });

  const searchBar = getByPlaceholderText('Begin typing to search for users by name or id');
  await fireEvent.update(searchBar, 'testuser');

  await waitFor(() => {
    getByText('testuser1'); // display text should not show odduser, because they are filtered out
  });

  // called once for initialization, and once after searchTerm is entered
  expect(UserService.searchUsersMin).toHaveBeenCalledTimes(2);
});
