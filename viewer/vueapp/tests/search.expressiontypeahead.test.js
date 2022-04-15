'use strict';

import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import ExpressionTypeahead from '../src/components/search/ExpressionTypeahead.vue';
import UserService from '../src/components/users/UserService';
import FieldService from '../src/components/search/FieldService';
import countries from '../src/components/search/countries.json';
const { fields, fieldsMap } = require('./consts');

console.info = jest.fn(); // don't display console.info messages

global.$ = global.jQuery = $;

Vue.use(VueAxios, axios);
Vue.use(BootstrapVue);

jest.mock('../src/components/users/UserService');
jest.mock('../src/components/search/FieldService');

const store = {
  state: {
    expression: '',
    focusSearch: false,
    shiftKeyHold: false,
    views: {},
    fieldsMap,
    fieldsArr: fields,
    fieldhistory: [],
    hideViz: false,
    stickyViz: false
  },
  mutations: {
    toggleHideViz: jest.fn(),
    toggleStickyViz: jest.fn(),
    setFocusSearch: jest.fn(),
    setExpression (state, value) {
      state.expression = value;
    }
  }
};

function addCaretPosition (input, pos) {
  for (let i = 0; i <= pos; i++) {
    fireEvent.keyDown(input, { key: 'ArrowRight', code: 'ArrowRight', keyCode: 39, charCode: 0 });
  }
}

beforeEach(() => {
  UserService.getState = jest.fn().mockResolvedValue({ data: {} });
  UserService.saveState = jest.fn().mockResolvedValue({});
  FieldService.getCountryCodes = jest.fn().mockResolvedValue(countries);
  FieldService.getValues = jest.fn(() => {
    const source = Vue.axios.CancelToken.source();
    const promise = new Promise((resolve, reject) => {
      resolve([
        '10.0.0.1', '10.0.0.2', '10.0.0.4', '10.0.0.6', '10.0.0.3', '10.0.0.5',
        '10.0.0.7', '10.0.0.8', '10.0.0.9', '10.0.0.16', '10.0.0.32', '10.0.2.15'
      ]);
    });
    return { promise, source };
  });

  FieldService.getField = jest.fn((search) => {
    if (!search) { return undefined; }

    if (store.state.fieldsMap[search]) return store.state.fieldsMap[search];

    return undefined;
  });
});

test('search expression typeahead', async () => {
  const $route = { query: {} };

  const {
    getByPlaceholderText, getByTitle, queryByTitle, getByText, queryByText
  } = render(ExpressionTypeahead, {
    store,
    mocks: { $route }
  });

  // search expression typehead component renders
  const searchInput = getByPlaceholderText('Search');

  // clear button works ---------------------------------------------------- //
  fireEvent.click(searchInput); // focus on input
  fireEvent.update(searchInput, 'ip.src');
  expect(searchInput.value).toBe('ip.src');
  await fireEvent.click(getByTitle('Remove the search text'));
  expect(searchInput.value).toBe('');

  // can get typeahead results --------------------------------------------- //
  await fireEvent.update(searchInput, 'ip.sr');

  let srcIPResult;
  await waitFor(() => { // need to wait for the debounce
    srcIPResult = getByTitle('Source IP'); // autocompletes fields
  });

  // arrow keys select a result -------------------------------------------- //
  await fireEvent.keyDown(searchInput, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 });
  expect(srcIPResult).toHaveClass('active');

  // enter click adds the selected result to the query and clears results -- //
  await fireEvent.keyDown(searchInput, { key: 'Enter', code: 'Enter', keyCode: 13, charCode: 0 });
  expect(searchInput.value).toBe('ip.src ');
  expect(queryByTitle('Source IP')).not.toBeInTheDocument();

  // selecting a field adds it to the field history ------------------------ //
  expect(UserService.saveState).toHaveBeenCalledWith({ fields: [fieldsMap['ip.src']] }, 'fieldHistory');

  // clicking a result adds it to the query -------------------------------- //
  await fireEvent.update(searchInput, 'ip.src =');
  addCaretPosition(searchInput, 8);

  let eqeqResult;
  await waitFor(() => { // need to wait for the debounce
    eqeqResult = getByText('=='); // autocompletes operators
  });
  await fireEvent.click(eqeqResult);
  expect(searchInput.value).toBe('ip.src == ');

  // escape removes results ------------------------------------------------ //
  await fireEvent.update(searchInput, 'ip.src == 10.0.0');
  addCaretPosition(searchInput, 6);
  await waitFor(() => { // need to wait for the debounce
    getByText('10.0.0.1'); // autocompletes values
  });
  await fireEvent.keyDown(searchInput, { key: 'Escape', code: 'Escape', keyCode: 27, charCode: 0 });
  expect(queryByText('10.0.0.1')).not.toBeInTheDocument();

  // can remove from history ----------------------------------------------- //
  await fireEvent.update(searchInput, 'ip.src == 10.0.0.1 && ip.sr');
  addCaretPosition(searchInput, 11);

  let removeFromHistoryBtn;
  await waitFor(() => { // need to wait for the debounce
    removeFromHistoryBtn = getByTitle('Remove ip.src from your field history');
  });
  await fireEvent.click(removeFromHistoryBtn);
  expect(UserService.saveState).toHaveBeenCalledWith({ fields: [] }, 'fieldHistory');

  // autocomplete values -------------------------------------------------- //
  await fireEvent.update(searchInput, 'ip.src == 10.0.0.1 && ip.dst == [10.0');
  addCaretPosition(searchInput, 10);

  await waitFor(() => { // need to wait for the debounce
    getByText('10.0.0.2'); // autocompletes values
  });
  await fireEvent.keyDown(searchInput, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 });
  await fireEvent.keyDown(searchInput, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 });
  await fireEvent.keyDown(searchInput, { key: 'Enter', code: 'Enter', keyCode: 13, charCode: 0 });
  expect(searchInput.value).toBe('ip.src == 10.0.0.1 && ip.dst == [10.0.0.2 ');

  await fireEvent.update(searchInput, 'ip.src == 10.0.0.1 && ip.dst == [10.0.0.2,10.0');
  addCaretPosition(searchInput, 3);

  await waitFor(() => { // need to wait for the debounce
    getByText('10.0.0.4'); // autocompletes values
  });
  await fireEvent.keyDown(searchInput, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 });
  await fireEvent.keyDown(searchInput, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 });
  await fireEvent.keyDown(searchInput, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 });
  await fireEvent.keyDown(searchInput, { key: 'Enter', code: 'Enter', keyCode: 13, charCode: 0 });
  expect(searchInput.value).toBe('ip.src == 10.0.0.1 && ip.dst == [10.0.0.2,10.0.0.4 ');
});
