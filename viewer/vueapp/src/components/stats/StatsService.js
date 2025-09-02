'use strict';

import store from '../../store';
import { fetchWrapper } from '@common/fetchWrapper.js';

const fetchedCapStartTimes = [];
// Helper to cache promises for script loading, preventing multiple fetches for the same URL.
const scriptLoadingPromises = {};

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
    return fetchWrapper({ url: 'api/esadmin', params });
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
  },

  /**
   * Loads a script if it's not already loaded and validated on the window object.
   * Executes the script with 'this' context as 'window'.
   * @param {string} libraryGlobalName - The expected global variable name (e.g., 'd3', 'cubism', 'hljs').
   * @param {function} validationFn - A function that takes window[libraryGlobalName] and returns true if valid.
   * @param {string} scriptUrl - The URL from which to fetch the script.
   * @returns {Promise<object>} A promise that resolves with the library object from window.
   * @throws {Error} If the script fails to load or validate.
   */
  async loadScriptOnce (libraryGlobalName, validationFn, scriptUrl) {
    // Check if library already exists on the window and is valid
    if (validationFn(window[libraryGlobalName])) {
      // console.log(`${libraryGlobalName} already loaded and validated.`);
      return window[libraryGlobalName];
    }

    // If not, check if a promise for this script URL already exists (to avoid redundant fetches)
    if (scriptLoadingPromises[scriptUrl]) {
      return scriptLoadingPromises[scriptUrl];
    }

    // Create a new promise to load the script
    const promise = (async () => {
      try {
        const response = await fetch(scriptUrl); // Assuming scriptUrl is a correctly resolved path
        if (!response.ok) {
          throw new Error(`Failed to fetch ${libraryGlobalName} script (HTTP ${response.status}) from ${scriptUrl}: ${response.statusText}`);
        }
        const scriptContent = await response.text();

        // Execute script with 'this' as window. This is crucial for legacy IIFE scripts like D3v3.
        new Function(scriptContent).call(window);

        // Validate that the library is now correctly initialized on the window object
        if (!validationFn(window[libraryGlobalName])) {
          throw new Error(`${libraryGlobalName} script from ${scriptUrl} executed but failed validation (e.g., not on window, wrong version, or missing key functions).`);
        }

        return window[libraryGlobalName];
      } catch (error) {
        delete scriptLoadingPromises[scriptUrl]; // Remove promise from cache on error to allow potential retry
        throw error; // Re-throw error to be caught by the caller
      }
    })();

    scriptLoadingPromises[scriptUrl] = promise; // Cache the promise
    return promise;
  },

  /**
   * Initializes and loads the time series libraries (D3, Cubism.js v1, Highlight.js).
   * This method ensures that the libraries are loaded only once and are validated.
   * @returns {Promise<Object>} A promise that resolves with an object containing the loaded libraries.
   * @throws {Error} If any of the libraries fail to load or validate.
   */
  async loadTimeSeriesLibraries () {
    try {
      // Define validation functions for each library
      const isD3V3Loaded = (lib) => lib && lib.version === '3.5.5';
      // For Cubism.js v1, check for a key function. `cubism.context` is fundamental.
      const isCubismV1Loaded = (lib) => lib && typeof lib.context === 'function';
      // For Highlight.js, we don't really care as most of the visualization doesn't need it
      const isHighlightJSLoaded = () => true;
      // paths to the libraries
      const d3Path = 'public/d3.min.js';
      const cubismPath = 'public/cubism.v1.min.js';
      const highlightPath = 'public/highlight.min.js';

      // Sequentially load libraries using the helper
      const loadedD3 = await this.loadScriptOnce('d3', isD3V3Loaded, d3Path);
      const loadedCubism = await this.loadScriptOnce('cubism', isCubismV1Loaded, cubismPath);
      // Highlight.js is used by cubism, so we don't need to return it as we don't use it directly.
      await this.loadScriptOnce('hljs', isHighlightJSLoaded, highlightPath);

      return { d3: loadedD3, cubism: loadedCubism };
    } catch (error) {
      console.error('Error initializing time series libraries:', error);
      throw error;
    }
  }
};
