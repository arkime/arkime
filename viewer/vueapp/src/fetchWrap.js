import setReqHeaders from '@real_common/setReqHeaders';

// TODO VUE3 - cancel requests with fetch - see interceptors.js
// TODO VUE3 provide cancelToken for ES/OS (options.cancelToken)
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
    const params = new URLSearchParams(options.params);
    url += `?${params}`;
    delete options.params;
  }

  // json data needs to be stringified
  if (options.data && typeof options.data === 'object') {
    options.data = JSON.stringify(options.data);
  }

  const response = await fetch(url, options); // fetch the data!
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
