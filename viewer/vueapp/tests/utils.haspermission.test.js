// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render } from '@testing-library/vue';
import '@testing-library/jest-dom'
import HasPermission from '../src/components/utils/HasPermission.vue';
import HasPermissionComponent from './components/haspermission.test.vue';
import UserService from '../src/components/users/UserService';

global.$ = global.jQuery = $;

test('has permission displays element', async () => {
  jest.mock('../src/components/users/UserService');

  UserService.hasPermission = jest.fn(() => true);

  const { getByText } = render(HasPermissionComponent, {}, vue =>
    vue.directive('HasPermission', HasPermission)
  );

  expect(getByText('has permission test')).toBeVisible();
});

test('has permission hides element', async () => {
  jest.mock('../src/components/users/UserService');

  UserService.hasPermission = jest.fn(() => false);

  const { getByText } = render(HasPermissionComponent, {}, vue =>
    vue.directive('HasPermission', HasPermission)
  );

  expect(getByText('has permission test')).not.toBeVisible();
});
