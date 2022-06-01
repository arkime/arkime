'use strict';

import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import IntegrationArray from '../src/components/integrations/IntegrationArray.vue';

const store = {
  state: {
    renderingArray: false
  },
  mutations: {
    SET_RENDERING_ARRAY: jest.fn()
  },
  getters: {
    getRenderingArray (state) {
      return state.renderingArray;
    }
  }
};

const field = {
  join: ' - ',
  type: 'array',
  label: 'ports',
  path: ['ports']
};

const arrayData = [2096, 2082, 2083, 2086, 2087, 2095, 80, 8880, 8080, 8443, 443];

test('Integration Array - join shows everything with separator', async () => {
  const { getByText } = render(IntegrationArray, {
    store,
    props: { field, arrayData }
  });

  getByText('2096 - 2082 - 2083 - 2086 - 2087 - 2095 - 80 - 8880 - 8080 - 8443 - 443');
});

test('Integration Array - show more/all/less', async () => {
  delete field.join;

  const { getByText, queryByText } = render(IntegrationArray, {
    store,
    props: { field, arrayData, size: 2 }
  });

  // displays show more/all btns
  const showAllBtn = getByText('show ALL');
  const showMoreBtn = getByText('show more...');

  getByText('2096'); // displays 1st item
  getByText('2082'); // displays 2nd item
  // doesn't display 3rd
  expect(queryByText('2083')).not.toBeInTheDocument();

  // can click show more button
  await fireEvent.click(showMoreBtn);
  getByText('2083'); // shows 3rd item
  getByText('2086'); // shows 4th item

  // displays show less button now
  const showLessBtn = getByText('show less...');

  // updates store
  expect(store.mutations.SET_RENDERING_ARRAY).toHaveBeenCalledWith(store.state, false);

  // can click show less btn
  await fireEvent.click(showLessBtn);
  // doesn't display 3rd
  expect(queryByText('2083')).not.toBeInTheDocument();
  // doesn't display 4th
  expect(queryByText('2086')).not.toBeInTheDocument();

  // can click shore more btn
  await fireEvent.click(showAllBtn);
  await waitFor(() => { // wait for rendering array
    // shows all the items in the array
    for (const item of arrayData) {
      getByText(item);
    }
  });
});
