'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, waitFor, fireEvent } from '@testing-library/vue';
import IntegrationTable from '../src/components/integrations/IntegrationTable.vue';

Vue.use(BootstrapVue);

const store = {
  state: {
    renderingTable: false
  },
  mutations: {
    SET_RENDERING_TABLE: jest.fn()
  }
};

const fields = [{
  label: 'col1',
  path: ['col1'],
  type: 'string'
}, {
  label: 'col2',
  path: ['col2'],
  type: 'string'
}, {
  label: 'col3',
  path: ['col3'],
  type: 'string'
}, {
  label: 'col4',
  path: ['col4'],
  type: 'string'
}];

const tableData = [{
  col1: 'row1col1',
  col2: 'row1col2',
  col3: 'row1col3',
  col4: 'row1col4'
}, {
  col1: 'row2col1',
  col2: 'row2col2',
  col3: 'row2col3',
  col4: 'row2col4'
}, {
  col1: 'row3col1',
  col2: 'row3col2',
  col3: 'row3col3',
  col4: 'row3col4'
}, {
  col1: 'row4col1',
  col2: 'row4col2',
  col3: 'row4col3',
  col4: 'row4col4'
}];

const tableObject = {
  col1: 'objcol1',
  col2: 'objcol2',
  col3: 'objcol3',
  col4: 'objcol4'
};

function displaysRows (first, last, getByText) {
  for (let i = first; i <= last; i++) {
    const row = tableData[i];
    for (const col in row) {
      getByText(row[col]);
    }
  }
}

function doesNotDisplayRows (first, last, queryByText) {
  for (let i = first; i <= last; i++) {
    const row = tableData[i];
    for (const col in row) {
      expect(queryByText(row[col])).not.toBeInTheDocument();
    }
  }
}

test('Integration Table - basic', async () => {
  const { getByText, getAllByText } = render(IntegrationTable, {
    store,
    props: { fields, tableData }
  });

  // displays table column headers and field selection dropdown for search
  for (const field of fields) {
    getAllByText(field.label);
  }

  await waitFor(() => { // wait for table to render
    for (const row of tableData) { // displays all values
      for (const col in row) {
        getByText(row[col]);
      }
    }
  });
});

test('Integration Table - object', async () => {
  const { getByText, getAllByText } = render(IntegrationTable, {
    store,
    props: { fields, tableData: tableObject }
  });

  // displays table column headers and field selection dropdown for search
  for (const field of fields) {
    getAllByText(field.label);
  }

  await waitFor(() => { // wait for table to render
    for (const col in tableObject) { // displays all values
      getByText(tableObject[col]);
    }
  });
});

test('Integration Table - show more/all/less', async () => {
  const { getByText, queryByText } = render(IntegrationTable, {
    store,
    props: { fields, tableData, size: 2 }
  });

  // displays show more/all btns
  const showAllBtn = getByText('show ALL');
  const showMoreBtn = getByText('show more...');

  await waitFor(() => { // wait for table to render
    displaysRows(0, 1, getByText); // displays first 2 rows
  });

  doesNotDisplayRows(2, 3, queryByText); // doesn't display last 2 rows

  // can click show more button
  await fireEvent.click(showMoreBtn);
  await waitFor(() => { // wait for table to render
    displaysRows(2, 3, getByText); // displays last 2 rows
  });

  // displays show less button now
  const showLessBtn = getByText('show less...');

  // can click show less btn
  await fireEvent.click(showLessBtn);
  doesNotDisplayRows(2, 3, queryByText); // doesn't display last 2 rows

  // can click shore ALL btn
  await fireEvent.click(showAllBtn);
  // updates store
  expect(store.mutations.SET_RENDERING_TABLE).toHaveBeenCalledWith(store.state, true);
  await waitFor(() => { // wait for rendering table
    displaysRows(0, 3, getByText); // displays all rows
  });

  // updates store
  expect(store.mutations.SET_RENDERING_TABLE).toHaveBeenCalledWith(store.state, false);
});

test('Integration Table - table sorting', async () => {
  const {
    getAllByText, getByTestId, queryByText
  } = render(IntegrationTable, {
    store,
    props: {
      fields,
      tableData,
      defaultSortField: 'col1',
      defaultSortDirection: 'asc'
    }
  });

  // displays sorted field as sorted and is sorted asc as passed by prop
  getByTestId('sort-asc-col1');
  expect(queryByText('sort-desc-col1')).not.toBeInTheDocument();

  // other columns are not sorted
  getByTestId('sort-col2');
  getByTestId('sort-col3');
  getByTestId('sort-col4');

  // can click sorted column to sort other direction
  const col1Header = getAllByText('col1')[1];
  await fireEvent.click(col1Header);
  await waitFor(() => { getByTestId('sort-desc-col1'); });

  // can cick another column to sort it and starts sorting desc
  const col2Header = getAllByText('col2')[1];
  await fireEvent.click(col2Header);
  await waitFor(() => { getByTestId('sort-desc-col2'); });
});

test('Integration Table - table searching', async () => {
  const {
    getByText, queryByText, getByPlaceholderText, getAllByRole
  } = render(IntegrationTable, {
    store,
    props: { fields, tableData }
  });

  // can update input and issue search
  const searchInput = getByPlaceholderText('Search table values');
  await fireEvent.update(searchInput, 'row4col4');

  await waitFor(() => { // only shows row4
    displaysRows(3, 3, getByText);
    doesNotDisplayRows(0, 2, queryByText);
  });

  const fieldSelectCheckbox = getAllByRole('checkbox')[3];
  fireEvent.click(fieldSelectCheckbox);
  await waitFor(() => { // doesn't show any rows
    doesNotDisplayRows(0, 3, queryByText);
  });
});
