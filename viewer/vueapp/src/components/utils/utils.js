import uuid from 'uuid';

import store from '../../store';

export default {

  /**
   * Creates a unique random string
   * @returns {string} The unique random string
   */
  createRandomString: function () {
    return uuid.v4();
  },

  /** @returns the default sessions table state if none is defined by the user */
  getDefaultTableState: function () {
    return {
      order: [['firstPacket', 'desc']],
      visibleHeaders: [
        'firstPacket',
        'lastPacket',
        'src',
        'source.port',
        'dst',
        'destination.port',
        'network.packets',
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
  },

  /**
   * Sets the facets query based on the query time range and whether the user wants to force aggregations
   * NOTE: mutates the query object and sets the store values
   * @param {object} query The query parameters for the search to be passed to the server
   * @param {string} page The page the query is for
   */
  setFacetsQuery (query, page) {
    if (!page || (page !== 'sessions' && page !== 'spiview')) {
      store.commit('setDisabledAggregations', false);
      query.facets = 1;
      return;
    }

    if (
      (localStorage['force-aggregations'] && localStorage['force-aggregations'] !== 'false') ||
      (sessionStorage['force-aggregations'] && sessionStorage['force-aggregations'] !== 'false')
    ) {
      store.commit('setDisabledAggregations', false);
      store.commit('setForcedAggregations', true);
      query.facets = 1;
      return;
    }

    if (query.date === '-1') {
      store.commit('setDisabledAggregations', true);
      query.facets = 0;
      return;
    } else if (query.stopTime && query.startTime) {
      store.commit('setDisabledAggregations', true);
      const deltaTime = (query.stopTime - query.startTime) / 86400; // secs to days
      /* eslint-disable no-undef */
      if (deltaTime >= (TURN_OFF_GRAPH_DAYS || 30)) {
        query.facets = 0;
        return;
      }
    }

    store.commit('setDisabledAggregations', false);
  }
};
