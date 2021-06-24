import { render } from '@testing-library/vue';
import LoadingComponent from '../src/components/utils/Loading.vue';

test('loading', async () => {
  const { getByText, queryByText, updateProps } = render(LoadingComponent, {
    props: { canCancel: false }
  });

  getByText('I\'m hootin');

  expect(queryByText('cancel')).toBeNull(); // can't see dropdown yet

  await updateProps({ canCancel: true });

  getByText('cancel');
});
