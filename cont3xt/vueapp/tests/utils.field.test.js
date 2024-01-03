'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import VueClipboard from 'vue-clipboard2';
import { render, fireEvent } from '@testing-library/vue';
import Cont3xtField from '../src/utils/Field.vue';

Vue.use(BootstrapVue);
Vue.use(VueClipboard);

window.btoa = jest.fn().mockImplementation((str) => str);

test('field - defaults', async () => {
  const { getByText, getByTestId, queryByTestId } = render(Cont3xtField, {
    props: {
      value: 'field value'
    }
  });

  // value shows up
  const dropdownToggle = getByText('field value');
  await fireEvent.click(dropdownToggle);// dropdown can be opened

  // dropdown options are displayed
  getByTestId('field-dropdown');

  // has all options
  const copyBtn = getByText('copy');
  const pivotBtn = getByText('pivot');

  // copy button can be clicked then it hides the dropdown
  await fireEvent.click(copyBtn);
  expect(queryByTestId('field-dropdown')).not.toBeInTheDocument();

  // pivot button uses current url search parameters
  expect(pivotBtn.href).toBe('http://localhost/?b=field+value');
  expect(window.btoa).toHaveBeenCalledWith('field value');
  expect(window.btoa).toHaveBeenCalledTimes(1);
});

test('field - with options', async () => {
  const { getByText, queryByText } = render(Cont3xtField, {
    props: {
      value: 'field value',
      display: 'field display value',
      options: { copy: 'COPY TEXT', pivot: 'PIVOT TEXT' }
    }
  });

  // value shows up
  expect(queryByText('field value')).not.toBeInTheDocument();
  const dropdownToggle = getByText('field display value');
  await fireEvent.click(dropdownToggle);// dropdown can be opened

  // has all options with provided text from props
  getByText('COPY TEXT');
  getByText('PIVOT TEXT');
});
