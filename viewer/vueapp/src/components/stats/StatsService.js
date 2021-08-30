'use strict';

import Vue from 'vue';
import store from '../../store';

const fetchedCapStartTimes = [];

export default {
  /**
   * Gets the elasticsearch health stats
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getESHealth: function () {
    return new Promise((resolve, reject) => {
      Vue.axios.get('api/eshealth').then((response) => {
        store.commit('setESHealthError', undefined);
        store.commit('setESHealth', response.data);
        resolve(response.data);
      }).catch((error) => {
        store.commit('setESHealthError', error.text || error);
        reject(error);
      });
    });
  },

  /**
   * Gets a list capture start times
   * @param {string} basePath The page that is requesting this data
                              Determine whether it's toggled on/off for this page
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getCapRestartTimes (basePath) {
    return new Promise((resolve, reject) => {
      const showCapStartTimes = localStorage &&
        localStorage[`${basePath}-cap-times`] &&
        localStorage[`${basePath}-cap-times`] !== 'false';

      // need something for the timeline graph viz to function
      const noCapStartTimes = [{ nodeName: 'none', startTime: 1 }];

      if (!showCapStartTimes) {
        store.commit('setCapStartTimes', noCapStartTimes);
        return resolve(noCapStartTimes);
      }

      // check if the cap start times already exist first
      if (fetchedCapStartTimes.length) {
        // we've already fetched the data, just return it
        store.commit('setCapStartTimes', fetchedCapStartTimes);
        return resolve(fetchedCapStartTimes);
      }

      Vue.axios.get('api/stats').then((response) => {
        for (const data of response.data.data) {
          fetchedCapStartTimes.push({
            nodeName: data.nodeName,
            startTime: data.startTime * 1000
          });
        }
        store.commit('setCapStartTimes', fetchedCapStartTimes);
        return resolve(fetchedCapStartTimes);
      }).catch((error) => { // don't reject just send no times
        store.commit('setCapStartTimes', noCapStartTimes);
        return resolve(noCapStartTimes);
      });
    });
  }
};
