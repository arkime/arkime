'use strict';

import '@testing-library/jest-dom';
import { render } from '@testing-library/vue';
import HasRole from '../HasRole.vue';
import HasRoleComponent from './components/hasrole.test.vue';

test('has role displays element', async () => {
  const { getByTestId } = render(HasRoleComponent, {}, vue =>
    vue.directive('HasRole', HasRole)
  );

  expect(getByTestId('role-test-1')).not.toHaveClass('d-none');
});

test('has role hides element', async () => {
  const { getByTestId } = render(HasRoleComponent, {}, vue =>
    vue.directive('HasRole', HasRole)
  );

  expect(getByTestId('role-test-2')).toHaveClass('d-none');
});

test('has role logic works', async () => {
  const { getByTestId } = render(HasRoleComponent, {}, vue =>
    vue.directive('HasRole', HasRole)
  );

  expect(getByTestId('role-test-3')).not.toHaveClass('d-none');
});
