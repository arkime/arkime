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

  acknowledgeIssues: function (issues) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/acknowledgeIssues`, {
        issues: issues
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  removeIssue: function (groupId, clusterId, issue) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/groups/${groupId}/clusters/${clusterId}/removeIssue`, {
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

  removeAllAcknowledgedIssues: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/issues/removeAllAcknowledgedIssues`, {})
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  removeSelectedAcknowledgedIssues: function (issues) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/removeSelectedAcknowledgedIssues`, {
        issues: issues
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  ignoreIssues: function (issues, forMs) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/ignoreIssues`, {
        ms: forMs,
        issues: issues
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  },

  removeIgnoreIssues: function (issues) {
    return new Promise((resolve, reject) => {
      Vue.axios.put(`api/removeIgnoreIssues`, {
        issues: issues
      })
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          reject(error.response.data);
        });
    });
  }
};
