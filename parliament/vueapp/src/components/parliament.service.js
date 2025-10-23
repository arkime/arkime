import { fetchWrapper } from '@common/fetchWrapper.js';

export default {
  getParliament: async function () {
    return await fetchWrapper({ url: 'api/parliament' });
  },

  getStats: async function () {
    return await fetchWrapper({ url: 'api/parliament/stats' });
  },

  createGroup: async function (newGroup) {
    return await fetchWrapper({ url: 'api/groups', method: 'POST', data: newGroup });
  },

  editGroup: async function (groupId, updatedGroup) {
    return await fetchWrapper({ url: `api/groups/${groupId}`, method: 'PUT', data: updatedGroup });
  },

  deleteGroup: async function (groupId) {
    return await fetchWrapper({ url: `api/groups/${groupId}`, method: 'DELETE' });
  },

  createCluster: async function (groupId, newCluster) {
    return await fetchWrapper({ url: `api/groups/${groupId}/clusters`, method: 'POST', data: newCluster });
  },

  editCluster: async function (groupId, clusterId, updatedCluster) {
    return await fetchWrapper({ url: `api/groups/${groupId}/clusters/${clusterId}`, method: 'PUT', data: updatedCluster });
  },

  deleteCluster: async function (groupId, clusterId) {
    return await fetchWrapper({ url: `api/groups/${groupId}/clusters/${clusterId}`, method: 'DELETE' });
  },

  updateOrder: async function (order) {
    return await fetchWrapper({ url: 'api/parliament/order', method: 'PUT', data: order });
  },

  getIssues: async function (query) {
    return await fetchWrapper({ url: 'api/issues', params: query });
  },

  acknowledgeIssues: async function (issues) {
    return await fetchWrapper({ url: 'api/acknowledgeIssues', method: 'PUT', data: { issues } });
  },

  removeIssue: async function (groupId, clusterId, issue) {
    return await fetchWrapper({ url: `api/groups/${groupId}/clusters/${clusterId}/removeIssue`, method: 'PUT', data: { type: issue.type, node: issue.node } });
  },

  removeAllAcknowledgedIssues: async function () {
    return await fetchWrapper({ url: 'api/issues/removeAllAcknowledgedIssues', method: 'PUT', data: {} });
  },

  removeSelectedAcknowledgedIssues: async function (issues) {
    return await fetchWrapper({ url: 'api/removeSelectedAcknowledgedIssues', method: 'PUT', data: { issues } });
  },

  ignoreIssues: async function (issues, forMs) {
    return await fetchWrapper({ url: 'api/ignoreIssues', method: 'PUT', data: { ms: forMs, issues } });
  },

  removeIgnoreIssues: async function (issues) {
    return await fetchWrapper({ url: 'api/removeIgnoreIssues', method: 'PUT', data: { issues } });
  }
};
