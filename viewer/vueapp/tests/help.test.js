'use strict';

import Vue from 'vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import { render, fireEvent } from '@testing-library/vue';
import Help from '../src/components/help/Help.vue';
import FieldService from '../src/components/search/FieldService';
import HasPermission from '../src/components/utils/HasPermission.vue';
import HasRole from '../../../common/vueapp/HasRole.vue';
const { fields, userWithSettings } = require('./consts');

global.$ = global.jQuery = $;

Vue.directive('has-permission', HasPermission);
Vue.directive('has-role', HasRole);

jest.mock('../src/components/search/FieldService');

const store = {
  user: userWithSettings
};

test('help page field list', async () => {
  FieldService.get = jest.fn().mockResolvedValue(fields);

  const $route = { path: 'http://localhost:8123/arkime/help#fields' };

  const { getByText, queryByText } = render(Help, {
    store,
    mocks: { $route }
  });

  getByText('Database Field');

  await fireEvent.click(getByText('Hide Database Fields'));

  getByText('Display Database Fields');
  expect(queryByText('Database Field')).toBeNull();
});
