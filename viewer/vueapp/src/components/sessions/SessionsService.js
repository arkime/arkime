import qs from 'qs';
import store from '../../store';
import Utils from '../utils/utils';
import { fetchWrapper, cancelFetchWrapper } from '@common/fetchWrapper.js';

let getDecodingsQIP;
let _decodingsCache;

export default {

  /* internal helper methods ----------------------------------------------- */
  /**
   * Builds base parameters from query for session-related requests
   * @param {object} query Parameters from the query/route
   * @param {object} options Configuration options for parameter building
   * @param {boolean} options.includePagination Include start/length parameters
   * @param {boolean} options.includeSort Include sort order parameter
   * @param {boolean} options.includeFields Include fields parameter
   * @param {boolean} options.flatten Include flatten parameter
   * @returns {object} Built parameters object
   */
  buildSessionParams: function (query, options = {}) {
    const {
      includePagination = true,
      includeSort = true,
      includeFields = true,
      flatten = true
    } = options;

    const params = {};
    if (flatten) {
      params.flatten = 1;
    }

    const sameParams = {
      view: true,
      facets: true,
      bounding: true,
      interval: true,
      spanning: true,
      cancelId: true,
      expression: true,
      cluster: true,
      map: true
    };

    if (includePagination) {
      sameParams.start = true;
      sameParams.length = true;
    }

    if (query) {
      for (const param in sameParams) {
        if (query[param]) { params[param] = query[param]; }
      }

      // always send stopTime and startTime unless date is all time (-1)
      if (parseInt(query.date, 10) === -1) {
        params.date = query.date;
      } else {
        params.startTime = query.startTime;
        params.stopTime = query.stopTime;
      }

      // add sort to params
      if (includeSort) {
        params.order = store.state.sortsParam;
      }

      // server takes one param (fields)
      if (includeFields && query.fields && query.fields.length) {
        params.fields = '';
        for (let i = 0, len = query.fields.length; i < len; ++i) {
          const item = query.fields[i];
          params.fields += item;
          if (i < len - 1) { params.fields += ','; }
        }
      }
    }

    return params;
  },

  /* service methods ------------------------------------------------------- */
  /**
   * Gets a list of sessions from the server
   * @param {object} query        Parameters to query the server
   * @param {boolean} calculateFacets Whether to calculate facets (true) or not (false)
   * @returns {AbortController} The AbortController used to cancel the request.
   * @returns {Promise<Object>} The response data parsed as JSON.
   */
  get: function (query, calculateFacets = true) {
    const params = this.buildSessionParams(query, {
      includePagination: true,
      includeSort: true,
      includeFields: true,
      flatten: true
    });

    if (calculateFacets) {
      Utils.setFacetsQuery(params, 'sessions');
      Utils.setMapQuery(params);
    }

    return cancelFetchWrapper({
      url: 'api/sessions',
      method: 'POST',
      data: params
    });
  },

  /**
   * Gets details about the session
   * @param {string} id         The unique id of the session
   * @param {string} node       The node that the session belongs to
   * @param {string} cluster  The Elasticsearch cluster that the session belongs to
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getDetail: async function (id, node, cluster) {
    return await fetchWrapper({
      url: `api/session/${node}/${id}/detail`,
      params: { cluster },
      headers: { 'Content-Type': 'text/html' }
    });
  },

  /**
   * Gets session packets
   * @param {string} id       The unique id of the session
   * @param {string} node     The node that the session belongs to
   * @param {string} cluster  The Elasticsearch cluster that the session belongs to
   * @param {Object} params   The params to send with the request
   * @returns {Object} { promise, source } An object including a promise object
   * that signals the completion or rejection of the request and a source object
   * to allow the request to be cancelled
   */
  getPackets (id, node, cluster, params) {
    const options = {
      params: { ...params, cluster },
      url: `api/session/${node}/${id}/packets`,
      headers: { 'Content-Type': 'text/html' }
    };

    return cancelFetchWrapper(options);
  },

  /**
   * Gets other decodings for session pcap data
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getDecodings: function () {
    if (getDecodingsQIP) { return getDecodingsQIP; }

    getDecodingsQIP = new Promise((resolve, reject) => {
      if (_decodingsCache) { return resolve(_decodingsCache); }

      fetchWrapper({ url: 'api/sessions/decodings' }).then((response) => {
        getDecodingsQIP = undefined;
        _decodingsCache = response;
        return resolve(response);
      }).catch((error) => {
        getDecodingsQIP = undefined;
        return reject(error);
      });
    });

    return getDecodingsQIP;
  },

  /**
   * Adds or Removes tags from sessions
   * @param {Boolean} addTags     Whether to add tags (otherwise remove)
   * @param {object} params       The parameters to be added to the url
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  tag: async function (addTags, params, routeParams) {
    let url = 'api/sessions';
    addTags ? url += '/addtags' : url += '/removetags';

    const { options, error } = this.getReqOptions(url, 'POST', params, routeParams);

    if (error) { return { text: error }; }

    // add sort to params
    options.params.order = store.state.sortsParam;

    // add tags to data instead of url params
    options.data.tags = params.tags;
    delete options.params.tags;

    return await fetchWrapper(options);
  },

  /**
   * Removes PCAP and/or SPI data
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  remove: async function (params, routeParams) {
    const { options, error } = this.getReqOptions('api/delete', 'POST', params, routeParams);

    if (error) { return { text: error }; }

    // add sort to params
    options.params.order = store.state.sortsParam;

    return await fetchWrapper(options);
  },

  /**
   * Sends a session to a cluster
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  send: async function (params, routeParams) {
    const cluster = params.cluster;
    const { options, error } = this.getReqOptions('api/sessions/send', 'POST', params, routeParams);

    if (error) { return { text: error }; }

    // add sort to params
    options.params.order = store.state.sortsParam;

    // add tags and cluster to data instead of url params
    options.data.tags = params.tags;
    options.data.remoteCluster = cluster; // use the cluster before routeParams replaces it
    delete options.params.tags;
    delete options.params.cluster;

    return await fetchWrapper(options);
  },

  /**
   * Exports a pcap by setting window.location
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  exportPcap: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      const filename = params.filename || 'sessions.pcap';
      delete params.filename; // don't need this anymore

      const baseUrl = `api/sessions/pcap/${filename}`;
      // save segments for later because getReqOptions deletes it
      const segments = params.segments;

      const { options, error } = this.getReqOptions(baseUrl, '', params, routeParams);

      if (error) { return reject({ text: error }); };

      // add missing params
      options.params.segments = segments;
      if (options.data.ids) {
        options.params.ids = options.data.ids;
      }

      // add sort to params
      options.params.order = store.state.sortsParam;

      const url = `${baseUrl}?${qs.stringify(options.params)}`;

      // use a link so any errors do not redirect to a broken page
      const link = document.createElement('a');
      link.href = url;
      link.download = filename;
      link.click();
      link.remove();

      return resolve({ success: true });
    });
  },

  /**
   * Exports a csv by setting window.location
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  exportCsv: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      const baseUrl = `api/sessions/csv/${params.filename}`;
      // save segments for later because getReqOptions deletes it
      const segments = params.segments;

      delete params.filename; // don't need this anymore

      const { options, error } = this.getReqOptions(baseUrl, '', params, routeParams);

      if (error) { return reject({ text: error }); };

      // add missing params
      options.params.segments = segments;
      if (options.data.ids) {
        options.params.ids = options.data.ids;
      }

      // add sort to params
      options.params.order = store.state.sortsParam;

      const url = `${baseUrl}?${qs.stringify(options.params)}`;

      window.location = url;

      return resolve({ success: true });
    });
  },

  /**
   * Open a new page to view unique values for multiple fields
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  viewIntersection: function (params, routeParams) {
    const clonedParams = JSON.parse(JSON.stringify(routeParams));

    params.date = clonedParams.date;
    params.view = clonedParams.view;
    params.stopTime = clonedParams.stopTime;
    params.startTime = clonedParams.startTime;
    params.expression = clonedParams.expression;
    params.cluster = clonedParams.cluster;

    const url = `api/multiunique?${qs.stringify(params)}`;

    window.open(url, '_blank');
  },

  /**
   * Open a new page to view unique values for different fields
   * @param {string} exp          The field to get unique values for
   * @param {number} counts       1 or 0 whether to include counts of the values
   * @param {object} routeParams  The current url route parameters
   */
  exportUniqueValues: function (exp, counts, routeParams) {
    const clonedParams = JSON.parse(JSON.stringify(routeParams));

    const params = {
      exp,
      counts,
      view: clonedParams.view,
      date: clonedParams.date,
      stopTime: clonedParams.stopTime,
      startTime: clonedParams.startTime,
      expression: clonedParams.expression,
      cluster: clonedParams.cluster
    };

    const url = `api/unique?${qs.stringify(params)}`;

    window.open(url, '_blank');
  },

  /**
   * Open a new page to view spi graph data
   * @param {string} dbField      The field to display spi graph data for
   * @param {object} routeParams  The current url route parameters
   */
  openSpiGraph: function (dbField, routeParams) {
    const clonedParams = JSON.parse(JSON.stringify(routeParams));
    clonedParams.field = dbField;

    const url = `spigraph?${qs.stringify(clonedParams)}`;

    window.open(url, '_blank');
  },

  /* internal methods ------------------------------------------------------ */
  /**
   * Get Request Options
   * @param {string} baseUrl      The base url to append params to
   * @param {string} method       The HTTP method (POST, GET, PUT, DELETE, etc)
   * @param {object} params       The parameters to be applied to url or data
   * @param {object} routeParams  The current url route parameters
   * @returns { url: {string}, method: {string}, data: {object} }
   */
  getReqOptions: function (baseUrl, method, params, routeParams) {
    let error; // keep track of any errors
    // add segments to data instead of url parameters
    const data = { segments: params.segments };

    // merge params and routeParams
    const combinedParams = { // last object property overwrites the previous one
      ...params,
      ...routeParams
    };

    // fallback to store time values if not in route query (can happen when navigating directly to /sessions)
    if (!combinedParams.date && !combinedParams.startTime && !combinedParams.stopTime) {
      if (parseInt(store.state.timeRange, 10) === -1) {
        combinedParams.date = store.state.timeRange;
      } else {
        combinedParams.startTime = store.state.time.startTime;
        combinedParams.stopTime = store.state.time.stopTime;
      }
    }

    if (!combinedParams.applyTo || combinedParams.applyTo === 'open') {
      // specific sessions that have been opened
      data.ids = [];
      for (let i = 0, len = combinedParams.sessions.length; i < len; ++i) {
        data.ids.push(combinedParams.sessions[i].id);
      }
      if (data.ids.length > 0) {
        delete combinedParams.expression;
      }
      data.ids = data.ids.join(',');
      if (!data.ids) { error = 'There are no matching sessions open.'; }
    } else if (combinedParams.applyTo === 'visible') {
      // all sessions visible on the sessions page
      combinedParams.start = params.start;
      combinedParams.length = combinedParams.numVisible;
    } else if (combinedParams.applyTo === 'matching') {
      // all sessions in query results
      combinedParams.start = 0;
      combinedParams.length = combinedParams.numMatching;
    }

    // remove unnecessary url route params
    delete combinedParams.applyTo;
    delete combinedParams.segments;
    delete combinedParams.sessions;
    delete combinedParams.numVisible;
    delete combinedParams.numMatching;

    return {
      error,
      options: {
        data,
        method,
        url: baseUrl,
        params: combinedParams
      }
    };
  }

};
