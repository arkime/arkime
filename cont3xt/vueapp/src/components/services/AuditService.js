import setReqHeaders from '../../../../../common/vueapp/setReqHeaders';
import { paramStr } from '../../utils/paramStr';

export default {
  /**
   * Fetches the list of audit log entries that a user can view.
   * @param {Object} query of shape { searchTerm, startMs, stopMs }
   * @returns {Promise} - The promise that either resolves to an `Audit[]` or rejects in error
   */
  getAudits (query) {
    return new Promise((resolve, reject) => {
      fetch(`api/audits/${paramStr(query)}`).then((response) => {
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
