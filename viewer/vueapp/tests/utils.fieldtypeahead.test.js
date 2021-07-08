'use strict';

import { render, fireEvent, waitFor } from '@testing-library/vue';
import FieldTypeahead from '../src/components/utils/FieldTypeahead.vue';
import UserService from '../src/components/users/UserService';
import '../src/filters.js';
const { fields } = require('./consts');

console.info = jest.fn(); // don't display console.info messages

jest.mock('../src/components/users/UserService');

UserService.getState = jest.fn().mockResolvedValue({
  data: {
    fields: [{
      friendlyName: 'Test History Field',
      dbField: 'test',
      exp: 'test'
    }]
  }
});

const $route = {
  path: 'http://localhost:8123/arkime/sessions?queryParam=ip.src',
  query: { queryParam: 'ip.src' }
};

const renderOptions = {
  mocks: { $route },
  props: {
    dropup: false,
    fields: fields,
    page: 'testpage',
    initialValue: 'Src IP',
    queryParam: 'queryParam'
  }
};

test('field typeahead has fields and history', async () => {
  const {
    getByText, getByPlaceholderText
  } = render(FieldTypeahead, renderOptions);
  const input = getByPlaceholderText('Begin typing to search for fields');

  expect(input.value).toBe('Src IP'); // input is intial value from props

  getByText('Dst IP'); // has normal field

  await waitFor(() => { // has history fields (need to wait for userservice)
    getByText('Test History Field');
  });
});

test('field typeahead dropdown toggles', async () => {
  const {
    getByText, getByPlaceholderText, getByRole, queryByRole, queryByText
  } = render(FieldTypeahead, renderOptions);
  const input = getByPlaceholderText('Begin typing to search for fields');

  await fireEvent.blur(input);

  expect(queryByRole('dropdown')).toBeNull(); // can't see dropdown yet

  await fireEvent.click(input);

  getByRole('dropdown'); // dropdown can be seen now

  await fireEvent.keyDown(input, { key: 'Escape', code: 'Escape', keyCode: 27, charCode: 0 });

  await waitFor(() => { // escape closes dropdown
    expect(queryByRole('dropdown')).toBeNull();
  });

  await fireEvent.click(input);
  await fireEvent.update(input, 'Dst IP');

  expect(input.value).toBe('Dst IP'); // input is intial value from props

  await waitFor(() => { // fields have been filtered
    expect(queryByText('Src IP')).toBeNull();
    getByText('Dst IP');
  });
});

test('field typeahead selected field gets emitted', async () => {
  const {
    getAllByText, getByPlaceholderText, emitted
  } = render(FieldTypeahead, renderOptions);
  const input = getByPlaceholderText('Begin typing to search for fields');

  await fireEvent.update(input, '');

  await waitFor(async () => {
    const dropdownItem = getAllByText('Dst IP')[0];

    await fireEvent.click(dropdownItem);

    expect(emitted()).toHaveProperty('fieldSelected');
    expect(emitted().fieldSelected[0][0]).toMatchObject({
      aliases: ['ip.dst:port'],
      category: 'ip',
      dbField: 'destination.ip',
      dbField2: 'dstIp',
      fieldECS: 'destination.ip',
      exp: 'ip.dst',
      friendlyName: 'Dst IP',
      group: 'general',
      help: 'Destination IP',
      portField: 'destination.port',
      portField2: 'dstPort',
      portFieldECS: 'destination.port',
      type: 'ip'
    });
  });
});

test('field typeahead key up/down focuses on field', async () => {
  const {
    getByPlaceholderText, emitted
  } = render(FieldTypeahead, renderOptions);
  const input = getByPlaceholderText('Begin typing to search for fields');

  await fireEvent.click(input); // focus on input to show dropdown
  await fireEvent.keyDown(input, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 }); // focus on history item
  await fireEvent.keyDown(input, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 }); // focus on first field
  await fireEvent.keyDown(input, { key: 'ArrowDown', code: 'ArrowDown', keyCode: 40, charCode: 0 }); // focus on second field
  await fireEvent.keyDown(input, { key: 'ArrowUp', code: 'ArrowUp', keyCode: 38, charCode: 0 }); // focus on first field
  await fireEvent.keyUp(input, { key: 'Enter', code: 'Enter', keyCode: 13, charCode: 0 }); // hit enter to select first field

  await waitFor(() => {
    expect(emitted()).toHaveProperty('fieldSelected');
    expect(emitted().fieldSelected[0][0]).toMatchObject({ // first field was selected
      dbField: '_id',
      dbField2: '_id',
      exp: 'id',
      friendlyName: 'Arkime ID',
      group: 'general',
      help: 'Arkime ID for the session',
      noFacet: 'true',
      type: 'termfield'
    });
  });
});

test('field typeahead can add field to history', async () => {
  const {
    getByText, getAllByText, getByPlaceholderText
  } = render(FieldTypeahead, renderOptions);
  const input = getByPlaceholderText('Begin typing to search for fields');

  await fireEvent.focus(input);

  const dropdownItem = getByText('All IP fields');

  await fireEvent.click(dropdownItem);

  await waitFor(async () => {
    expect(getAllByText('All IP fields').length).toBe(2);
  });
});

test('field typeahead can remove field from history', async () => {
  const {
    getAllByTitle, queryByText, getByPlaceholderText
  } = render(FieldTypeahead, renderOptions);
  const input = getByPlaceholderText('Begin typing to search for fields');

  await fireEvent.focus(input);

  const removes = getAllByTitle('Remove from history');

  await fireEvent.click(removes[removes.length - 1]);

  await waitFor(async () => {
    expect(queryByText('Test History Field')).toBeNull();
  });
});
