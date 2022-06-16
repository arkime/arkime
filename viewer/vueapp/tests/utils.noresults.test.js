'use strict';

import { render } from '@testing-library/vue';
import NoResults from '../src/components/utils/NoResults.vue';
const { views } = require('../../../common/vueapp/tests/consts');

const store = {
  state: { views }
};

test('no results', async () => {
  const { getByText, updateProps } = render(NoResults, {
    store,
    props: { recordsTotal: 0 }
  });

  getByText('Oh no, Arkime is empty! There is no data to search.');

  await updateProps({ recordsTotal: undefined });

  getByText('No results or none that match your search within your time range.');

  // displays view by id
  await updateProps({ recordsTotal: undefined, view: '1' });
  getByText('Don\'t forget! You have a view applied to your search:');
  getByText('test view 1');

  // displays view by name
  await updateProps({ recordsTotal: undefined, view: 'test view 2' });
  getByText('Don\'t forget! You have a view applied to your search:');
  getByText('test view 2');
});
