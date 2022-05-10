'use strict';

import '@testing-library/jest-dom';
import { render } from '@testing-library/vue';
import HasRole from '../HasRole.vue';
import UserService from '../UserService';
import HasRoleComponent from './components/hasrole.test.vue';

jest.mock('../UserService');

test('has role displays element', async () => {
  UserService.hasRole = jest.fn(() => true);

  const { getByTestId } = render(HasRoleComponent, {}, vue =>
    vue.directive('HasRole', HasRole)
  );

  expect(getByTestId('role-test')).not.toHaveClass('d-none');
});

test('has role hides element', async () => {
  UserService.hasRole = jest.fn(() => false);

  const { getByTestId } = render(HasRoleComponent, {}, vue =>
    vue.directive('HasRole', HasRole)
  );

  expect(getByTestId('role-test')).toHaveClass('d-none');
});
