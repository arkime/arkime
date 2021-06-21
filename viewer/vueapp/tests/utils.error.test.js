import { render } from '@testing-library/vue';
import ErrorComponent from '../src/components/utils/Error.vue';

test('errors', async () => {
  const { getByTestId, getByText, updateProps } = render(ErrorComponent, {
    props: { message: 'test error message' }
  });

  getByText('test error message');

  await updateProps({ message: 'a different error message' });
  getByText('a different error message');

  await updateProps({
    message: '',
    messageHtml: '<div data-testid="0">An HTML test message</div>'
  });

  expect(getByTestId('0').textContent).toBe('An HTML test message');
});
