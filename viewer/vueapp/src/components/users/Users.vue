<template>

  <div>

    <div class="users-search">
      <div class="mr-1 ml-1 mt-1 mb-1">
        <div class="input-group input-group-sm">
          <div class="input-group-prepend">
            <span class="input-group-text input-group-text-fw">
              <span v-if="!shiftKeyHold"
                class="fa fa-search fa-fw">
              </span>
              <span v-else
                class="query-shortcut">
                Q
              </span>
            </span>
          </div>
          <input type="text"
            class="form-control"
            v-model="query.filter"
            v-focus-input="focusInput"
            @blur="onOffFocus"
            @input="searchForUsers"
            placeholder="Begin typing to search for users by name"
          />
          <span class="input-group-append">
            <button type="button"
              @click="clear"
              :disabled="!query.filter"
              class="btn btn-outline-secondary btn-clear-input">
              <span class="fa fa-close">
              </span>
            </button>
          </span>
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
              <th width="50px;">&nbsp;</th>
              <th v-for="column of notAdditionalCols"
                :key="column.name"
                class="cursor-pointer"
                :class="{'no-wrap':column.nowrap}"
                :title="column.help"
                v-b-tooltip.hover
                @click="columnClick(column.sort)">
                {{ column.name }}
                <span v-if="column.sort !== undefined">
                  <span v-show="query.sortField === column.sort && !query.desc" class="fa fa-sort-asc"></span>
                  <span v-show="query.sortField === column.sort && query.desc" class="fa fa-sort-desc"></span>
                  <span v-show="query.sortField !== column.sort" class="fa fa-sort"></span>
                </span>
              </th>
              <th width="180px">&nbsp;</th>
            </tr>
          </thead>
          <transition-group name="list"
            tag="tbody">
            <!-- no results -->
            <tr v-if="!users.data.length"
              key="noUsers"
              class="text-danger text-center">
              <td colspan="12"
                class="pt-2">
                <h6>No users match your search</h6>
              </td>
            </tr> <!-- /no results -->
            <!-- user -->
            <template v-for="(listUser, index) of users.data">
              <!-- user settings -->
              <tr :key="listUser.id + 'user'">
                <!-- /toggle settings button -->
                <td :class="{'btn-indicator':listUser.hideStats || listUser.hideFiles || listUser.hidePcap || listUser.disablePcapDownload || listUser.timeLimit || listUser.expression}">
                  <toggle-btn v-if="listUser.hideStats || listUser.hideFiles || listUser.hidePcap || listUser.disablePcapDownload || listUser.timeLimit || listUser.expression"
                    :opened="listUser.expanded"
                    v-b-tooltip.hover
                    title="This user has additional restricted permissions"
                    class="btn-toggle-user"
                    @toggle="toggleAdvSettings(listUser)">
                  </toggle-btn>
                  <toggle-btn v-else
                    :opened="listUser.expanded"
                    class="btn-toggle-user"
                    @toggle="toggleAdvSettings(listUser)">
                  </toggle-btn>
                </td> <!-- /toggle advanced settings button -->
                <td class="no-wrap">
                  <div class="cell-text">
                    {{ listUser.userId }}
                  </div>
                </td>
                <td class="no-wrap">
                  <input v-model="listUser.userName"
                    class="form-control form-control-sm"
                    type="text"
                    @input="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.enabled"
                    @change="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.createEnabled"
                    @change="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.webEnabled"
                    @change="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.headerAuthEnabled"
                    @change="userChanged(listUser);"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.emailSearch"
                    @change="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.removeEnabled"
                    @change="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <input type="checkbox"
                    v-model="listUser.packetSearch"
                    @change="userChanged(listUser)"
                  />
                </td>
                <td class="no-wrap">
                  <div v-if="listUser.lastUsed"
                    class="cell-text">
                    {{ listUser.lastUsed | timezoneDateString(user.settings.timezone, user.settings.ms) }}
                  </div>
                  <div v-else
                    class="cell-text">
                    Never
                  </div>
                </td>
                <td class="no-wrap">
                  <span class="pull-right">
                    <button v-if="listUser.changed"
                      type="button"
                      class="btn btn-sm btn-success"
                      @click="updateUser(listUser)"
                      v-b-tooltip.hover
                      :title="`Save the updated settings for ${listUser.userId}`">
                      <span class="fa fa-save">
                      </span>
                    </button>
                    <button v-if="listUser.changed"
                      type="button"
                      class="btn btn-sm btn-warning"
                      @click="loadData"
                      v-b-tooltip.hover
                      :title="`Cancel changed settings for ${listUser.userId}`">
                      <span class="fa fa-ban">
                      </span>
                    </button>
                    <button type="button"
                      class="btn btn-sm btn-theme-primary"
                      @click="openSettings(listUser.userId)"
                      v-b-tooltip.hover
                      :title="`Settings for ${listUser.userId}`">
                      <span class="fa fa-gear">
                      </span>
                    </button>
                    <button type="button"
                      class="btn btn-sm btn-theme-secondary"
                      @click="openHistory(listUser.userId)"
                      v-b-tooltip.hover
                      :title="`History for ${listUser.userId}`">
                      <span class="fa fa-history">
                      </span>
                    </button>
                    <button type="button"
                      class="btn btn-sm btn-danger"
                      @click="deleteUser(listUser, index)"
                      v-b-tooltip.hover
                      :title="`Delete ${listUser.userId}`">
                      <span class="fa fa-trash-o">
                      </span>
                    </button>
                  </span>
                </td>
              </tr> <!-- /user settings -->
              <!-- advanced user settings -->
              <tr :key="listUser.id + 'adv'"
                v-if="listUser.expanded">
                <td colspan="3">
                  <div class="row">
                    <div class="col mt-2">
                      <strong>
                        Configure additional user permissions:
                      </strong>
                    </div>
                  </div>
                </td>
                <td colspan="9">
                  <div class="row">
                    <div class="col">
                      <div class="form-check form-check-inline form-group">
                        <span v-b-tooltip.hover
                          :title="columns[12].help">
                          <input class="form-check-input"
                            type="checkbox"
                            :id="listUser.id + 'stats'"
                            v-model="listUser[columns[12].sort]"
                            @change="userChanged(listUser)"
                          />
                          <label class="form-check-label"
                            :for="listUser.id + 'stats'">
                            {{ columns[12].name }}
                          </label>
                        </span>
                        <span v-b-tooltip.hover
                          :title="columns[13].help">
                          <input class="form-check-input ml-3"
                            type="checkbox"
                            :id="listUser.id + 'files'"
                            v-model="listUser[columns[13].sort]"
                            @change="userChanged(listUser)"
                          />
                          <label class="form-check-label"
                            :for="listUser.id + 'files'">
                            {{ columns[13].name }}
                          </label>
                        </span>
                        <span v-b-tooltip.hover
                          :title="columns[14].help">
                          <input class="form-check-input ml-3"
                            type="checkbox"
                            :id="listUser.id + 'pcap'"
                            v-model="listUser[columns[14].sort]"
                            @change="userChanged(listUser)"
                          />
                          <label class="form-check-label"
                            :for="listUser.id + 'pcap'">
                            {{ columns[14].name }}
                          </label>
                        </span>
                        <span v-b-tooltip.hover
                          :title="columns[15].help">
                          <input class="form-check-input ml-3"
                            type="checkbox"
                            :id="listUser.id + 'pcapDownload'"
                            v-model="listUser[columns[15].sort]"
                            @change="userChanged(listUser)"
                          />
                          <label class="form-check-label"
                            :for="listUser.id + 'pcapDownload'">
                            {{ columns[15].name }}
                          </label>
                        </span>
                      </div>
                    </div>
                  </div>
                  <div class="row">
                    <div class="col form-group">
                      <div class="input-group input-group-sm">
                        <div class="input-group-prepend cursor-help"
                          v-b-tooltip.hover
                          :title="columns[10].help">
                          <span class="input-group-text">
                          {{ columns[10].name }}
                          </span>
                        </div>
                        <input v-model="listUser[columns[10].sort]"
                          class="form-control form-control-sm"
                          type="text"
                          @input="userChanged(listUser)"
                        />
                      </div>
                    </div>
                  </div>
                  <div class="row">
                    <div class="col form-group">
                      <div class="input-group input-group-sm">
                        <div class="input-group-prepend cursor-help"
                          v-b-tooltip.hover
                          :title="columns[11].help">
                          <span class="input-group-text">
                            {{ columns[11].name }}
                          </span>
                        </div>
                        <select class="form-control time-limit-select"
                          v-model="listUser[columns[11].sort]"
                          @change="changeTimeLimit(listUser)">
                          <option value="1">1 hour</option>
                          <option value="6">6 hours</option>
                          <option value="24">24 hours</option>
                          <option value="48">48 hours</option>
                          <option value="72">72 hours</option>
                          <option value="168">1 week</option>
                          <option value="336">2 weeks</option>
                          <option value="720">1 month</option>
                          <option value="1440">2 months</option>
                          <option value="4380">6 months</option>
                          <option value="8760">1 year</option>
                          <option value=undefined>All (careful)</option>
                        </select>
                      </div>
                    </div>
                  </div>
                </td>
              </tr> <!-- /advanced user settings -->
            </template> <!-- /user -->
          </transition-group>
        </table> <!-- /user table -->

        <!-- new user form -->
        <div class="row new-user-form mr-1 ml-1 mt-4">

          <div class="col-sm-7">
            <div class="row mb-3">
              <div class="col-sm-9 offset-sm-3">
                <h3 class="mt-3">
                  New User
                </h3>
              </div>
            </div>
            <form>
              <div class="form-group row">
                <label :for="columns[0].sort"
                  class="col-sm-3 col-form-label text-right">
                  {{ columns[0].name }}<sup v-if="columns[0].required">*</sup>
                </label>
                <div class="col-sm-9">
                  <input :id="columns[0].sort"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newuser[columns[0].sort]"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label :for="columns[1].sort"
                  class="col-sm-3 col-form-label text-right">
                  {{ columns[1].name }}<sup v-if="columns[1].required">*</sup>
                </label>
                <div class="col-sm-9">
                  <input :id="columns[1].sort"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newuser[columns[1].sort]"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label :for="columns[10].sort"
                  class="col-sm-3 col-form-label text-right cursor-help"
                  v-b-tooltip.hover
                  :title="columns[10].help">
                  {{ columns[10].name }}
                </label>
                <div class="col-sm-9">
                  <input :id="columns[10].sort"
                    type="text"
                    class="form-control form-control-sm"
                    v-model="newuser[columns[10].sort]"
                  />
                </div>
              </div>
              <div class="form-group row">
                <label :for="columns[11].sort"
                  class="col-sm-3 col-form-label text-right cursor-help"
                  v-b-tooltip.hover
                  :title="columns[11].help">
                  {{ columns[11].name }}
                </label>
                <div class="col-sm-9">
                  <select :id="columns[11].sort"
                    class="form-control form-control-sm"
                    v-model="newuser[columns[11].sort]">
                    <option value="1">1 hour</option>
                    <option value="6">6 hours</option>
                    <option value="24">24 hours</option>
                    <option value="48">48 hours</option>
                    <option value="72">72 hours</option>
                    <option value="168">1 week</option>
                    <option value="336">2 weeks</option>
                    <option value="720">1 month</option>
                    <option value="1440">2 months</option>
                    <option value="4380">6 months</option>
                    <option value="8760">1 year</option>
                    <option value=undefined selected>All (careful)</option>
                  </select>
                </div>
              </div>
              <div class="form-group row">
                <label for="password"
                  class="col-sm-3 col-form-label text-right">
                  Password<sup>*</sup>
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
                  @click="createUser">
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
          <div class="col-sm-3">
            <div class="row mb-3">
              <div class="col-sm-12">
                <h3 class="mt-2">&nbsp;</h3>
              </div>
            </div>
            <form>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[2].help">
                    <input type="checkbox"
                      v-model="newuser[columns[2].sort]"
                    />
                    Enabled
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[3].help">
                    <input type="checkbox"
                      v-model="newuser[columns[3].sort]"
                    />
                    Admin
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[4].help">
                    <input type="checkbox"
                      v-model="newuser[columns[4].sort]"
                    />
                    Web Interface
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[5].help">
                    <input type="checkbox"
                      v-model="newuser[columns[5].sort]"
                    />
                    Web Auth Header
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[6].help">
                    <input type="checkbox"
                      v-model="newuser[columns[6].sort]"
                    />
                    Email Search
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[7].help">
                    <input type="checkbox"
                      v-model="newuser[columns[7].sort]"
                    />
                    Can Remove Data
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm offset-sm-1 col-sm-11">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[8].help">
                    <input type="checkbox"
                      v-model="newuser[columns[8].sort]"
                    />
                    Packet Search
                  </label>
                </div>
              </div>
            </form>
          </div>
          <div class="col-sm-2">
            <div class="row mb-3">
              <div class="col-sm-12">
                <h3 class="mt-2">&nbsp;</h3>
              </div>
            </div>
            <form>
              <div class="form-group form-group-sm">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[12].help">
                    <input type="checkbox"
                      v-model="newuser[columns[12].sort]"
                    />
                    {{ columns[12].name }}
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[13].help">
                    <input type="checkbox"
                      v-model="newuser[columns[13].sort]"
                    />
                    {{ columns[13].name }}
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[14].help">
                    <input type="checkbox"
                      v-model="newuser[columns[14].sort]"
                    />
                    {{ columns[14].name }}
                  </label>
                </div>
              </div>
              <div class="form-group form-group-sm">
                <div class="checkbox">
                  <label v-b-tooltip.hover
                    :title="columns[15].help">
                    <input type="checkbox"
                      v-model="newuser[columns[15].sort]"
                    />
                    {{ columns[15].name }}
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
import FocusInput from '../utils/FocusInput';
import ToggleBtn from '../utils/ToggleBtn';

let searchInputTimeout; // timeout to debounce the search input

export default {
  name: 'Users',
  components: {
    MolochPaging,
    MolochError,
    MolochLoading,
    MolochToast,
    ToggleBtn
  },
  directives: { FocusInput },
  data: function () {
    return {
      error: '',
      loading: true,
      users: null,
      createError: '',
      newuser: {
        enabled: true,
        packetSearch: true
      },
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
        { name: 'User ID', sort: 'userId', nowrap: true, required: true, help: 'The id used for login, can not be changed once created' },
        { name: 'User Name', sort: 'userName', nowrap: true, required: true, help: 'Friendly name for user' },
        { name: 'Enabled', sort: 'enabled', nowrap: true, help: 'Is the account currently enabled for anything?' },
        { name: 'Admin', sort: 'createEnabled', nowrap: true, help: 'Can create new accounts and change the settings for other accounts' },
        { name: 'Web Interface', sort: 'webEnabled', help: 'Can access the web interface. When off only APIs can be used' },
        { name: 'Web Auth Header', sort: 'headerAuthEnabled', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled' },
        { name: 'Email Search', sort: 'emailSearch', help: 'Can perform email searches' },
        { name: 'Can Remove Data', sort: 'removeEnabled', help: 'Can delete tags or delete/scrub pcap data' },
        { name: 'Can Search Packets', sort: 'packetSearch', help: 'Can create a packet search job (hunt)' },
        { name: 'Last Used', sort: 'lastUsed', help: 'The last time this user used Moloch' },
        { additional: true, name: 'Forced Expression', sort: 'expression', help: 'A Moloch search expression that is silently added to all queries. Useful to limit what data a user can access (e.g. which nodes or IPs)' },
        { additional: true, name: 'Query Time Limit', sort: 'timeLimit', help: 'Restrict the maximum time window of a user\'s query' },
        { additional: true, name: 'Hide Stats Page', sort: 'hideStats', help: 'Hide the Stats page from this user' },
        { additional: true, name: 'Hide Files Page', sort: 'hideFiles', help: 'Hide the Files page from this user' },
        { additional: true, name: 'Hide PCAP', sort: 'hidePcap', help: 'Hide PCAP (and only show metadata/session detail) for this user when they open a Session' },
        { additional: true, name: 'Disable PCAP Download', sort: 'disablePcapDownload', help: 'Do not allow this user to download PCAP files' }
      ]
    };
  },
  computed: {
    user: {
      get: function () {
        return this.$store.state.user;
      },
      set: function (newValue) {
        this.$store.commit('setUser', newValue);
      }
    },
    focusInput: {
      get: function () {
        return this.$store.state.focusSearch;
      },
      set: function (newValue) {
        this.$store.commit('setFocusSearch', newValue);
      }
    },
    shiftKeyHold: function () {
      return this.$store.state.shiftKeyHold;
    },
    notAdditionalCols: function () {
      return this.columns.filter(column => !column.additional);
    }
  },
  created: function () {
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
    clear () {
      this.query.filter = undefined;
      this.loadData();
    },
    onOffFocus: function () {
      this.focusInput = false;
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
    changeTimeLimit: function (user) {
      if (user.timeLimit === 'undefined') {
        delete user.timeLimit;
      } else {
        user.timeLimit = parseInt(user.timeLimit);
      }
      this.userChanged(user);
    },
    userChanged: function (user) {
      this.$set(user, 'changed', true);
    },
    updateUser: function (user) {
      this.$set(user, 'expanded', undefined);
      this.$http.post('user/update', user)
        .then((response) => {
          this.msg = response.data.text;
          this.msgType = 'success';
          // update the current user if they were changed
          if (this.user.userId === user.userId) {
            // update all the fields
            for (let field in user) {
              if (this.user.hasOwnProperty(field) &&
                user[field] !== undefined) {
                this.user[field] = user[field];
              }
            }
            // time limit is special because it can be undefined
            this.user.timeLimit = user.timeLimit || undefined;
          }
          this.$set(user, 'changed', false);
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
          this.newuser = { enabled: true, packetSearch: true };
          this.loadData();

          this.msg = response.data.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.createError = error.text;
        });
    },
    openSettings: function (userId) {
      this.$router.push({
        path: '/settings',
        query: {
          ...this.$route.query,
          userId: userId
        }
      });
    },
    openHistory: function (userId) {
      this.$router.push({
        path: '/history',
        query: {
          ...this.$route.query,
          userId: userId
        }
      });
    },
    toggleAdvSettings: function (user) {
      this.$set(user, 'expanded', !user.expanded);
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

/* center cell content vertically */
.btn-toggle-user {
  margin-top: 2px;
}

td .cell-text {
  margin-top: 5px;
}

td input[type="checkbox"] {
  margin-top: 9px;
}

/* indication that a user has additional permissions set */
.btn-indicator .btn-toggle-user.btn-success {
  background: linear-gradient(135deg, #28a745 1%, #28a745 75%, #28a745 75%, #1c7730 77%, #1c7730 100%);
}

.form-control.time-limit-select {
  -webkit-appearance: none;
  border-radius: 0 4px 4px 0;
  max-width: 120px;
}

.form-check-inline {
  white-space: nowrap;
}
</style>
