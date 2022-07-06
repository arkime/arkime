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
   * @returns {Promise} - The promise that either resolves to an `Audit[]` or rejects in error
   */
  getAudits () {
    return new Promise((resolve, reject) => {
      fetch('api/audits').then((response) => {
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
  }
};
