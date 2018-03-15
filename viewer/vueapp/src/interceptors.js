import axios from 'axios';

export default function setup () {
  // set moloch xsrf cookie and always send credentials
  axios.defaults.withCredentials = true;
  axios.defaults.xsrfCookieName = 'MOLOCH-COOKIE';
  axios.defaults.xsrfHeaderName = 'X-MOLOCH-COOKIE';

  // watch for no response to let the user know the server is down
  axios.interceptors.response.use(function (response) {
    return response;
  }, function (error) {
    if (!error.response) {
      error = error.message || 'Cannot connect to server: request timed out or canceled.';
      return Promise.reject(error);
    }
    return Promise.reject(error.response.data);
  });
}
