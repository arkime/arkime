import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';
import Vue from 'vue';

export default {
  /**
   * Creates an audit log entry.
   * @param {Object} audit - The audit data
   * @returns {Promise} - The promise that either resolves or rejects in error
   */
  createAudit (audit) {
    return new Promise((resolve, reject) => {
      fetch('api/audit', {
        method: 'PUT',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(audit)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  },

  /**
   * Fetches the list of audit log entries that a user can view.
   * @param {Object} query of shape { searchTerm, startMs, stopMs }
   * @returns {Promise} - The promise that either resolves to an `Audit[]` or rejects in error
   */
  getAudits (query) {
    return new Promise((resolve, reject) => {
      const options = {
        method: 'GET',
        params: query,
        url: 'api/audits'
      };
      Vue.axios(options).then((response) => {
        if (response.status !== 200) { // test for bad response code
          throw new Error(response.statusText);
        }
        return resolve(response.data.audits);
      }).catch((err) => {
        return reject(err);
      });
    });
  },

  /**
   * Deletes a history log
   * @param {String} id - The id of the view to delete
   * @returns {Promise} - The promise that either resolves the request or rejects in error
   */
  deleteAudit (id) {
    return new Promise((resolve, reject) => {
      fetch(`api/audit/${id}`, {
        method: 'DELETE',
        headers: setReqHeaders({ 'Content-Type': 'application/json' })
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  }
};
