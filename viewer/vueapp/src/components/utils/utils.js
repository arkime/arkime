import uuid from 'uuidv4';

export default {

  /**
   * Creates a unique random string
   * @returns {string} The unique random string
   */
  createRandomString: function () {
    return uuid();
  },

  /** @returns the default sessions table state if none is defined by the user */
  getDefaultTableState: function () {
    return {
      order: [['firstPacket', 'desc']],
      visibleHeaders: [
        'firstPacket',
        'lastPacket',
        'src',
        'srcPort',
        'dst',
        'dstPort',
        'totPackets',
        'dbby',
        'node',
        'info'
      ]
    };
  },

  /**
   * Finds all of the factors of a given number
   * @param {int} An integer to find factors for
   * @returns {array} An array of factors
   */
  findFactors: function (num) {
    const numArr = [...Array(num).keys()];
    return numArr.filter((i) => { return num % i === 0; });
  },

  /**
   * Check that at least one ES cluster is selected from ES Cluster Dropdown Menu
   * Also, check that a valid/active cluster is selected
   * @param {string} A string the contains a list of ES cluster in the format ES1,ES2,ES3, ...
   * @param {array} An array of available ES cluster
   * @returns {object} An object of result
   */
  checkClusterSelection: function (queryCluster, availableClusterList) {
    const result = {
      valid: true,
      error: ''
    };

    if (queryCluster === undefined) {
      return result;
    } else if (queryCluster === 'none') {
      result.valid = false;
      result.error = 'No ES cluster is selected. Select at least one ES cluster.';
      return result;
    } else if (availableClusterList.length === 0) {
      // either no active cluster or it is taking time to fetch the available cluster
      return result;
    } else {
      const queryClusterList = queryCluster ? queryCluster.split(',') : [];
      for (let i = 0; i < queryClusterList.length; i++) {
        if (availableClusterList.includes(queryClusterList[i])) { // valid selection
          result.valid = true;
          result.error = '';
          return result;
        }
      }
      // invalid selection
      result.valid = false;
      result.error = 'Invalid ES cluster is selected';
      return result;
    }
  }
};
