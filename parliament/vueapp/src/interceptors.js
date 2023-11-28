import axios from 'axios';

export default function setup () {
  // set xsrf cookie and always send credentials
  axios.defaults.withCredentials = true;
  axios.defaults.xsrfCookieName = 'PARLIAMENT-COOKIE';
  axios.defaults.xsrfHeaderName = 'X-PARLIAMENT-COOKIE';
  axios.defaults.headers.common['X-Requested-With'] = 'XMLHttpRequest';

  axios.interceptors.response.use((response) => {
    return response;
  }, (error) => {
    return new Promise((resolve, reject) => {
      if (!error.response) {
        error.response = {
          data: {
            text: error.message || 'Cannot connect to server',
            networkError: true
          }
        };
      }

      reject(error);
    });
  });
}
