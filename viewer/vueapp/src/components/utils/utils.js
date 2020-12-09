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
    let numArr = [...Array(num).keys()];
    return numArr.filter((i) => { return num % i === 0; });
  },

  /**
   * Check that at least one ES cluster is selected from ES Cluster Dropdown Menu
   * Also, check that a valid/active cluster is selected
   * @param {string} A string the contains a list of ES cluster in the format ES1,ES2,ES3, ...
   * @param {array} An array of available ES cluster
   * @returns {object} An object of result
   */
  checkESClusterSelection: function (queryESCluster, availableESClusterList) {
    var result = {
      valid: true,
      error: ''
    };

    if (queryESCluster === undefined) {
      return result;
    } else if (queryESCluster === 'none') {
      result.valid = false;
      result.error = 'No ES cluster is selected. Select at least one ES cluster.';
      return result;
    } else if (availableESClusterList.length === 0) {
      // either no active cluster or it is taking time to fetch the available cluster
      return result;
    } else {
      var queryESClusterList = queryESCluster ? queryESCluster.split(',') : [];
      for (var i = 0; i < queryESClusterList.length; i++) {
        if (availableESClusterList.includes(queryESClusterList[i])) { // valid selection
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
