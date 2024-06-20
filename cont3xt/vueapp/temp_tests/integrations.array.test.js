'use strict';

import { vi, test, expect } from 'vitest';
import { render, fireEvent, waitFor, screen } from '@testing-library/vue';
import IntegrationArray from '@/components/integrations/IntegrationArray.vue';
import HighlightableText from '@/utils/HighlightableText.vue';
import TestOne from '@/TestOne.vue';
import TestThree from '@/TestThree.vue';
import TestFour from '@/TestFour.vue';
import { createStore } from 'vuex';

const store = {
  state: {
    renderingArray: false
  },
  mutations: {
    SET_RENDERING_ARRAY: vi.fn()
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

test('increments value on click', async () => {
  render(TestThree)

  // screen has all queries that you can use in your tests.
  // getByText returns the first matching node for the provided text, and
  // throws an error if no elements match or if more than one match is found.
  screen.getByText('Times clicked: 0')

  const button = screen.getByText('increment')

  // Dispatch a native click event to our button element.
  await fireEvent.click(button)
  await fireEvent.click(button)

  screen.getByText('Times clicked: 2')
  screen.getByText('what 0');
  screen.getByText('what 1');
})

test('increments value on click four', async () => {
  render(TestFour)

  // screen has all queries that you can use in your tests.
  // getByText returns the first matching node for the provided text, and
  // throws an error if no elements match or if more than one match is found.
  screen.getByText('Times clicked: 0')

  const button = screen.getByText('increment')

  // Dispatch a native click event to our button element.
  await fireEvent.click(button)
  await fireEvent.click(button)

  screen.getByText('Times clicked: 2')
  screen.getByText('what 0');
  screen.getByText('what 1');
})

// test('Temp test', async () => {
//   render(TestOne, {
//     props: {},
//     global: {
//       plugins: [createStore(store)]
//     }
//   });
//   screen.getByText('Hello');
//   screen.getByText('Goodbye no');
//
//   const btn = screen.getByText('click');
//   // expect(queryByText('1')).toBeInTheDocument();
//   screen.getByText('hi 1');
//   expect(screen.queryByText('hi 2')).not.toBeInTheDocument();
//
//   await fireEvent.click(btn);
//   // await waitFor(() => {
//   screen.getByText('hi 2');
//   expect(screen.queryByText('hi 3')).not.toBeInTheDocument();
//   // }, { timeout: 3000 })
// });

// test('Integration Array - join shows everything with separator', async () => {
//   const { getByText } = render(IntegrationArray, {
//     // store,
//     props: { field, arrayData },
//     plugins: [createStore(store)]
//   });
//
//   getByText('2096 - 2082 - 2083 - 2086 - 2087 - 2095 - 80 - 8880 - 8080 - 8443 - 443');
// });

// test('Integration Array - show more/all/less', async () => {
//   delete field.join;
//
//   const { getByText, queryByText } = render(IntegrationArray, {
//     global: {
//       components: HighlightableText, // TODO: toby
//       plugins: [createStore(store)]
//     },
//     store,
//     props: { field, arrayData, size: 2 }
//   });
//
//   // displays show more/all btns
//   const showAllBtn = getByText('show ALL');
//   const showMoreBtn = getByText('show more...');
//
//   getByText('2096'); // displays 1st item
//   getByText('2082'); // displays 2nd item
//   // doesn't display 3rd
//   expect(queryByText('2083')).not.toBeInTheDocument();
//
//   // can click show more button
//   await fireEvent.click(showAllBtn);
//
//   await waitFor(() => {
//     getByText('2083'); // shows 3rd item
//     getByText('2086'); // shows 4th item
//   }, {/* timeout: 10000 */});
//
//   // // displays show less button now
//   // const showLessBtn = getByText('show less...');
//   //
//   // await waitFor(() => {
//   //   // updates store
//   //   expect(store.mutations.SET_RENDERING_ARRAY).toHaveBeenCalledWith(store.state, false);
//   // });
//   //
//   // // can click show less btn
//   // await fireEvent.click(showLessBtn);
//   // // doesn't display 3rd
//   // expect(queryByText('2083')).not.toBeInTheDocument();
//   // // doesn't display 4th
//   // expect(queryByText('2086')).not.toBeInTheDocument();
//   //
//   // // can click shore more btn
//   // await fireEvent.click(showAllBtn);
//   // await waitFor(() => { // wait for rendering array
//   //   // shows all the items in the array
//   //   for (const item of arrayData) {
//   //     getByText(item);
//   //   }
//   // });
// });
