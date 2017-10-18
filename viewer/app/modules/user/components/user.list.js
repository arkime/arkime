(function() {

  'use strict';

  let timeout;

  /**
   * @class UserListController
   * @classdesc Interacts with moloch users page
   * @example
   * '<moloch-users></moloch-users>'
   */
  class UserListController {

    /**
     * Initialize global variables for this controller
     * @param UserService  Transacts users with the server
     * TODO
     *
     * @ngInject
     */
    constructor($scope, $timeout, $location, $anchorScroll, UserService) {
      this.$scope         = $scope;
      this.$timeout       = $timeout;
      this.$location      = $location;
      this.$anchorScroll  = $anchorScroll;
      this.UserService    = UserService;

      // offset anchor scroll position to account for navbar
      this.$anchorScroll.yOffset = 70;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.focusInput   = false;
      this.sortField    = 'userId';
      this.sortReverse  = false;
      this.query        = {length: 50, start: 0};
      this.currentPage  = 1;
      this.newuser      = {enabled:true};

      this.$scope.$on('change:pagination', (event, args) => {
        // pagination affects length, currentPage, and start
        this.query.length = args.length;
        this.query.start  = args.start;
        this.currentPage  = args.currentPage;

        this.loadData();
      });

      this.UserService.getSettings()
        .then((response) => { this.settings = response; })
        .catch((error)   => { this.settings = {timezone: 'local'}; });

      this.loadData();

      this.columns = [
        { name: 'User ID', sort: 'userId', nowrap: true, help: 'The id used for login, can not be changed once created' },
        { name: 'User Name', sort: 'userName', nowrap: true, help: 'Friendly name for user' },
        { name: 'Forced Expression', sort: 'expression', nowrap: true, help:'A moloch expression that is silently added to all queries. Useful to limit what data a user can access (ex what nodes or ips)'  },
        { name: 'Enabled', sort: 'enabled', nowrap: true, help: 'Is the account currently enabled for anything?' },
        { name: 'Admin', sort: 'createEnabled', nowrap: true, help: 'Can create new accounts and change the settings for other accounts' },
        { name: 'Web Interface', sort: 'webEnabled', help: 'Can access the web interface. When off only APIs can be used' },
        { name: 'Web Auth Header', sort: 'headerAuthEnabled', help: 'Can login using the web auth header. This setting doesn\'t disable the password so it should be scrambled' },
        { name: 'Email Search', sort: 'emailSearch', help: 'Can perform email searches' },
        { name: 'Can Remove Data', sort: 'removeEnabled', help: 'Can delete tags or delete/scrub pcap data' }
      ];
    }

    /* fired when controller's containing scope is destroyed */
    $onDestroy() {
      if (timeout) { this.$timeout.cancel(timeout); }
    }

    /* remove the message when user is done with it or duration ends */
    messageDone() {
      this.msg = null;
      this.msgType = null;
    }

    goToNewUserForm() {
      let old = this.$location.hash();
      this.$location.hash('newUser');
      this.$anchorScroll();

      this.focusInput = true; // focus on first user form input box

      // reset to old to keep any additional routing logic from kicking in
      this.$location.hash(old);

      timeout = this.$timeout(() => {
        this.focusInput = false;
      });
    }

    columnClick(name) {
      this.sortField=name; 
      this.sortReverse = !this.sortReverse;
      this.loadData();
    }

    loadData() {
      this.loading = true;

      let params = {
        filter    : this.searchUsers,
        sortField : this.sortField,
        desc      : this.sortReverse,
        start     : this.query.start,
        length    : this.query.length
      };

      this.UserService.listUsers(params)
        .then((response) => {
          this.loading  = false;
          this.users    = response;
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error.text;
        });
    }

    userChanged(user) {
      this.UserService.updateUser(user)
        .then((response) => {
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    createUser() {
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

      this.UserService.createUser(this.newuser)
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

    deleteUser(user) {
      this.UserService.deleteUser(user)
        .then((response) => {
          this.loadData();

          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

  }

  UserListController.$inject = ['$scope','$timeout','$location','$anchorScroll',
    'UserService'];

  /**
   * Moloch Users Directive
   * Displays users
   */
  angular.module('moloch')
     .component('molochUsers', {
       template  : require('../templates/user.list.html'),
       controller: UserListController
     });

})();
