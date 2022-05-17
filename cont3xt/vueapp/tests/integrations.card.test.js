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

const integrationsData = {
  _query: 'threatbutt.com',
  data: {
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
    integrationsData,
    displayIntegration: {
      source: 'Whois',
      itype: 'domain',
      value: integrations.Whois
    }
  },
  mutations: {
    SET_RENDERING_CARD: jest.fn(),
    SET_INTEGRATION_DATA: jest.fn()
  },
  getters: {
    getIntegrations (state) {
      return state.integrations;
    },
    getIntegrationData (state) {
      return state.integrationsData;
    }
  }
};

test('Integration Card', async () => {
  Cont3xtService.refresh = jest.fn().mockResolvedValue(integrationsData);

  const {
    getByTitle, getByText, emitted
  } = render(IntegrationCard, { store });

  // displays labels & values
  getByText('registrar');
  getByText('DNC Holdings, Inc');
  getByText('updatedDate');
  getByText('2022/03/06 20:16:37');
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
    value: store.state.displayIntegration.value,
    itype: store.state.displayIntegration.itype,
    source: store.state.displayIntegration.source
  });

  // and emits update-results with the appropriate values
  await waitFor(() => {
    expect(emitted()).toHaveProperty('update-results');
    expect(emitted()['update-results'][0][0].data).toBe(integrationsData);
    expect(emitted()['update-results'][0][0].value).toBe(store.state.displayIntegration.value);
    expect(emitted()['update-results'][0][0].itype).toBe(store.state.displayIntegration.itype);
    expect(emitted()['update-results'][0][0].source).toBe(store.state.displayIntegration.source);
  });

  // error display
  Cont3xtService.refresh = jest.fn().mockRejectedValue('test error');
  await fireEvent.click(refreshBtn);
  await waitFor(() => {
    getByText('test error');
  });
});

test('Integration Card - empty card ', async () => {
  store.state.integrations = {
    Whois: {
      order: 100,
      doable: true,
      icon: 'integrations/whois/icon.png'
    }
  };

  const { getByText } = render(IntegrationCard, { store });

  getByText(/Missing information to render the data./);
});

test('Integration Card - empty data ', async () => {
  store.state.integrationsData = {
    _query: 'threatbutt.com',
    data: {
      _cont3xt: {
        count: 1,
        createTime: 1652796469956
      }
    }
  };

  const { getByText } = render(IntegrationCard, { store });

  getByText('No data');
});
