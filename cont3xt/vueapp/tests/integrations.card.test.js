'use strict';

import Vue from 'vue';
import VueMoment from 'vue-moment';
import moment from 'moment-timezone';
import BootstrapVue from 'bootstrap-vue';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Cont3xtService from '../src/components/services/Cont3xtService';
import IntegrationCard from '../src/components/integrations/IntegrationCard.vue';
import '../src/utils/filters';

Vue.use(BootstrapVue);
Vue.use(VueMoment, { moment });

jest.mock('../src/components/services/Cont3xtService');

const results = {
  domain: {
    'threatbutt.com': {
      Whois: {
        registrar: 'DNC Holdings, Inc',
        updatedDate: '2022-03-07T01:16:37Z',
        nested: {
          test: 'value!'
        },
        _cont3xt: {
          count: 1,
          createTime: 1652796469956
        }
      }
    }
  }
};

const integrations = {
  Whois: {
    card: {
      title: 'Whois for %{query}',
      fields: [{
        label: 'updatedDate',
        path: ['updatedDate'],
        type: 'date'
      }, {
        label: 'registrar',
        path: ['registrar'],
        type: 'string'
      }, {
        label: 'nested test',
        path: ['nested', 'test'],
        type: 'string'
      }]
    },
    order: 100,
    doable: true,
    icon: 'integrations/whois/icon.png'
  }
};

const store = {
  state: {
    integrations,
    results
  },
  mutations: {
    SET_RENDERING_CARD: jest.fn(),
    SET_INTEGRATION_RESULT: jest.fn()
  },
  getters: {
    getIntegrations (state) {
      return state.integrations;
    },
    getResults (state) {
      return state.results;
    }
  }
};

const indicator = { itype: 'domain', query: 'threatbutt.com' };

test('Integration Card', async () => {
  Cont3xtService.refresh = jest.fn().mockResolvedValue(results.domain['threatbutt.com'].Whois);

  const {
    getByTitle, getByText, emitted
  } = render(IntegrationCard, {
    store,
    props: {
      indicator,
      source: 'Whois'
    }
  });

  // displays labels & values
  getByText('registrar');
  getByText('DNC Holdings, Inc');
  getByText('updatedDate');
  getByText('nested test');
  getByText('value!');

  // displays card buttons
  getByTitle('Copy as raw JSON');
  getByTitle('Download as raw JSON');
  const refreshBtn = getByTitle(/Queried/);

  // refresh button calls cont3xt service
  await fireEvent.click(refreshBtn);
  expect(store.mutations.SET_RENDERING_CARD).toHaveBeenCalledWith(store.state, true);
  expect(Cont3xtService.refresh).toHaveBeenCalledWith({
    indicator,
    source: 'Whois'
  });

  // and emits update-results with the appropriate values
  await waitFor(() => {
    expect(emitted()).toHaveProperty('update-results');
    expect(emitted()['update-results'][0][0].data).toBe(results.domain['threatbutt.com'].Whois);
    expect(emitted()['update-results'][0][0].indicator).toBe(indicator);
    expect(emitted()['update-results'][0][0].source).toBe('Whois');
  });

  // error display
  Cont3xtService.refresh = jest.fn().mockRejectedValue('test error');
  await fireEvent.click(refreshBtn);
  await waitFor(() => {
    getByText('test error');
  });
});

test('Integration Card - empty card', async () => {
  store.state.integrations = {
    Whois: {
      order: 100,
      doable: true,
      icon: 'integrations/whois/icon.png'
    }
  };

  const { getByText } = render(IntegrationCard, {
    store,
    props: {
      indicator,
      source: 'Whois'
    }
  });

  getByText(/Missing information to render the data./);
});

test('Integration Card - empty data', async () => {
  store.state.results = {
    domain: {
      'threatbutt.com': {
        Whois: {
          _cont3xt: {
            count: 1,
            createTime: 1652796469956
          }
        }
      }
    }
  };

  const { getByText } = render(IntegrationCard, {
    store,
    props: {
      indicator,
      source: 'Whois'
    }
  });

  getByText('No data');
});
