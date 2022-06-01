'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, waitFor } from '@testing-library/vue';
import FieldActions from '../src/components/sessions/FieldActions.vue';
import { fieldsMap } from '../../../common/vueapp/tests/consts';

const store = {
  state: {
    fieldsMap,
    fieldActions: {
      testAction: {
        category: 'ip',
        name: 'Field Action %FIELDNAME%!',
        url: 'https://www.asdf.com?expression=%EXPRESSION%&date=%DATE%&field=%FIELD%&dbField=%DBFIELD%'
      }
    }
  }
};

Vue.use(BootstrapVue);

test('field actions', async () => {
  const $route = {
    query: {
      stopTime: '1653278399',
      startTime: '1621828800',
      expression: 'ip.src == 10.0.0.1'
    }
  };

  const { getByTestId, getByText } = render(FieldActions, {
    store,
    mocks: { $route },
    props: {
      expr: 'ip.src'
    }
  });

  await waitFor(() => {
    // has separator by default
    getByTestId('separator');
  });

  // displays action and replaces dropdown item text with values
  const menuItem = getByText('Field Action Src IP!');

  // has href and fills in values
  expect(menuItem.href).toBe('https://www.asdf.com/?expression=ip.src%20%3D%3D%2010.0.0.1&date=startTime=1621828800&stopTime=1653278399&field=ip.src&dbField=source.ip');
});

test('field actions - no separator', async () => {
  const $route = {
    query: {
      date: -1,
      expression: 'ip.src == 10.0.0.1'
    }
  };

  const { queryByTestId, getByText } = render(FieldActions, {
    store,
    mocks: { $route },
    props: {
      expr: 'ip.src',
      separator: false
    }
  });

  let menuItem;
  await waitFor(() => {
    // displays action and replaces dropdown item text with values
    menuItem = getByText('Field Action Src IP!');
  });

  // separator excluded (based on prop)
  expect(queryByTestId('separator')).not.toBeInTheDocument();

  // has href and fills in values
  expect(menuItem.href).toBe('https://www.asdf.com/?expression=ip.src%20%3D%3D%2010.0.0.1&date=-1&field=ip.src&dbField=source.ip');
});
