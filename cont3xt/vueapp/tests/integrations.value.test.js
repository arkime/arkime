'use strict';

import Vue from 'vue';
import '@testing-library/jest-dom';
import BootstrapVue from 'bootstrap-vue';
import { render, waitFor } from '@testing-library/vue';
import IntegrationValue from '../src/components/integrations/IntegrationValue.vue';
import '../src/utils/filters';

Vue.use(BootstrapVue);

const integrationsData = {
  _query: 'threatbutt.com',
  data: {
    registrar: 'DNC Holdings, Inc',
    defang: 'https://threatbutt.com/map',
    updatedDate: '2022-03-07T01:16:37Z',
    nested: { test: 'value!' },
    array: [80, 8080, 443],
    ms: 1652884457000,
    seconds: 1652884457,
    url: 'https://buttthreat.com/',
    json: '{test: "json", is: ["amazing1", "amazing2"]}',
    table: { col1: 'asdf', col2: 'asdf2' },
    truncateme: 'asdfghjklasdfghjklasdfghjklasdfghjklasdfghjklasdfg - asdfghjklasdfghjklasdfghjklasdfghjklasdfghjklasdfg',
    _cont3xt: {
      count: 1,
      createTime: 1652796469956
    }
  }
};

const field1 = {
  label: 'registrar',
  path: ['registrar'],
  type: 'string'
};
const field2 = {
  label: 'nested test',
  path: ['nested', 'test'],
  type: 'string'
};
const field3 = {
  label: 'defang test',
  path: ['defang'],
  type: 'string',
  defang: true
};
const field4 = {
  label: 'updatedDate',
  path: ['updatedDate'],
  type: 'date'
};
const field5 = {
  label: 'array test',
  path: ['array'],
  type: 'array',
  join: ', '
};
const field6 = {
  label: 'ms test',
  path: ['ms'],
  type: 'ms'
};
const field7 = {
  label: 'seconds test',
  path: ['seconds'],
  type: 'seconds'
};
const field8 = {
  label: 'url test',
  path: ['url'],
  type: 'url'
};
const field9 = {
  label: 'json test',
  path: ['json'],
  type: 'json'
};
const field10 = {
  label: 'table test',
  path: ['table'],
  type: 'table',
  fields: [{
    label: 'col1',
    path: ['col1'],
    type: 'string'
  }, {
    label: 'col2',
    path: ['col2'],
    type: 'string'
  }]
};
const field11 = {
  label: 'truncate test',
  path: ['truncateme'],
  type: 'string'
};

const store = {
  state: {
    renderingArray: false,
    renderingTable: false,
    displayIntegration: {
      source: 'Whois',
      itype: 'domain'
    }
  },
  getters: {
    getRenderingArray (state) {
      return state.renderingArray;
    },
    getRenderingTable (state) {
      return state.renderingTable;
    },
    getIntegrationData (state) {
      return state.integrationData;
    }
  },
  mutations: {
    SET_RENDERING_TABLE (state, data) {
      state.renderingTable = data;
    }
  }
};

test('Integration Value - simple string', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field1,
      data: integrationsData.data
    }
  });

  getByText('registrar'); // displays label
  getByText('DNC Holdings, Inc'); // displays value
});

test('Integration Value - nested string', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field2,
      data: integrationsData.data
    }
  });

  getByText('nested test'); // displays label
  getByText('value!'); // displays value
});

test('Integration Value - defanged string', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field3,
      data: integrationsData.data
    }
  });

  getByText('defang test'); // displays label
  getByText('hXXps://threatbutt[.]com/map'); // displays defanged value
});

test('Integration Value - date', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field4,
      data: integrationsData.data
    }
  });

  getByText('updatedDate'); // displays label
  getByText(/2022\/03/); // displays formatted date
});

test('Integration Value - array', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field5,
      data: integrationsData.data
    }
  });

  getByText('array test'); // displays label
  getByText('80, 8080, 443'); // displays array
});

test('Integration Value - ms', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field6,
      data: integrationsData.data
    }
  });

  getByText('ms test'); // displays label
  getByText('2022/05/18 14:34:17 UTC'); // displays formatted ms
});

test('Integration Value - seconds', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field7,
      data: integrationsData.data
    }
  });

  getByText('seconds test'); // displays label
  getByText('2022/05/18 14:34:17 UTC'); // displays formatted seconds
});

test('Integration Value - url', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field8,
      data: integrationsData.data
    }
  });

  getByText('url test'); // displays label
  const anchor = getByText('https://buttthreat.com/'); // displays url
  expect(anchor.href).toBe('https://buttthreat.com/'); // url has href
});

test('Integration Value - json', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field9,
      data: integrationsData.data
    }
  });

  getByText('json test'); // displays label
  getByText(JSON.stringify(integrationsData.data.json, null, 2)); // displays stringified json
});

test('Integration Value - table', async () => {
  const { getByText } = render(IntegrationValue, {
    store,
    props: {
      field: field10,
      data: integrationsData.data
    }
  });

  // displays table label
  getByText('table test');
  // displays col headers
  getByText('col1');
  getByText('col2');
  // displays col values
  await waitFor(() => { // wait for table transition
    getByText('asdf');
    getByText('asdf2');
  });
});

test('Integration Value - hide label', async () => {
  const { getByText, queryByText } = render(IntegrationValue, {
    store,
    props: {
      field: field1,
      hideLabel: true,
      data: integrationsData.data
    }
  });

  // doesn't display label
  expect(queryByText('registrar')).not.toBeInTheDocument();
  getByText('DNC Holdings, Inc'); // displays value
});

test('Integration Value - truncate value default', async () => {
  const {
    getByText, queryByText, getByTitle
  } = render(IntegrationValue, {
    store,
    props: {
      field: field11,
      truncate: true,
      data: integrationsData.data
    }
  });

  const value = integrationsData.data.truncateme;

  // doesn't display full value
  expect(queryByText(value)).not.toBeInTheDocument();
  // displays truncated value
  getByText(`${value.substring(0, 100)}...`);
  // display full value in title
  getByTitle(value);
});

test('Integration Value - truncate value', async () => {
  field11.len = 20;

  const {
    getByText, queryByText, getByTitle
  } = render(IntegrationValue, {
    store,
    props: {
      field: field11,
      truncate: true,
      data: integrationsData.data
    }
  });

  const value = integrationsData.data.truncateme;

  // doesn't display full value
  expect(queryByText(value)).not.toBeInTheDocument();
  // displays truncated value
  getByText(`${value.substring(0, field11.len)}...`);
  // display full value in title
  getByTitle(value);
});
