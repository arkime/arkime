import setReqHeaders from './setReqHeaders';

export default {
  /**
   * Searches for users
   * @param {Object} query - The query to search for users
   *                         {desc:false,length:50,filter:"",sortField:"userId",start:0,searchFields:["userName","userId","roles"]}
   */
  searchUsers (query) {
    return new Promise((resolve, reject) => {
      fetch('api/users', {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(query)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          return reject(response);
        }
        return resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Searches for users
   * @param {Object} query - The query to search for limited user info (userId, userName, and whether they have some role)
   *                         {roleId:"role:something",filter:""}
   */
  searchAssignableUsers (query) {
    return new Promise((resolve, reject) => {
      fetch('api/users/list', {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(query)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          return reject(response);
        }
        return resolve(response);
      }).catch((error) => {
        reject(error);
      });
    });
  },

  /**
   * Sets whether a user has a certain role
   * @param {Object} query - The query to update a user's role
   *                         {userId:"user123",roleId:"role:something",newRoleState:true}
   */
  updateUserRole (query) {
    const { userId, roleId, newRoleState } = query;

    return new Promise((resolve, reject) => {
      fetch(`/api/user/${userId}/assignment`, {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify({ roleId, newRoleState })
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          return reject(response);
        }
        return resolve(response);
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
      fetch(`api/user/${user.id}`, {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(user)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          return reject(response);
        }
        return resolve(response);
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
      fetch(`api/user/${user.id}`, {
        method: 'DELETE',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(user)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          return reject(response);
        }
        return resolve(response);
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
      fetch('api/user', {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(user)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (!response.success) {
          return reject(response);
        }
        return resolve(response);
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
  },

  /**
   * Changes current user's password
   * @param {object} data       The data to send to the server
   *                            { newPassword }
   * @param {string} userId     The unique identifier for a user
   *                            (only required if not the current user)
   * @returns {Promise} Promise A promise object that signals the completion
   *                            or rejection of the request.
   */
  changePassword (data, userId) {
    return new Promise((resolve, reject) => {
      let url = 'api/user/password';
      if (userId) { url += `?userId=${userId}`; }

      fetch(url, {
        method: 'POST',
        headers: setReqHeaders({ 'Content-Type': 'application/json' }),
        body: JSON.stringify(data)
      }).then((response) => {
        return response.json();
      }).then((response) => {
        if (response.success) {
          return resolve(response);
        } else {
          return reject(response.text);
        }
      });
    });
  }
};
