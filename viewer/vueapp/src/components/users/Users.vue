<template>

  <div class="container-fluid">

    <div class="sub-navbar">
      <span class="sub-navbar-title">
        <span class="fa-stack">
          <span class="fa fa-users-o fa-stack-1x"></span>
          <span class="fa fa-square-o fa-stack-2x"></span>
        </span>&nbsp;
        Users
      </span>

      <div class="input-group input-group-sm user-search pull-right mt-1">
        <div class="input-group-prepend">
          <span class="input-group-text">
            <span class="fa fa-search"></span>
          </span>
        </div>
        <input type="text"
          class="form-control"
          v-model="query.filter"
          @keyup="searchForUsers()"
          placeholder="Begin typing to search for users by name">
      </div>

      <moloch-paging v-if="users"
        class="inline-paging pull-right mr-2"
        :records-total="users.recordsTotal"
        :records-filtered="users.recordsFiltered"
        v-on:changePaging="changePaging">
      </moloch-paging>

    </div>

    <div class="users-content">

      <moloch-loading v-if="loading && !error">
      </moloch-loading>

      <moloch-error v-if="error"
        :message="error">
      </moloch-error>

      <div v-show="!error">
        <table class="table table-sm text-right small">
          <thead>
            <tr>
              <th v-for="column of columns"
                :key="column.name"
                class="cursor-pointer"
                :class="{'text-left':!column.doStats}"
                @click="columnClick(column.sort)">
                {{ column.name }}
                <span v-if="column.sort !== undefined">
                  <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                  <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                  <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
                </span>
              </th>
            </tr>
          </thead>
          <tbody v-if="users">
            <!-- TODO: debounce:500 input fields -->
            <template v-for="user of users.data">
              <tr :key="user.id">
                <td class="no-wrap">{{user.userId}}</td>
                <td class="no-wrap">
                  <input class="form-control input-sm" type="text" v-model="user.userName" @change="userChanged(user)">
                </td>
                <td class="no-wrap">
                  <input class="form-control input-sm" type="text" v-model="user.expression" @change="userChanged(user);">
                </td>
                <td class="no-wrap"><input type="checkbox" v-model="user.enabled" @change="userChanged(user);"></td>
                <td class="no-wrap"><input type="checkbox" v-model="user.createEnabled" @change="userChanged(user);"></td>
                <td class="no-wrap"><input type="checkbox" v-model="user.webEnabled" @change="userChanged(user);"></td>
                <td class="no-wrap"><input type="checkbox" v-model="user.headerAuthEnabled" @change="userChanged(user);"></td>
                <td class="no-wrap"><input type="checkbox" v-model="user.emailSearch" @change="userChanged(user);"></td>
                <td class="no-wrap"><input type="checkbox" v-model="user.removeEnabled" @change="userChanged(user);"></td>
                <td class="no-wrap">
                  <a class="btn btn-sm btn-theme-primary"
                    :href="`settings?userId=${user.userId}`"
                    v-b-tooltip.hover :title="`Settings for ${user.userId}`">
                    <span class="fa fa-gear"></span>
                  </a>
                  <a class="btn btn-sm btn-theme-secondary"
                    :href="`history?userId=${user.userId}`"
                    v-b-tooltip.hover :title="`History for ${user.userId}`">
                    <span class="fa fa-history"></span>
                  </a>
                  <a class="btn btn-sm btn-danger"
                    v-click="deleteUser(user)"
                    v-b-tooltip.hover :title="`Delete ${user.userId}`">
                    <span class="fa fa-trash-o"></span>
                  </a>
                </td>
              </tr>
            </template>
          </tbody>
        </table>
      </div> <!-- /!error -->

      <div class="row" id="newUser"> <!-- new user form -->

        <div class="col-sm-8">
          <div class="row margined-bottom-xlg">
            <div class="col-sm-9 col-sm-offset-3">
              <h3>New User</h3>
            </div>
          </div>
          <form class="form-horizontal">
            <div class="form-group">
              <label for="userid" class="col-sm-3 control-label">User ID</label>
              <div class="col-sm-9">
                <input id="userid" class="form-control input-sm" type="text"
                  v-model="newuser.userId" focus-input="focusInput">
              </div>
            </div>
            <div class="form-group">
              <label for="username" class="col-sm-3 control-label">User Name</label>
              <div class="col-sm-9">
                <input id="username" class="form-control input-sm" type="text"
                  v-model="newuser.userName">
              </div>
            </div>
            <div class="form-group">
              <label for="expression" class="col-sm-3 control-label">Forced Expression</label>
              <div class="col-sm-9">
                <input id="expression" class="form-control input-sm" type="text"
                  v-model="newuser.expression">
              </div>
            </div>
            <div class="form-group">
              <label for="password" class="col-sm-3 control-label">Password</label>
              <div class="col-sm-9">
                <input id="password" class="form-control input-sm" type="password"
                  v-model="newuser.password">
              </div>
            </div>
            <div>
            <button class="btn btn-sm btn-theme-tertiary pull-right margined-top"
              @click="createUser()">
              <span class="fa fa-plus-circle"></span>&nbsp;
              Create
            </button>
            <span v-if="createError"
              class="pull-right alert alert-sm alert-danger margined-right-xxlg">
              <span class="fa fa-exclamation-triangle"></span>&nbsp;
              {{createError}}
            </span>
            </div>
          </form>
        </div>

        <div class="col-sm-4">
          <div class="row margined-bottom-xlg">
            <div class="col-sm-12"><h3>&nbsp;</h3></div>
          </div>
          <form class="form-horizontal">
            <div class="form-group form-group-sm">
              <div class="col-sm-offset-1 col-sm-11">
                <div class="checkbox">
                  <label tooltip-placement="left"
                    v-b-tooltip.hover title="Is the account currently enabled for anything?">
                    <input type="checkbox" v-model="newuser.enabled">
                    Enabled
                  </label>
                </div>
              </div>
            </div>
            <div class="form-group form-group-sm">
              <div class="col-sm-offset-1 col-sm-11">
                <div class="checkbox">
                  <label tooltip-placement="left"
                    v-b-tooltip.hover title="Can create new accounts and change the settings for other accounts">
                    <input type="checkbox" v-model="newuser.createEnabled">
                    Admin
                  </label>
                </div>
              </div>
            </div>
            <div class="form-group form-group-sm">
              <div class="col-sm-offset-1 col-sm-11">
                <div class="checkbox">
                  <label tooltip-placement="left"
                    v-b-tooltip.hover title="Can access the web interface. When off only APIs can be used">
                    <input type="checkbox" v-model="newuser.webEnabled">
                    Web Interface
                  </label>
                </div>
              </div>
            </div>
            <div class="form-group form-group-sm">
              <div class="col-sm-offset-1 col-sm-11">
                <div class="checkbox">
                  <label tooltip-placement="left"
                    v-b-tooltip.hover title="Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled">
                    <input type="checkbox" v-model="newuser.headerAuthEnabled">
                    Web Auth Header
                  </label>
                </div>
              </div>
            </div>
            <div class="form-group form-group-sm">
              <div class="col-sm-offset-1 col-sm-11">
                <div class="checkbox">
                  <label tooltip-placement="left"
                    v-b-tooltip.hover title="Can perform email searches">
                    <input type="checkbox" v-model="newuser.emailSearch">
                    Email Search
                  </label>
                </div>
              </div>
            </div>
            <div class="form-group form-group-sm">
              <div class="col-sm-offset-1 col-sm-11">
                <div class="checkbox">
                  <label tooltip-placement="left"
                    v-b-tooltip.hover title="Can delete tags or delete/scrub pcap data">
                    <input type="checkbox" v-model="newuser.removeEnabled">
                    Can Remove Data
                  </label>
                </div>
              </div>
            </div>
          </form>
        </div>
      </div> <!-- /new user form -->

    </div> <!-- /users content -->
  </div> <!-- container-fluid -->

</template>

<script>
import UserService from '../UserService';
import MolochPaging from '../utils/Pagination';
import MolochError from '../utils/Error';
import MolochLoading from '../utils/Loading';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Users',
  components: {MolochPaging, MolochError, MolochLoading},
  data: function () {
    return {
      error: '',
      loading: true,
      user: null,
      users: null,
      newuser: {enabled: true},
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
    loadUser: function () {
      UserService.getCurrent()
        .then((response) => {
          this.user = response;
        }, (error) => {
          this.user = { settings: { timezone: 'local' } };
        });
    },
    loadData: function () {
      this.$http.post('user/list', { params: this.query })
        .then((response) => {
          this.error = '';
          this.loading = false;
          this.users = response.data;
        }, (error) => {
          this.loading = false;
          this.error = error;
        });
    },
    onError: function (message) {
      this.childError = message;
    },
    userChanged: function (user) {
      this.$http.post('user/update', user)
        .then((response) => {
          this.msg = response.text;
          this.msgType = 'success';
        }, (error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    deleteUser: function (user) {
      this.$http.post('user/update', user)
        .then((response) => {
          this.msg = response.text;
          this.msgType = 'success';
        }, (error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    },
    createUser: function () {
      this.createError = '';

      if (this.newuser.userId === '' || this.newuser.userId === undefined) {
        this.createError = 'User ID can not be empty';
        return;
      }

      if (this.newuser.userName === '' || this.newuser.userName === undefined) {
        this.createError = 'User Name can not be empty';
        return;
      }

      if (this.newuser.password === '' || this.newuser.password === undefined) {
        this.createError = 'Password can not be empty';
        return;
      }

      this.$http.post('user/create', this.newuser)
        .then((response) => {
          this.newuser = { enabled: true };
          this.loadData();

          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.createError = error.text;
        });
    }
  }
};
</script>
<style scoped>
.users-content {
  margin-top: 90px;
}

.inline-paging {
  display: inline-block;
  margin-bottom: -13px;
  margin-top: -4px;
}

.user-search {
  max-width: 50%;
  margin-bottom: -13px;
  margin-top: -4px;
  max-width: 333px;
}

.users-content form .form-group-sm .checkbox {
  min-height    : 0;
  padding-top   : 0;
  margin-bottom : -8px;
}
</style>
