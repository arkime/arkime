'use strict';

// eslint-disable-next-line no-shadow
import $ from 'jquery';
import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Table from '../src/components/utils/Table.vue';
import UserService from '../src/components/users/UserService';
const { userWithSettings } = require('../../../common/vueapp/tests/consts');

console.info = jest.fn(); // don't display console.info messages

global.$ = global.jQuery = $;

Vue.use(BootstrapVue);

const loadFn = jest.fn(() => {});
const infoFn = jest.fn(() => {});

jest.mock('../src/components/users/UserService');

UserService.saveState = jest.fn().mockResolvedValue();
UserService.getPageConfig = jest.fn().mockResolvedValue({
  tableState: {
    order: [['test', 'asc']],
    visibleHeaders: ['test', 'col2']
  }
});

const store = {
  state: {
    user: userWithSettings
  }
};

const props = {
  page: 'test',
  id: 'unique',
  loadData: loadFn,
  actionColumn: true,
  infoRow: true,
  infoRowFunction: infoFn,
  noResults: true,
  sortField: 'test',
  desc: true,
  tableStateName: 'test',
  tableWidthsStateName: 'testWidths',
  columns: [{
    default: true,
    id: 'test',
    sort: 'test',
    name: 'Test',
    width: 140
  }, {
    default: true,
    id: 'col2',
    sort: 'col2',
    help: 'Test 2 help',
    name: 'Test Column 2',
    width: 140
  }, {
    default: true,
    id: 'col3',
    sort: 'col3',
    name: 'Column 3',
    width: 140
  }],
  data: [{
    test: 'test value 0',
    col2: 'column 2 value',
    col3: 'column 3 value'
  }]
};

const $route = {
  path: 'http://localhost:8123/arkime/test',
  query: {}
};

test('table', async () => {
  const {
    getByText, getByRole, getAllByText, getAllByRole, queryByRole,
    getAllByTitle, queryByText, updateProps
  } = render(Table, { store, props, mocks: { $route } });

  await waitFor(() => {
    expect(loadFn).toHaveBeenCalled(); // load data function was called
    expect(UserService.getPageConfig).toHaveBeenCalled(); // get state was called
  });

  getByRole('dropdown'); // displays info column dropdown btn
  // displays column headers (and buttons in dropdown)
  expect(getAllByText('Test Column 2').length).toBe(2);
  // doesn't display non-visible visibleHeaders (just displays toggle)
  expect(getAllByText('Column 3').length).toBe(1);

  getByText('test value 0'); // rows are displayed
  expect(queryByText('column 3 value')).toBeNull(); // but not non-visible col values

  // get the last button (it's the last toggle button)
  const btns = getAllByRole('button');
  const btn = btns[btns.length - 1];
  await fireEvent.click(btn);

  await waitFor(() => {
    expect(infoFn).toHaveBeenCalled(); // info function was called
  });

  await fireEvent.click(getAllByTitle('Test 2 help')[1]);

  await waitFor(() => {
    expect(UserService.saveState).toHaveBeenCalled(); // table state was saved
  });

  props.data = [];
  props.columns = [];
  props.infoRow = false;
  props.actionColumn = false;
  await updateProps(props);

  await waitFor(() => { // displays no results and no action column
    getByText('No results match your search');
    expect(queryByRole('dropdown')).toBeNull();
  });
});
