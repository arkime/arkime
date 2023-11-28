'use strict';

import Vue from 'vue';
import BootstrapVue from 'bootstrap-vue';
import { render } from '@testing-library/vue';
import LinkGroupCard from '../src/components/links/LinkGroupCard.vue';
import '../src/utils/filters';

Vue.use(BootstrapVue);

const linkGroup = {
  name: 'test link group',
  links: [{
    name: 'test IP',
    /* eslint-disable no-template-curly-in-string */
    url: 'http://test.com?q=${indicator}&ips=${array,{"iType":"ip"}}',
    itypes: ['ip', 'domain', 'email']
  }],
  viewRoles: [],
  editRoles: [],
  creator: 'admin',
  _id: 'asdfghjkl',
  _editable: false,
  _viewable: true
};

const store = {
  state: {
    collapsedLinkGroups: {},
    indicatorGraph: {
      '10.0.0.1-ip': {
        indicator: {
          query: '10.0.0.1',
          itype: 'ip'
        },
        parentIds: new Set(),
        children: [],
        enhanceInfo: {}
      },
      'test.com-domain': {
        indicator: {
          query: 'test.com',
          itype: 'domain'
        },
        parentIds: new Set(),
        children: [
          {
            indicator: {
              query: '10.0.0.3',
              itype: 'ip'
            },
            parentIds: new Set(),
            children: [],
            enhanceInfo: {
              ttl: 3490
            }
          }
        ],
        enhanceInfo: {}
      },
      '10.0.0.2-ip': {
        indicator: {
          query: '10.0.0.2',
          itype: 'ip'
        },
        parentIds: new Set(),
        children: [],
        enhanceInfo: {}
      },
      '10.0.0.3-ip': {
        indicator: {
          query: '10.0.0.3',
          itype: 'ip'
        },
        parentIds: new Set('test.com-domain'),
        children: [],
        enhanceInfo: {
          ttl: 3490
        }
      }
    }
  },
  getters: {
    getLinkGroups (state) {
      return [linkGroup];
    },
    getActiveIndicator (state) {
      return {
        itype: 'ip',
        query: '10.0.0.1'
      };
    },
    getRoles (state) {
      return [];
    },
    getCheckedLinks (state) {
      return {};
    },
    getUser (state) {
      return {
        enabled: true,
        settings: {},
        userId: 'admin',
        userName: 'admin',
        webEnabled: true,
        roles: [
          'superAdmin',
          'usersAdmin',
          'arkimeAdmin',
          'arkimeUser',
          'parliamentAdmin',
          'parliamentUser',
          'wiseAdmin',
          'wiseUser',
          'cont3xtAdmin',
          'cont3xtUser'
        ],
        assignableRoles: [],
        canAssignRoles: false
      };
    }
  }
};

test('Link Group Card Display', async () => {
  const {
    getByText
  } = render(LinkGroupCard, {
    store,
    props: {
      linkGroup,
      indicator: {
        itype: 'ip',
        query: '10.0.0.1'
      },
      startDate: '2023-08-17T19:02:54Z',
      stopDate: '2023-08-24T19:02:54Z'
    }
  });

  // displays link group
  getByText('test link group');

  // displays link
  const link = getByText('test IP');

  // replaces the ${indicator} placeholder with active indicator AND
  // parses and replaces ${array,{...}} placeholder using defaults
  expect(link.href).toBe('http://test.com/?q=10.0.0.1&ips=10.0.0.1,10.0.0.2,10.0.0.3');
});

test('Link Group Card Display 2', async () => {
  linkGroup.links[0].url = 'http://test.com?q=${array,{"iType":"domain"}}&ips=${array,{"iType":"ip","include":"top","sep":"|","quote":"\'"}}';

  const {
    getByText
  } = render(LinkGroupCard, {
    store,
    props: {
      linkGroup,
      indicator: {
        itype: 'ip',
        query: '10.0.0.1'
      },
      startDate: '2023-08-17T19:02:54Z',
      stopDate: '2023-08-24T19:02:54Z'
    }
  });

  // parses and replaces multiple ${array,{...}} placeholder using options
  expect(getByText('test IP').href).toBe('http://test.com/?q=test.com&ips=%2710.0.0.1%27|%2710.0.0.2%27');
});
