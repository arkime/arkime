import store from '@/store';
import setReqHeaders from '@real_common/setReqHeaders';

// TODO VUE3 - provide cancelToken for ES/OS (options.cancelToken)
/**
 * A wrapper function for making HTTP requests using the Fetch API.
 *
 * @param {Object} options - The options for the fetch request.
 * @param {string} options.url - REQUIRED The URL to which the request is sent.
 * @param {Object} [options.headers] - The headers to include with the request. Always uses 'Content-Type: application/json'.
 * @param {string} [options.method='GET'] - The HTTP method to use for the request. Defaults to 'GET'.
 * @param {Object} [options.params] - The URL parameters to include with the request.
 * @param {Object} [options.data] - The JSON data to include in the body of the request (will be stringified).
 * @param {AbortSignal} [options.signal] - An AbortSignal to cancel the request.
 * @returns {Promise<Object>} The response data parsed as JSON.
 * @throws {Error} If the URL is missing, the response status is not in the range 200-299, or if there is a bsq error (bsq = build session query).
 */
export async function fetchWrapper (options) {
  // url is required for every request
  if (!options.url) { throw new Error('missing url'); }

  // but it shouldn't be part of the options
  let url = options.url;
  delete options.url;

  // set the request headers (including cookies)
  options.headers = setReqHeaders({
    ...options.headers,
    'Content-Type': 'application/json' // default is json
  });

  if (!options.method) { // default method is GET
    options.method = 'GET';
  }

  if (options.params) { // add any params to the url
    for (const key in options.params) {
      if (options.params[key] === undefined) {
        delete options.params[key];
      }
    }
    const params = new URLSearchParams(options.params);
    url += `?${params}`;
    delete options.params;
  }

  // json data needs to be stringified in the body
  if (options.data && typeof options.data === 'object') {
    options.body = JSON.stringify(options.data);
    delete options.data;
  }

  const response = await fetch(url, options); // fetch the data!

  // add the response time to the store so it can be displayed
  if (response.headers.get('x-arkime-response-time')) {
    store.commit('setResponseTime', response.headers.get('x-arkime-response-time'));
  }

  const data = await response.json(); // parse the response!

  // catch bad status codes and throw an error
  if (response.status < 200 || response.status >= 300) {
    throw new Error(data.text || response.statusText || 'bad response status');
  }

  if (data.data?.bsqErr) { // check for a bsq error
    // bsq = build session query
    throw new Error(data.data.bsqErr);
  }

  return data; // done!
}

/**
 * A wrapper function for making HTTP requests using the Fetch API with the ability to cancel the request.
 *
 * @param {Object} options - The options for the fetch request.
 * @param {string} options.url - REQUIRED The URL to which the request is sent.
 * @param {Object} [options.headers] - The headers to include with the request. Always uses 'Content-Type: application/json'.
 * @param {string} [options.method='GET'] - The HTTP method to use for the request. Defaults to 'GET'.
 * @param {Object} [options.params] - The URL parameters to include with the request.
 * @param {Object} [options.data] - The JSON data to include in the body of the request (will be stringified).
 * @param {AbortSignal} [options.signal] - An AbortSignal to cancel the request.
 * @returns {AbortController} The AbortController used to cancel the request.
 * @returns {Promise<Object>} The response data parsed as JSON.
 * @throws {Error} If the URL is missing, the response status is not in the range 200-299, or if there is a bsq error (bsq = build session query).
 */
export function cancelFetchWrapper (options) {
  const controller = new AbortController();
  options.signal = controller.signal;
  return { controller, fetcher: fetchWrapper(options) };
}
