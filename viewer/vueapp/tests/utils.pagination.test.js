'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import userEvent from '@testing-library/user-event';
import Pagination from '../src/components/utils/Pagination.vue';
import '../src/filters';
import '../../../common/vueapp/vueFilters';

Vue.use(BootstrapVue);

test('pagination max page length', async () => {
  const store = {
    state: { expression: '' }
  };

  const {
    getByText, getByRole
  } = render(Pagination, {
    store,
    routes: [],
    props: {
      infoOnly: false,
      lengthDefault: 100000,
      recordsTotal: 100000,
      recordsFiltered: 100000
    }
  });

  const pageLenSelect = getByRole('listbox');

  expect(pageLenSelect.value).toBe('1000');
  getByText('- 1,000');
});

test('pagination', async () => {
  const store = {
    state: { expression: '' }
  };

  const {
    getByText, getByRole, queryByRole, getByTitle, getAllByRole, updateProps
  } = render(Pagination, {
    store,
    routes: [],
    props: {
      infoOnly: true,
      lengthDefault: 100,
      recordsTotal: 100000,
      recordsFiltered: 50
    }
  });

  // displays info and applys commastring filter
  getByText('Showing 50 entries, filtered from 100,000 total entries');

  // doesn't display paging (info only)
  expect(queryByRole('listbox')).not.toBeInTheDocument();

  await updateProps({
    infoOnly: false,
    lengthDefault: 100,
    recordsTotal: 100000,
    recordsFiltered: 500
  });

  // displays paging controls
  const pageLenSelect = getByRole('listbox');

  // has correct tooltip
  getByTitle('filtered from 100,000 total entries');

  // uses length default and displays it
  expect(pageLenSelect.value).toBe('100');
  getByText('- 100');

  // change page length and displays it
  await userEvent.selectOptions(pageLenSelect, '50');
  expect(pageLenSelect.value).toBe('50');
  getByText('- 50');

  // change the page to page 3
  await fireEvent.click(getAllByRole('menuitemradio')[2]);

  // page 3 shown
  await waitFor(() => { getByText('- 150'); });
});
