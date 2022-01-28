import axios from 'axios';

export default {
  /**
   * Searches for users
   * @param {Object} query - The query to search for users
   *                         {desc:false,length:50,filter:"",sortField:"userId",start:0}
   */
  searchUsers (query) {
    return new Promise((resolve, reject) => {
      const options = {
        data: query,
        method: 'POST',
        url: 'api/users'
      };

      axios(options).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Updates a user
   * @param {Object} user - The updated user object
   */
  updateUser (user) {
    return new Promise((resolve, reject) => {
      axios.post(`api/user/${user.id}`, user).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Deletes a user
   * @param {Object} user - The user to delete
   */
  deleteUser (user) {
    return new Promise((resolve, reject) => {
      axios.delete(`api/user/${user.id}`, user).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Creates a new user
   * @param {Object} user - The user to create
   */
  createUser (user) {
    return new Promise((resolve, reject) => {
      axios.post('api/user', user).then((response) => {
        resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Determines whether a user has role to perform a specific task
   * @param {Object} user The user to check roles for
   * @param {String} role The role in question
   * @returns {Boolean} true if all roles are included
   */
  hasRole (user, role) {
    if (!user) { return false; }
    const roles = role.split(',');
    for (let r of roles) {
      let reverse = false;
      if (r.startsWith('!')) {
        reverse = true;
        r = r.substr(1);
      }
      if ((!reverse && !user.roles.includes(r)) ||
        (reverse && user.roles.includes(r))) {
        return false;
      }
    }
    return true;
  }
};
