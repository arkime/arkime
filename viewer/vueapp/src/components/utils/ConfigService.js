import Vue from 'vue';

let _molochClustersCache;
let queryInProgress;

export default {

  getMolochClusters () {
    if (queryInProgress) { return queryInProgress; }

    queryInProgress = new Promise((resolve, reject) => {
      if (_molochClustersCache) { resolve(_molochClustersCache); }

      Vue.axios.get('molochclusters')
        .then((response) => {
          queryInProgress = undefined;
          _molochClustersCache = response.data;
          resolve(response.data);
        }, (error) => {
          queryInProgress = undefined;
          reject(error);
        });
    });

    return queryInProgress;
  }

};
