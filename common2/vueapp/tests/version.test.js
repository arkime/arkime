'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import { render } from '@testing-library/vue';
import Version from '../Version';

Vue.use(BootstrapVue);

Vue.prototype.$constants = {
  VERSION: '4.0',
  BUILD_DATE: 1652214936000,
  BUILD_VERSION: 'abcdefg'
};

test('version', async () => {
  const {
    getByText, getByTitle
  } = render(Version, {
    props: { timezone: 'gmt' }
  });

  // displays version number
  getByText('v4.0');

  // displays build info
  getByTitle('abcdefg @ 2022/05/10 20:35:36 UTC');
});
