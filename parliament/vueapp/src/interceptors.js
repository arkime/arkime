import axios from 'axios';
import AuthService from './auth';

export default function setup () {
  axios.defaults.withCredentials = true;

  axios.interceptors.request.use((config) => {
    const token = AuthService.getToken();

    if (token) {
      config.headers['x-access-token'] = token;
    }

    return config;
  }, (error) => {
    return Promise.reject(error);
  });

  axios.interceptors.response.use((response) => {
    return response;
  }, (error) => {
    return new Promise((resolve, reject) => {
      if (error.response && !error.response.success && error.response.data.tokenError) {
        // Token was not sent or was rejected, log out the user in the UI
        AuthService.logout();
      } else if (!error.response) {
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
