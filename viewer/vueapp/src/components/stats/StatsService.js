'use strict';

import store from '../../store';
import { fetchWrapper } from '@/fetchWrapper.js';

const fetchedCapStartTimes = [];

export default {
  /**
   * Gets the elasticsearch health stats
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  async getESHealth () {
    try {
      const response = await fetchWrapper({ url: 'api/eshealth' });
      store.commit('setESHealthError', undefined);
      store.commit('setESHealth', response);
      return response;
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
  },

  /* STATS --------------------------------------------------------------- */
  /**
   * Gets the capture stats
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getStats (params) {
    return fetchWrapper({ url: 'api/stats', params });
  },

  /**
   * Gets the detailed stats
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getDStats (params) {
    return fetchWrapper({ url: 'api/dstats', params });
  },

  /* ADMIN --------------------------------------------------------------- */
  /**
   * Gets the admin options
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getAdmin (params) {
    return fetchWrapper({ url: 'api/esadmin/get', params });
  },

  /**
   * Sets the admin options
   * @param {Object} options The options for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async setAdmin (options) {
    return fetchWrapper({ ...options, url: 'api/esadmin/set', method: 'POST' });
  },

  /**
   * Clears the data cache
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async clearCacheAdmin (params) {
    return fetchWrapper({ url: 'api/esadmin/clearcache', method: 'POST', params });
  },

  /**
   * Unfloods data nodes
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async unFloodAdmin (params) {
    return fetchWrapper({ url: 'api/esadmin/unflood', method: 'POST', params });
  },

  /**
   * Flushes the data nodes
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async flushAdmin (params) {
    return fetchWrapper({ url: 'api/esadmin/flush', method: 'POST', params });
  },

  /**
   * Reroutes the data nodes
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async rerouteAdmin (params) {
    return fetchWrapper({ url: 'api/esadmin/reroute', method: 'POST', params });
  },

  /* INDICES ------------------------------------------------------------- */
  /**
   * Gets the data indices
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getIndices (params) {
    return fetchWrapper({ url: 'api/esindices', params });
  },

  /**
   * Deletes the specified index
   * @param {string} indexName The name of the index to delete
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async deleteIndex (indexName, params) {
    return fetchWrapper({ url: `api/esindices/${indexName}`, method: 'DELETE', params });
  },

  /**
   * Optimizes the specified index
   * @param {string} indexName The name of the index to optimize
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async optimizeIndex (indexName, params) {
    return fetchWrapper({ url: `api/esindices/${indexName}/optimize`, method: 'POST', params });
  },

  /**
   * Closes the specified index
   * @param {string} indexName The name of the index to close
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async closeIndex (indexName, params) {
    return fetchWrapper({ url: `api/esindices/${indexName}/close`, method: 'POST', params });
  },

  /**
   * Opens the specified index
   * @param {string} indexName The name of the index to open
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async openIndex (indexName, params) {
    return fetchWrapper({ url: `api/esindices/${indexName}/open`, method: 'POST', params });
  },

  /**
   * Shrinks the specified index
   * @param {string} indexName The name of the index to shrink
   * @param {Object} body The body of the request
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async shrinkIndex (indexName, body, params) {
    return fetchWrapper({ url: `api/esindices/${indexName}/shrink`, method: 'POST', body, params });
  },

  /* DATA NODES ---------------------------------------------------------- */
  /**
   * Gets the data nodes
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getDataNodes (params) {
    return fetchWrapper({ url: 'esstats.json', params });
  },

  /**
   * Gets the shard information
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getShards (params) {
    return fetchWrapper({ url: 'api/esshards', params });
  },

  /**
   * Excludes the specified shard
   * @param {string} type The type of the shard (ip or name)
   * @param {string} shardName The name of the shard
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async excludeShard (type, shardName, params) {
    return fetchWrapper({ url: `api/esshards/${type}/${shardName}/exclude`, method: 'POST', params });
  },

  /**
   * Includes the specified shard
   * @param {string} type The type of the shard (ip or name)
   * @param {string} shardName The name of the shard
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async includeShard (type, shardName, params) {
    return fetchWrapper({ url: `api/esshards/${type}/${shardName}/include`, method: 'POST', params });
  },

  /**
   * Deletes the specified shard
   * @param {string} shard The shard to delete
   * @param {string} node The node that the shard belongs to
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async deleteShard (shard, node, params) {
    return fetchWrapper({ url: `api/esshards/${shard}/${node}/delete`, method: 'POST', params });
  },

  /**
   * Gets the data recovery information
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getRecovery (params) {
    return fetchWrapper({ url: 'api/esrecovery', params });
  },

  /**
   * Gets the list of database tasks
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async getTasks (params) {
    return fetchWrapper({ url: 'api/estasks', params });
  },

  /**
   * Cancels the specified database task
   * @param {string} taskId The ID of the task to cancel
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async cancelTask (taskId, params) {
    return fetchWrapper({ url: `api/estasks/${taskId}/cancel`, method: 'POST', params });
  },

  /**
   * Cancels all cancellable database tasks
   * @param {Object} params The parameters for the request
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  async cancelAllTasks (params) {
    return fetchWrapper({ url: 'api/estasks/cancel', method: 'POST', params });
  }
};
