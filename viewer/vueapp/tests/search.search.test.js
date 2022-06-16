'use strict';

import Vue from 'vue';
import axios from 'axios';
import VueAxios from 'vue-axios';
import BootstrapVue from 'bootstrap-vue';
// eslint-disable-next-line no-shadow
import $ from 'jquery';
import '@testing-library/jest-dom';
import { render, fireEvent, waitFor } from '@testing-library/vue';
import Search from '../src/components/search/Search.vue';
import UserService from '../src/components/users/UserService';
import HasPermission from '../src/components/utils/HasPermission.vue';
import SessionsService from '../src/components/sessions/SessionsService';
import '../src/filters.js';
import '../../../common/vueapp/vueFilters';
const { userWithSettings, fields, views } = require('../../../common/vueapp/tests/consts');

global.$ = global.jQuery = $;

Vue.use(VueAxios, axios);
Vue.use(BootstrapVue);
Vue.directive('has-permission', HasPermission);

Vue.prototype.$constants = {
  MOLOCH_MULTIVIEWER: false
};

jest.mock('../src/components/users/UserService');
jest.mock('../src/components/sessions/SessionsService');

const $router = { push: jest.fn() };

const store = {
  state: {
    user: userWithSettings,
    expression: '',
    issueSearch: false,
    shiftKeyHold: false,
    views,
    timeRange: -1,
    time: { startTime: 0, stopTime: 0 },
    remoteclusters: { test2: { name: 'Test2', url: 'http://localhost:8124' } },
    esCluster: {
      availableCluster: {
        active: [],
        inactive: []
      },
      selectedCluster: []
    },
    hideViz: false,
    stickyViz: false,
    disabledAggregations: true
  },
  mutations: {
    setAvailableCluster: jest.fn(),
    setSelectedCluster: jest.fn(),
    setFocusSearch: jest.fn(),
    setViews: jest.fn(),
    setTimeRange: jest.fn(),
    setExpression: jest.fn(),
    deleteViews: jest.fn(),
    toggleHideViz: jest.fn(),
    toggleStickyViz: jest.fn(),
    setFetchGraphData: jest.fn(),
    setForcedAggregations: jest.fn()
  }
};

beforeEach(() => {
  UserService.deleteView = jest.fn().mockResolvedValue({
    success: true, text: 'yay!'
  });
  UserService.getState = jest.fn().mockResolvedValue({ data: {} });
  SessionsService.tag = jest.fn().mockResolvedValue({
    data: { text: 'did it!', success: true }
  });
});

test("search bar doesn't have actions button", async () => {
  const $route = {
    query: {},
    name: 'Spiview',
    path: 'http://localhost:8123/arkime/spiview'
  };

  const {
    queryByTitle
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  // actions menu is only available on the sessions page
  expect(queryByTitle('Actions menu')).not.toBeInTheDocument();
});

test('search bar', async () => {
  const $route = {
    query: {},
    name: 'Sessions',
    path: 'http://localhost:8123/arkime/sessions'
  };

  const {
    getByText, getAllByText, getByTitle, getByPlaceholderText
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  getByText('Search'); // component rendered, search button is visible

  // forms display --------------------------------------------------------- //
  await fireEvent.click(getByTitle('Export PCAP'));
  expect(getAllByText('Export PCAP').length).toBe(2);

  await fireEvent.click(getByTitle('Export CSV'));
  expect(getAllByText('Export CSV').length).toBe(2);

  await fireEvent.click(getByTitle('Remove Data'));
  expect(getAllByText('Remove Data').length).toBe(2);

  await fireEvent.click(getByTitle('Send to Test2')); // displays clusters
  getByText('Send Session(s)');

  await fireEvent.click(getByTitle('Export Intersection'));
  expect(getAllByText('Export Intersection').length).toBe(2);

  await fireEvent.click(getByTitle('Add Tags'));
  expect(getAllByText('Add Tags').length).toBe(2);

  await fireEvent.click(getByTitle('Remove Tags'));
  expect(getAllByText('Remove Tags').length).toBe(2);

  // form closes  ---------------------------------------------------------- //
  await fireEvent.update(getByPlaceholderText('Enter a comma separated list of tags'), 'tag1,tag2');
  await fireEvent.click(getAllByText('Remove Tags')[1]);
  await waitFor(() => {
    getByText('did it!'); // displays message from server
    expect(getAllByText('Remove Tags').length).toBe(1);
  });

  // views ----------------------------------------------------------------- //
  await waitFor(() => {
    getByText('test view 1'); // view is displayed
  });

  await fireEvent.click(getByTitle('Create a new view'));
  getByText('Create View'); // view form can be opened

  await fireEvent.click(getByTitle('Delete this view.'));
  expect(UserService.deleteView).toHaveBeenCalledTimes(1); // view can be deleted
});

test('search bar - change view with no view applied', async () => {
  const $route = {
    query: {},
    name: 'Sessions',
    path: 'http://localhost:8123/arkime/sessions'
  };

  const {
    getByText, emitted
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  // setView gets called when there is no view applied
  // there should only be one element with 'text view 1' since this view is not applied
  await fireEvent.click(getByText('test view 1'));
  expect(emitted()).toHaveProperty('setView');
  expect($router.push).toHaveBeenCalledWith({ query: { view: 'test view 1' } });
});

test('search bar - change view when same view applied', async () => {
  const $route = {
    query: { view: 'test view 1' },
    name: 'Sessions',
    path: 'http://localhost:8123/arkime/sessions'
  };

  const {
    getAllByText, emitted
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  // changeSearch gets called when there is the view applied has the same name
  // there should be 2 elements with 'text view 1' since this view not applied
  await fireEvent.click(getAllByText('test view 1')[1]);
  expect(emitted()).toHaveProperty('changeSearch');
  expect($router.push).not.toHaveBeenCalled();
});

test('search bar - change view to different view', async () => {
  const $route = {
    query: { view: 'test view 2' },
    name: 'Sessions',
    path: 'http://localhost:8123/arkime/sessions'
  };

  const {
    getByText, emitted
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  // setview gets called when there is a different view applied
  // there should only be one element with 'text view 1' since this view is not applied
  await fireEvent.click(getByText('test view 1'));
  expect(emitted()).toHaveProperty('setView');
  expect($router.push).toHaveBeenCalledWith({ query: { view: 'test view 1' } });
});

test('search bar - viz buttons', async () => {
  const $route = {
    query: { date: '-1' },
    name: 'Sessions',
    path: 'arkime/sessions'
  };

  const {
    getByText, queryByText
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  // should show viz options buttons
  const vizDropdown = getByText('Fetch Viz Data');

  // clicking button should fetch data and set forced aggs
  await fireEvent.click(vizDropdown);
  expect(store.mutations.setFetchGraphData).toHaveBeenCalledWith(store.state, true);
  expect(store.mutations.setForcedAggregations).toHaveBeenCalledWith(store.state, true);

  // should not have the remove disabled force viz button
  expect(queryByText('Disable forced vizualizations')).not.toBeInTheDocument();

  // should show all fetch viz buttons
  getByText('Fetch vizualizations for this query');
  const alwaysBtn = getByText('Always fetch vizualizations for this browser');
  const sessionBtn = getByText('Fetch vizualizations for this browser session');

  // clicking always button should fetch data and set forced aggs
  await fireEvent.click(alwaysBtn);
  expect(store.mutations.setFetchGraphData).toHaveBeenCalledWith(store.state, true);
  expect(store.mutations.setForcedAggregations).toHaveBeenCalledWith(store.state, true);

  // clicking session button should fetch data and set forced aggs
  await fireEvent.click(sessionBtn);
  expect(store.mutations.setFetchGraphData).toHaveBeenCalledWith(store.state, true);
  expect(store.mutations.setForcedAggregations).toHaveBeenCalledWith(store.state, true);
});

test('search bar - disable forced viz button', async () => {
  const $route = {
    query: { date: '-1' },
    name: 'Sessions',
    path: 'arkime/sessions'
  };

  store.state.disabledAggregations = false; // aggregations are visible
  store.state.forcedAggregations = true; // show the disable forced button

  const {
    getByText, queryByText
  } = render(Search, {
    store,
    mocks: { $route, $router },
    props: { openSessions: [], fields }
  });

  // should show viz options dropdown, but not the button to fetch viz data
  // (since it's already visible because aggregations have been forced)
  getByText('Toggle Dropdown');
  expect(queryByText('Fetch Viz Data')).not.toBeInTheDocument();

  // should see disable force viz button
  const disableForced = getByText('Disable forced vizualizations');
  await fireEvent.click(disableForced);
  expect(store.mutations.setFetchGraphData).not.toHaveBeenCalled();
  expect(store.mutations.setForcedAggregations).toHaveBeenCalledWith(store.state, false);
});
