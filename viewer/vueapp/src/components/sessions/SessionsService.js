import Vue from 'vue';
import qs from 'qs';
import store from '../../store';

let getDecodingsQIP;
let _decodingsCache;

export default {

  /* service methods ------------------------------------------------------- */
  /**
   * Gets a list of sessions from the server
   * @param {object} query        Parameters to query the server
   * @param {object} cancelToken  Token to cancel the request
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  get: function (query, cancelToken) {
    return new Promise((resolve, reject) => {
      let params = { flatten: 1 };
      const sameParams = {
        view: true,
        start: true,
        length: true,
        facets: true,
        bounding: true,
        interval: true,
        cancelId: true,
        expression: true
      };

      if (query) {
        for (let param in sameParams) {
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
        params.order = store.state.sortsParam;

        // server takes one param (fields)
        if (query.fields && query.fields.length) {
          params.fields = '';
          for (let i = 0, len = query.fields.length; i < len; ++i) {
            const item = query.fields[i];
            params.fields += item;
            if (i < len - 1) { params.fields += ','; }
          }
        }
      }

      // set whether map is open on the sessions page
      if (localStorage.getItem('sessions-open-map') === 'true') {
        params.map = true;
      }

      let options = {
        url: 'sessions.json',
        method: 'GET',
        params: params,
        cancelToken: cancelToken
      };

      Vue.axios(options)
        .then((response) => {
          if (response.data.bsqErr) { reject(response.data.bsqErr); }
          resolve(response);
        }, (error) => {
          if (!Vue.axios.isCancel(error)) {
            reject(error);
          }
        });
    });
  },

  /**
   * Gets details about the session
   * @param {string} id         The unique id of the session
   * @param {string} node       The node that the session belongs to
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getDetail: function (id, node) {
    return new Promise((resolve, reject) => {
      Vue.axios.get(`${node}/session/${id}/detail`)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Gets session packets
   * @param {string} id         The unique id of the session
   * @param {string} node       The node that the session belongs to
   * @param {Object} params     The params to send with the request
   * @returns {Object} { promise, source } An object including a promise object
   * that signals the completion or rejection of the request and a source object
   * to allow the request to be cancelled
   */
  getPackets: function (id, node, params) {
    let source = Vue.axios.CancelToken.source();

    let promise = new Promise((resolve, reject) => {
      let options = {
        method: 'GET',
        params: params,
        cancelToken: source.token,
        url: `${node}/session/${id}/packets`
      };

      Vue.axios(options)
        .then((response) => {
          resolve(response.data);
        })
        .catch((error) => {
          if (!Vue.axios.isCancel(error)) {
            reject(error);
          }
        });
    });

    return { promise, source };
  },

  /**
   * Gets other decodings for session pcap data
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  getDecodings: function () {
    if (getDecodingsQIP) { return getDecodingsQIP; }

    getDecodingsQIP = new Promise((resolve, reject) => {
      if (_decodingsCache) { resolve(_decodingsCache); }

      Vue.axios.get('decodings')
        .then((response) => {
          getDecodingsQIP = undefined;
          _decodingsCache = response.data;
          resolve(response.data);
        }, (error) => {
          getDecodingsQIP = undefined;
          reject(error);
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
  tag: function (addTags, params, routeParams) {
    return new Promise((resolve, reject) => {
      let url = addTags ? 'addTags' : 'removeTags';
      let options = this.getReqOptions(url, 'POST', params, routeParams);

      if (options.error) { return reject({text: options.error}); }

      // add sort to params
      options.params.order = store.state.sortsParam;

      // add tags to data instead of url params
      options.data.tags = params.tags;
      delete options.params.tags;

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        }, (error) => {
          reject(error);
        });
    });
  },

  /**
   * Removes PCAP and/or SPI data
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  remove: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      let options = this.getReqOptions('delete', 'POST', params, routeParams);

      if (options.error) { return reject({text: options.error}); }

      // add sort to params
      options.params.order = store.state.sortsParam;

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        })
        .catch((error) => {
          reject(error);
        });
    });
  },

  /**
   * Sends a session to a cluster
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  send: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      let options = this.getReqOptions('sendSessions', 'POST', params, routeParams);

      if (options.error) { return reject({text: options.error}); };

      // add sort to params
      options.params.order = store.state.sortsParam;

      // add tags and cluster to data instead of url params
      options.data.tags = params.tags;
      options.data.cluster = params.cluster;
      delete options.params.tags;
      delete options.params.cluster;

      Vue.axios(options)
        .then((response) => {
          resolve(response);
        })
        .catch((error) => {
          reject(error);
        });
    });
  },

  /**
   * Exports a pcap by setting window.location
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  exportPcap: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      let baseUrl = `sessions.pcap/${params.filename}`;
      // save segments for later because getReqOptions deletes it
      let segments = params.segments;

      delete params.filename; // don't need this anymore

      let options = this.getReqOptions(baseUrl, '', params, routeParams);

      if (options.error) { return reject({text: options.error}); };

      // add missing params
      options.params.segments = segments;
      if (options.data.ids) {
        options.params.ids = options.data.ids;
      }

      // add sort to params
      options.params.order = store.state.sortsParam;

      let url = `${baseUrl}?${qs.stringify(options.params)}`;

      window.location = url;

      return resolve({text: 'PCAP now exporting'});
    });
  },

  /**
   * Exports a csv by setting window.location
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  exportCsv: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      let baseUrl = `sessions.csv/${params.filename}`;
      // save segments for later because getReqOptions deletes it
      let segments = params.segments;

      delete params.filename; // don't need this anymore

      let options = this.getReqOptions(baseUrl, '', params, routeParams);

      if (options.error) { return reject({text: options.error}); };

      // add missing params
      options.params.segments = segments;
      if (options.data.ids) {
        options.params.ids = options.data.ids;
      }

      // add sort to params
      options.params.order = store.state.sortsParam;

      let url = `${baseUrl}?${qs.stringify(options.params)}`;

      window.location = url;

      return resolve({text: 'CSV Exported'});
    });
  },

  /**
   * Open a new page to view unique values for multiple fields
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  viewIntersection: function (params, routeParams) {
    let clonedParams = JSON.parse(JSON.stringify(routeParams));

    params.date = clonedParams.date;
    params.view = clonedParams.view;
    params.stopTime = clonedParams.stopTime;
    params.startTime = clonedParams.startTime;
    params.expression = clonedParams.expression;

    let url = `multiunique.txt?${qs.stringify(params)}`;

    window.open(url, '_blank');
  },

  /**
   * Open a new page to view unique values for different fields
   * @param {string} exp          The field to get unique values for
   * @param {number} counts       1 or 0 whether to include counts of the values
   * @param {object} routeParams  The current url route parameters
   */
  exportUniqueValues: function (exp, counts, routeParams) {
    let clonedParams = JSON.parse(JSON.stringify(routeParams));

    let params = {
      exp: exp,
      counts: counts,
      view: clonedParams.view,
      date: clonedParams.date,
      stopTime: clonedParams.stopTime,
      startTime: clonedParams.startTime,
      expression: clonedParams.expression
    };

    let url = `unique.txt?${qs.stringify(params)}`;

    window.open(url, '_blank');
  },

  /**
   * Open a new page to view spi graph data
   * @param {string} dbField      The field to display spi graph data for
   * @param {object} routeParams  The current url route parameters
   */
  openSpiGraph: function (dbField, routeParams) {
    let clonedParams = JSON.parse(JSON.stringify(routeParams));
    clonedParams.field = dbField;

    let url = `spigraph?${qs.stringify(clonedParams)}`;

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
    let data = { segments: params.segments };

    // merge params and routeParams
    params = Object.assign(params, routeParams);

    if (!params.applyTo || params.applyTo === 'open') {
      // specific sessions that have been opened
      data.ids = [];
      for (let i = 0, len = params.sessions.length; i < len; ++i) {
        data.ids.push(params.sessions[i].id);
      }
      if (data.ids.length > 0) {
        delete params.expression;
      }
      data.ids = data.ids.join(',');
      if (!data.ids) { error = 'There are no matching sessions open.'; }
    } else if (params.applyTo === 'visible') {
      // all sessions visible on the sessions page
      params.length = params.numVisible;
    } else if (params.applyTo === 'matching') {
      // all sessions in query results
      params.start = 0;
      params.length = params.numMatching;
    }

    // remove unnecessary url route params
    delete params.applyTo;
    delete params.segments;
    delete params.sessions;
    delete params.numVisible;
    delete params.numMatching;

    return {
      data: data,
      url: baseUrl,
      error: error,
      method: method,
      params: params
    };
  }

};
