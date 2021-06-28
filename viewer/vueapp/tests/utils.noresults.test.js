import { render } from '@testing-library/vue';
import NoResults from '../src/components/utils/NoResults.vue';

test('no results', async () => {
  const { getByText, updateProps } = render(NoResults, {
    props: { recordsTotal: 0 }
  });

  getByText('Oh no, Arkime is empty! There is no data to search.');

  await updateProps({ recordsTotal: undefined });

  getByText('No results or none that match your search within your time range.');

  await updateProps({ recordsTotal: undefined, view: 'testview' });
  getByText('Don\'t forget! You have a view applied to your search:');
  getByText('testview');
});
