'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import VueClipboard from 'vue-clipboard2';
import { render, fireEvent } from '@testing-library/vue';
import Cont3xtField from '../src/utils/Field.vue';

Vue.use(BootstrapVue);
Vue.use(VueClipboard);

window.btoa = jest.fn();

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

  // pivot button can be clicked and uses value
  await fireEvent.click(pivotBtn);
  expect(window.btoa).toHaveBeenCalledWith('field value');
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
  const pivotBtn = getByText('PIVOT TEXT');

  // pivot button can be clicked and uses value (not display)
  await fireEvent.click(pivotBtn);
  expect(window.btoa).toHaveBeenCalledWith('field value');
});
