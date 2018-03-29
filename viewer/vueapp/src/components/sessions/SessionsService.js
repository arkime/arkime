import Vue from 'vue';
import qs from 'qs';

export default {

  /* service methods ------------------------------------------------------- */
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
   * Deletes a session
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  remove: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      let options = this.getReqOptions('delete', 'POST', params, routeParams);

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
   * Scrubs pcap data in session
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   * @returns {Promise} Promise   A promise object that signals the completion
   *                              or rejection of the request.
   */
  scrub: function (params, routeParams) {
    return new Promise((resolve, reject) => {
      let options = this.getReqOptions('scrub', 'POST', params, routeParams);

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
    let baseUrl = `sessions.pcap/${params.filename}`;
    // save segments for later because getReqOptions deletes it
    let segments = params.segments;

    delete params.filename; // don't need this anymore

    let options = this.getReqOptions(baseUrl, '', params, routeParams);

    // add missing params
    options.params.segments = segments;
    if (options.data.ids) {
      options.params.ids = options.data.ids;
    }

    let url = `${baseUrl}?${qs.stringify(options.params)}`;

    window.location = url;
  },

  /**
   * Exports a csv by setting window.location
   * @param {object} params       The parameters to be passed to server
   * @param {object} routeParams  The current url route parameters
   */
  exportCsv: function (params, routeParams) {
    let baseUrl = `sessions.csv/${params.filename}`;
    // save segments for later because getReqOptions deletes it
    let segments = params.segments;

    delete params.filename; // don't need this anymore

    let options = this.getReqOptions(baseUrl, '', params, routeParams);

    // add missing params
    options.params.segments = segments;
    if (options.data.ids) {
      options.params.ids = options.data.ids;
    }

    // TODO test passing in fields (params.fields => options.params.fields)
    let url = `${baseUrl}?${qs.stringify(options.params)}`;

    window.location = url;
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
      data.ids = data.ids.join(',');
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
      method: method,
      params: params
    };
  }

};
