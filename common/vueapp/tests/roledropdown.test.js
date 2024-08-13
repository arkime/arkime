'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import RoleDropdown from '../RoleDropdown';
import { roles } from './consts';
import '../vueFilters';

Vue.use(BootstrapVue);

test('roles dropdown render and update', async () => {
  const {
    getByText, getByTitle, getByLabelText, emitted, getByPlaceholderText, queryByText
  } = render(RoleDropdown, {
    props: {
      roles,
      id: 'test_id',
      displayText: 'display me'
    }
  });

  // displays displayText prop by default as dropdown button text
  const roleDropdown = getByText('display me');
  await fireEvent.click(roleDropdown); // click to open the dropdown

  let roleCheckbox;
  for (const role of roles) {
    roleCheckbox = getByLabelText(role.text); // should show all role options
    expect(roleCheckbox).not.toBeChecked(); // and they should all be unchecked to start
  }

  getByTitle('User defined role'); // should be one labeled as user defined

  // can check a checkbox
  await fireEvent.click(roleCheckbox);
  expect(roleCheckbox).toBeChecked();

  // emits roles update event with new role and prop id
  expect(emitted()).toHaveProperty('selected-roles-updated');
  expect(emitted()['selected-roles-updated'][0][0]).toStrictEqual(['userDefined']);
  expect(emitted()['selected-roles-updated'][0][1]).toBe('test_id');

  // can search roles
  const searchBar = getByPlaceholderText('Search for roles...');
  await fireEvent.update(searchBar, 'arkimeAdmin');

  await waitFor(() => { // should only show arkimeAdmin because others are filtered out
    getByText('arkimeAdmin');
    expect(queryByText('arkimeUser')).not.toBeInTheDocument(); // user removed
  });
});

test('roles dropdown with selected roles', async () => {
  const selectedRoles = ['arkimeUser', 'userDefined'];

  const {
    getByText, getByLabelText
  } = render(RoleDropdown, {
    props: {
      roles,
      selectedRoles
    }
  });

  // displays selected roles dropdown button text
  const roleDropdown = getByText('arkimeUser, userDefined');
  await fireEvent.click(roleDropdown); // click to open the dropdown

  let roleCheckbox;
  for (const role of roles) {
    roleCheckbox = getByLabelText(role.text); // should show all role options
    if (selectedRoles.includes(role.text)) {
      expect(roleCheckbox).toBeChecked(); // selected roles should be checked to start
    } else {
      expect(roleCheckbox).not.toBeChecked(); // all others should be unchecked to start
    }
  }
});
