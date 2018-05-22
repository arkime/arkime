<template>

  <div>

    <div class="users-search">
      <div class="mr-1 ml-1 mt-1 mb-1">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text">
              <span class="fa fa-search">
              </span>
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="query.filter"
            @keyup="searchForUsers()"
            placeholder="Begin typing to search for users by name">
        </div>
      </div>
    </div>

    <div class="users-paging">
      <div class="ml-1 mt-1 pull-right">
        <moloch-toast
          class="mr-1"
          :message="msg"
          :type="msgType"
          :done="messageDone">
        </moloch-toast>
      </div>
      <moloch-paging v-if="users"
        class="mt-1 ml-1"
        :records-total="users.recordsTotal"
        :records-filtered="users.recordsFiltered"
        v-on:changePaging="changePaging">
      </moloch-paging>
    </div>

    <div class="users-content">

      <moloch-error v-if="error"
        :message="error">
      </moloch-error>

      <div v-if="!error">

        <moloch-loading v-if="loading">
        </moloch-loading>

        <!-- user table -->
        <table v-if="users"
          class="table table-sm table-striped small">
          <thead>
            <tr>
              <th v-for="column of columns"
                :key="column.name"
                class="cursor-pointer"
                :class="{'no-wrap':column.nowrap}"
                @click="columnClick(column.sort)">
                {{ column.name }}
                <span v-if="column.sort !== undefined">
                  <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                  <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                  <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
                </span>
              </th>
              <th>&nbsp;</th>
            </tr>
          </thead>
          <transition-group name="list"
            tag="tbody">
            <tr v-for="(user, index) of users.data"
              :key="user.id">
              <td class="no-wrap">
                {{ user.userId }}
              </td>
              <td class="no-wrap">
                <input v-model="user.userName"
                  class="form-control form-control-sm"
                  type="text"
                  @change="userChanged(user)"
                />
              </td>
              <td class="no-wrap">
                <input v-model="user.expression"
                  class="form-control form-control-sm"
                  type="text"
                  @change="userChanged(user)"
                />
              </td>
              <td class="no-wrap">
                <input type="checkbox"
                  v-model="user.enabled"
                  @change="userChanged(user)"
                />
              </td>
              <td class="no-wrap">
                <input type="checkbox"
                  v-model="user.createEnabled"
                  @change="userChanged(user)"
                />
              </td>
              <td class="no-wrap">
                <input type="checkbox"
                  v-model="user.webEnabled"
                  @change="userChanged(user)"
                />
              </td>
              <td class="no-wrap">
                <input type="checkbox"
                  v-model="user.headerAuthEnabled"
                  @change="userChanged(user);"
                />
              </td>
              <td class="no-wrap">
                <input type="checkbox"
                  v-model="user.emailSearch"
                  @change="userChanged(user)"
                />
              </td>
              <td class="no-wrap">
                <input type="checkbox"
                    v-model="user.removeEnabled"
                    @change="userChanged(user)"
                  />
              </td>
              <td class="no-wrap pull-right">
                <a class="btn btn-sm btn-theme-primary"
                  :href="`settings?userId=${user.userId}`"
                  v-b-tooltip.hover
                  :title="`Settings for ${user.userId}`">
                  <span class="fa fa-gear">
                  </span>
                </a>
                <a class="btn btn-sm btn-theme-secondary"
                  :href="`history?userId=${user.userId}`"
                  v-b-tooltip.hover
                  :title="`History for ${user.userId}`">
                  <span class="fa fa-history">
                  </span>
                </a>
                <button type="button"
                  class="btn btn-sm btn-danger"
                  @click="deleteUser(user, index)"
                  v-b-tooltip.hover
                  :title="`Delete ${user.userId}`">
                  <span class="fa fa-trash-o">
                  </span>
                </button>
              </td>
            </tr>
          </transition-group>
        </table> <!-- /user table -->

        <!-- new user form -->
        <div class="row new-user-form mr-1 ml-1 mt-4">

          <div class="col-sm-8">
            <div class="row mb-3">
              <div class="col-sm-9 offset-sm-3">
                <h3 class="mt-3">
                  New User
                </h3>
              </div>
            </div>
            <form>
              <div class="form-group row">
                <label for="userid"
                  class="col-sm-3 col-form-label text-right">
                  User ID
                </label>
                <div class="col-sm-9">
                  <input id="userid"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newuser.userId"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="username"
                  class="col-sm-3 col-form-label text-right">
                  User Name
                </label>
                <div class="col-sm-9">
                  <input id="username"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newuser.userName"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="expression"
                  class="col-sm-3 col-form-label text-right">
                  Forced Expression
                </label>
                <div class="col-sm-9">
                  <input id="expression"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newuser.expression"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label for="password"
                  class="col-sm-3 col-form-label text-right">
                  Password
                </label>
                <div class="col-sm-9">
                  <input id="password"
                    type="password"
                    class="form-control form-control-sm"
                    v-model="newuser.password"
                  />
                </div>
              </div>
              <div>
                <button type="button"
                  class="btn btn-sm btn-theme-tertiary pull-right mb-4"
                  @click="createUser()">
                  <span class="fa fa-plus-circle">
                  </span>&nbsp;
                  Create
                </button>
                <span v-if="createError"
                  class="pull-right alert alert-sm alert-danger mr-3">
                  <span class="fa fa-exclamation-triangle">
                  </span>&nbsp;
                  {{ createError }}
                </span>
              </div>
            </form>
          </div>

          <div class="col-sm-4">
            <div class="row mb-3">
              <div class="col-sm-12">
                <h3 class="mt-2">&nbsp;</h3>
              </div>
            </div>
            <form>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    title="Is the account currently enabled for anything?">
                    <input type="checkbox"
                      v-model="newuser.enabled"
                    />
                    Enabled
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    title="Can create new accounts and change the settings for other accounts">
                    <input type="checkbox"
                      v-model="newuser.createEnabled"
                    />
                    Admin
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    title="Can access the web interface. When off only APIs can be used">
                    <input type="checkbox"
                      v-model="newuser.webEnabled"
                    />
                    Web Interface
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    title="Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled">
                    <input type="checkbox"
                      v-model="newuser.headerAuthEnabled"
                    />
                    Web Auth Header
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    title="Can perform email searches">
                    <input type="checkbox"
                      v-model="newuser.emailSearch"
                    />
                    Email Search
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    title="Can delete tags or delete/scrub pcap data">
                    <input type="checkbox"
                      v-model="newuser.removeEnabled"
                    />
                    Can Remove Data
                  </label>
                </div>
              </div>
            </form>
          </div>
        </div> <!-- /new user form -->

      </div>

    </div> <!-- /users content -->
  </div> <!-- container-fluid -->

</template>

<script>
import UserService from '../users/UserService';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';
import MolochToast from '../utils/Toast';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Users',
  components: { MolochPaging, MolochError, MolochLoading, MolochToast },
  data: function () {
    return {
      error: '',
      loading: true,
      user: null,
      users: null,
      createError: '',
      newuser: { enabled: true },
      msg: '',
      msgType: undefined,
      query: {
        length: parseInt(this.$route.query.length) || 50,
        start: 0,
        filter: null,
        sortField: 'userId',
        desc: false
      },
      columns: [
        { name: 'User ID', sort: 'userId', nowrap: true, help: 'The id used for login, can not be changed once created' },
        { name: 'User Name', sort: 'userName', nowrap: true, help: 'Friendly name for user' },
        { name: 'Forced Expression', sort: 'expression', nowrap: true, help: 'A moloch expression that is silently added to all queries. Useful to limit what data a user can access (ex what nodes or ips)' },
        { name: 'Enabled', sort: 'enabled', nowrap: true, help: 'Is the account currently enabled for anything?' },
        { name: 'Admin', sort: 'createEnabled', nowrap: true, help: 'Can create new accounts and change the settings for other accounts' },
        { name: 'Web Interface', sort: 'webEnabled', help: 'Can access the web interface. When off only APIs can be used' },
        { name: 'Web Auth Header', sort: 'headerAuthEnabled', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled' },
        { name: 'Email Search', sort: 'emailSearch', help: 'Can perform email searches' },
        { name: 'Can Remove Data', sort: 'removeEnabled', help: 'Can delete tags or delete/scrub pcap data' }
      ]
    };
  },
  created: function () {
    this.loadUser();
    this.loadData();
  },
  methods: {
    /* exposed page functions ------------------------------------ */
    changePaging (pagingValues) {
      this.query.length = pagingValues.length;
      this.query.start = pagingValues.start;

      this.loadData();
    },
    searchForUsers () {
      if (searchInputTimeout) { clearTimeout(searchInputTimeout); }
      // debounce the input so it only issues a request after keyups cease for 400ms
      searchInputTimeout = setTimeout(() => {
        searchInputTimeout = null;
        this.loadData();
      }, 400);
    },
    columnClick (name) {
      this.query.sortField = name;
      this.query.desc = !this.query.desc;
      this.loadData();
    },
    /* remove the message when user is done with it or duration ends */
    messageDone: function () {
      this.msg = null;
      this.msgType = null;
    },
    userChanged: function (user) {
      this.$http.post('user/update', user)
        .then((response) => {
          this.msg = response.data.text;
          this.msgType = 'success';
        }, (error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    deleteUser: function (user, index) {
      this.$http.post('user/delete', user)
        .then((response) => {
          this.users.data.splice(index, 1);
          this.msg = response.data.text;
          this.msgType = 'success';
        }, (error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    createUser: function () {
      this.createError = '';

      if (!this.newuser.userId) {
        this.createError = 'User ID can not be empty';
        return;
      }

      if (!this.newuser.userName) {
        this.createError = 'User Name can not be empty';
        return;
      }

      if (!this.newuser.password) {
        this.createError = 'Password can not be empty';
        return;
      }

      this.$http.post('user/create', this.newuser)
        .then((response) => {
          this.newuser = { enabled: true };
          this.loadData();

          this.msg = response.data.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.createError = error.text;
        });
    },
    /* helper functions ------------------------------------------ */
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.user = response;
        }, (error) => {
          this.user = { settings: { timezone: 'local' } };
        });
    },
    loadData: function () {
      this.$http.post('user/list', this.query)
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.users = response.data;
        }, (error) => {
          this.loading = false;
          this.error = error.text;
        });
    }
  }
};
</script>

<style scoped>
/* search navbar */
.users-search {
  z-index: 5;
  position: fixed;
  right: 0;
  left: 0;
  top: 36px;
  border: none;
  background-color: var(--color-secondary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* paging/toast navbar */
.users-paging {
  z-index: 4;
  position: fixed;
  top: 75px;
  left: 0;
  right: 0;
  height: 40px;
  background-color: var(--color-quaternary-lightest);

  -webkit-box-shadow: 0 0 16px -2px black;
     -moz-box-shadow: 0 0 16px -2px black;
          box-shadow: 0 0 16px -2px black;
}

/* page content */
.users-content {
  margin-top: 125px;
}

.users-content form .form-group-sm .checkbox {
  min-height: 0;
  padding-top: 0;
  margin-bottom: -8px;
}

/* condense the form */
.new-user-form .form-group {
  margin-bottom: .25rem;
}
.new-user-form {
  box-shadow: inset 0 1px 1px rgba(0, 0, 0, .05);
  background-color: var(--color-gray-lighter);
  border: 1px solid var(--color-gray-light);
  border-radius: 3px;
}

/* field table animation */
.list-enter-active, .list-leave-active {
  transition: all .5s;
}
.list-enter, .list-leave-to {
  opacity: 0;
  transform: translateX(30px);
}
.list-move {
  transition: transform .5s;
}
</style>
