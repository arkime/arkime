(function() {

  'use strict';

  /**
   * @class CreateViewController
   * @classdesc Interacts with create view area
   *
   * @example
   * '<create-view></create-view>'
   */
  class CreateViewController {

    /**
     * Initialize global variables for this controller
     * @param $scope      Angular application model object
     * @param UserService Transacts users with the server
     *
     * @ngInject
     */
    constructor($scope, UserService) {
      this.$scope       = $scope;
      this.UserService  = UserService;
    }

    $onInit() {
      this.viewName = '';
      this.loading  = false;

      if (!this.expression) { this.expression = ''; }
    }


    /* exposed functions --------------------------------------------------- */
    /**
     * Triggered by add/remove tag button in form
     * @param {bool} addTags Whether to add tags or remove tags
     */
    createView() {
      if (!this.viewName || this.viewName === '') {
        this.error = 'No view name specified.';
        return;
      }

      if (!this.expression || this.expression === '') {
        this.error = 'No expression specified.';
        return;
      }

      this.loading = true;

      let data = {
        viewName  : this.viewName,
        expression: this.expression
      };

      this.UserService.createView(data)
         .then((response) => {
           this.loading = false;

           let args = {};

           if (response.text) {
             args.message = response.text;
             args.success = response.success;
           }

           // notify parent to close form
           this.$scope.$emit('close:form:container', args);

           // notify parent to update views
           this.$scope.$emit('update:views', { views: response.views });
         })
         .catch((error) => {
           this.error    = error;
           this.loading  = false;
         });
    }

    cancel() { // close the form
      this.$scope.$emit('close:form:container');
    }

  }

  CreateViewController.$inject = ['$scope', 'UserService'];

  /**
   * Session Tag Directive
   * Displays add/remove tags form
   */
  angular.module('moloch')
     .component('createView', {
       template  : require('../templates/create.view.html'),
       controller: CreateViewController,
       bindings  : {
         expression : '<', // the existing query expression
       }
     });

})();
