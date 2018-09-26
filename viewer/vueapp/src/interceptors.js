import axios from 'axios';
import store from './store';

export default function setup () {
  // set moloch xsrf cookie and always send credentials
  axios.defaults.withCredentials = true;
  axios.defaults.xsrfCookieName = 'MOLOCH-COOKIE';
  axios.defaults.xsrfHeaderName = 'X-MOLOCH-COOKIE';
  axios.defaults.headers.common['X-Requested-With'] = 'XMLHttpRequest';

  // watch for no response to let the user know the server is down
  axios.interceptors.response.use(function (response) {
    // add the response time to the store so it can be displayed
    if (response.headers['x-moloch-response-time']) {
      store.commit('setResponseTime', response.headers['x-moloch-response-time']);
    }
    return response;
  }, function (error) {
    return new Promise((resolve, reject) => {
      if (axios.isCancel(error)) {
        // don't modify the cancelled request or else axios.isCancel will
        // not return true when being caught at the service or component level
        reject(error);
      }

      if (!error.response) {
        error = error.message || 'Cannot connect to server: request timed out or canceled.';
        reject(error);
      }

      reject(error.response.data);
    });
  });
}
