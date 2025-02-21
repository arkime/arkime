'use strict';

import store from '../../store';
import { fetchWrapper } from '@/fetchWrap';

const fetchedCapStartTimes = [];

export default {
  /**
   * Gets the elasticsearch health stats
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getESHealth () {
    try {
      const response = await fetchWrapper('api/eshealth');
      store.commit('setESHealthError', undefined);
      store.commit('setESHealth', response.data);
      return response.data;
    } catch (error) {
      store.commit('setESHealthError', error.text || error);
      throw error;
    }
  },

  /**
   * Gets a list capture start times
   * @param {string} basePath The page that is requesting this data
                              Determine whether it's toggled on/off for this page
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getCapRestartTimes (basePath) {
    const showCapStartTimes = localStorage &&
      localStorage[`${basePath}-cap-times`] &&
      localStorage[`${basePath}-cap-times`] !== 'false';

    // need something for the timeline graph viz to function
    const noCapStartTimes = [{ nodeName: 'none', startTime: 1 }];

    if (!showCapStartTimes) {
      store.commit('setCapStartTimes', noCapStartTimes);
      return noCapStartTimes;
    }

    // check if the cap start times already exist first
    if (fetchedCapStartTimes.length) {
      // we've already fetched the data, just return it
      store.commit('setCapStartTimes', fetchedCapStartTimes);
      return fetchedCapStartTimes;
    }

    try {
      const response = await fetchWrapper({ url: 'api/stats' });
      for (const data of response.data.data) {
        fetchedCapStartTimes.push({
          nodeName: data.nodeName,
          startTime: data.startTime * 1000
        });
      }
      store.commit('setCapStartTimes', fetchedCapStartTimes);
      return fetchedCapStartTimes;
    } catch (error) {
      store.commit('setCapStartTimes', noCapStartTimes);
      return noCapStartTimes;
    }
  }
};
