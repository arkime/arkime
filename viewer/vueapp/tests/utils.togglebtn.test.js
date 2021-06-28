import { render, fireEvent } from '@testing-library/vue';
import ToggleBtn from '../src/components/utils/ToggleBtn.vue';

test('toggle button', async () => {
  const { getByRole, emitted } = render(ToggleBtn);

  const btn = getByRole('button');

  await fireEvent.click(btn);

  expect(emitted()).toHaveProperty('toggle');
});
