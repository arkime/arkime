import Vue from 'vue';

export default {
  getParliament: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/parliament')
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  createGroup: function (newGroup) {
    return new Promise((resolve, reject) => {
      Vue.axios.post('api/groups', newGroup)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  editGroup: function (groupId, updatedGroup) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}`, updatedGroup)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  deleteGroup: function (groupId) {
    return new Promise((resolve, reject) => {
      Vue.axios.delete(`api/groups/${groupId}`)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  createCluster: function (groupId, newCluster) {
    return new Promise((resolve, reject) => {
      Vue.axios.post(`api/groups/${groupId}/clusters`, newCluster)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  editCluster: function (groupId, clusterId, updatedCluster) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}/clusters/${clusterId}`, updatedCluster)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  deleteCluster: function (groupId, clusterId) {
    return new Promise((resolve, reject) => {
      Vue.axios.delete(`api/groups/${groupId}/clusters/${clusterId}`)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  updateParliamentOrder: function (reorderedParliament) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/parliament`, { reorderedParliament: reorderedParliament })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  getIssues: function (query) {
    return new Promise((resolve, reject) => {
      Vue.axios.get(`api/issues`, { params: query })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  dismissIssue: function (groupId, clusterId, issue) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}/clusters/${clusterId}/dismissIssue`, {
        type: issue.type,
        node: issue.node
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  ignoreIssue: function (groupId, clusterId, issue, forMs) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}/clusters/${clusterId}/ignoreIssue`, {
        ms: forMs,
        type: issue.type,
        node: issue.node
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  removeIgnoreIssue: function (groupId, clusterId, issue) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}/clusters/${clusterId}/removeIgnoreIssue`, {
        type: issue.type,
        node: issue.node
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  dismissAllIssues: function (groupId, clusterId) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}/clusters/${clusterId}/dismissAllIssues`, {})
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  }
};
