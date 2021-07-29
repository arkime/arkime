'use strict';

import userEvent from '@testing-library/user-event';
import { render } from '@testing-library/vue';
import SegmentSelect from '../src/components/sessions/SegmentSelect.vue';

const props = { segments: 'no' };

test('sessions - segment select', async () => {
  const { getByRole, updateProps } = render(SegmentSelect, { props });

  // uses prop for initial value ------------------------------------------- //
  const segmentSelect = getByRole('listbox');
  expect(segmentSelect.value).toBe('no');

  // can change value ------------------------------------------------------ //
  await userEvent.selectOptions(segmentSelect, 'all');
  expect(segmentSelect.value).toBe('all');

  // props can udpate value ------------------------------------------------ //
  await updateProps({ segments: 'time' });
  expect(segmentSelect.value).toBe('time');
});
