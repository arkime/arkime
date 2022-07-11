import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';

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
   * @param {Object} dateRange - An object holding the requested time range, of shape: { start, end }
   * @param {Object} audit - The audit data
   * @returns {Promise} - The promise that either resolves to an `Audit[]` or rejects in error
   */
  getAudits (dateRange) {
    return new Promise((resolve, reject) => {
      fetch(`api/audits/${JSON.stringify(dateRange)}`, {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
      }).then((response) => {
        if (!response.ok) { // test for bad response code
          throw new Error(response.statusText);
        }
        return response.json();
      }).then((response) => {
        return resolve(response.audits);
      }).catch((err) => { // this catches an issue within the ^ .then
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
