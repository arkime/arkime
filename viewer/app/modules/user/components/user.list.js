(function() {

  'use strict';

  /**
   * @class UserListController
   * @classdesc Interacts with moloch users page
   * @example
   * '<moloch-fields></moloch-fields>'
   */
  class UserListController {

    /**
     * Initialize global variables for this controller
     * @param UserService  Transacts users with the server
     *
     * @ngInject
     */
    constructor($scope, UserService) {
      this.$scope         = $scope;
      this.UserService    = UserService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
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
        .then((response) => {this.settings = response; })
        .catch((error)   => {this.settings = {timezone: "local"}; });

      this.loadData();

      this.columns = [
        { name: 'User Id', sort: 'userId'},
        { name: 'User Name', sort: 'userName' },
        { name: 'Forced Expression', sort: 'expression' },
        { name: 'Enabled?', sort: 'enabled' },
        { name: 'Admin?', sort: 'createEnabled' },
        { name: 'Web Interface?', sort: 'webEnabled' },
        { name: 'Web Auth Header?', sort: 'headerAuthEnabled' },
        { name: 'Email Search?', sort: 'emailSearch' },
        { name: 'Can Remove Data?', sort: 'removeEnabled' },
        { name: '', sort: '' }
      ];
    }

    columnClick(name) {
      this.sortField=name; 
      this.sortReverse = !this.sortReverse;
      this.loadData();
    }

    loadData() {
      this.UserService.listUsers({filter: this.searchUsers, sortField: this.sortField, desc: this.sortReverse, start: this.query.start, length:this.query.length})
        .then((response)  => { this.users = response; })
        .catch((error)    => { this.error = error; });
    }

    userChanged(user) {
      this.UserService.updateUser(user);
    }

    createUser() {
      console.log("CREATE", this.newuser);
      this.error = "";
      if (this.newuser.userId === "" || this.newuser.userId === undefined) {
        this.error = "User Id can not be empty";
        return;
      }
      if (this.newuser.userName === "" || this.newuser.userName === undefined) {
        this.error = "User Name can not be empty";
        return;
      }

      if (this.newuser.password === "" || this.newuser.password === undefined) {
        this.error = "Password can not be empty";
        return;
      }

      this.UserService.createUser(this.newuser)
        .then((response) => {this.newuser = {enabled: true}; this.loadData(); })
        .catch((error)   => {this.error = error; });
    }

    deleteUser(user) {
      this.UserService.deleteUser(user)
        .then((response) => {this.loadData(); })
        .catch((error)   => {this.error = error; });
    }

  }

  UserListController.$inject = ['$scope', 'UserService'];

  /**
   * Moloch Users Directive
   * Displays pcap users
   */
  angular.module('moloch')
     .component('molochUsers', {
       template  : require('html!../templates/user.list.html'),
       controller: UserListController
     });

})();
