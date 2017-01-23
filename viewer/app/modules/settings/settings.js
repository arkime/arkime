(function() {

  'use strict';

  /**
   * @class SettingsController
   * @classdesc Interacts with moloch settings page
   * @example
   * '<moloch-settings></moloch-settings>'
   */
  class SettingsController {

    /**
     * Initialize global variables for this controller
     * @param $interval     Angular's wrapper for window.setInterval
     * @param UserService   Transacts users and user data with the server
     * @param FieldService  Transacts fields with the server
     * @param ConfigService Transacts app configurations with the server
     *
     * @ngInject
     */
    constructor($interval, UserService, FieldService, ConfigService) {
      this.$interval      = $interval;
      this.UserService    = UserService;
      this.FieldService   = FieldService;
      this.ConfigService  = ConfigService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading  = true;
      this.error    = false;
      this.view     = 'general';

      this.newCronQueryProcess  = '0';
      this.newCronQueryAction   = 'tag';

      this.UserService.getSettings()
        .then((response) => {
          this.settings = response;
          this.loading  = false;

          this.tick();
          this.$interval(() => { this.tick(); }, 1000);
        })
        .catch((error) => {
          this.loading  = false;
          this.error    = error;
        });

      this.UserService.getViews()
         .then((response) => {
           this.views = response;
         })
         .catch((error) => {
           this.viewListError = error;
         });

      this.UserService.getCronQueries()
        .then((response) => {
          this.cronQueries = response;
        })
        .catch((error) => {
          this.cronQueryListError = error;
        });

      this.ConfigService.getMolochClusters()
        .then((response) => {
          this.molochClusters = response;
        });

      this.FieldService.get(true)
        .then((response) => {
          this.fields     = response;
          this.fieldsPlus = response;
          this.fieldsPlus.push({
            dbField : 'ip.dst:port',
            exp     : 'ip.dst:port',
            help    : 'Destination IP:Destination Port'
          });
        });
    }

    /* page functions ------------------------------------------------------ */
    openView(view) {
      this.view = view;
    }

    /* remove the message when user is done with it or duration ends */
    messageDone() {
      this.msg = null;
      this.msgType = null;
    }

    /* GENERAL ------------------------------------------------------------- */
    /* saves the user's settings and displays a message */
    update() {
      this.UserService.saveSettings(this.settings)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    /* updates the date and format for the timezone setting */
    tick() {
      this.date = new Date();
      if (this.settings.timezone === 'gmt') {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss\'Z\'';
      } else {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss';
      }
    }

    /* updates the displayed date for the timzeone setting
     * triggered by the user changing the timezone setting */
    updateTime() {
      this.tick();
      this.update();
    }

    /**
     * Displays the field.exp instead of field.dbField in the
     * field typeahead inputs
     * @param {string} value The dbField of the field
     */
    formatField(value) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (value === this.fields[i].dbField) {
          return this.fields[i].exp;
        }
      }
    }

    /**
     * Gets the field that corresponds to a field's dbField value
     * @param {string} dbField The fields dbField value
     * @returns {object} field The field that corresponds to the entered dbField
     */
    getField(dbField) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (dbField === this.fields[i].dbField) {
          return this.fields[i];
        }
      }
    }

    /* VIEWS --------------------------------------------------------------- */
    /* creates a view given the view name and expression */
    createView() {
      if (!this.newViewName || this.newViewName === '') {
        this.viewFormError = 'No view name specified.';
        return;
      }

      if (!this.newViewExpression || this.newViewExpression === '') {
        this.viewFormError = 'No view expression specified.';
        return;
      }

      let data = {
        viewName  : this.newViewName,
        expression: this.newViewExpression
      };

      this.UserService.createView(data)
        .then((response) => {
          // add the view to the view list
          this.views[data.viewName] = {
            expression: data.expression,
            name      : data.viewName
          };
          this.viewFormError = false;
          // clear the inputs
          this.newViewName = null;
          this.newViewExpression = null;
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    /**
     * Sets a view as having been changed
     * @param {string} key The unique id of the changed view
     */
    viewChanged(key) {
      this.views[key].changed = true;
    }

    /**
     * Cancels a view change by retrieving the view
     * @param {string} key The unique id of the view
     */
    cancelViewChange(key) {
      this.UserService.getViews()
        .then((response) => {
          this.views[key] = response[key];
        })
        .catch((error) => {
          this.viewListError = error;
        });
    }

    /**
     * Updates a view
     * @param {string} key The unique id of the view to update
     */
    updateView(key) {
      let data = this.views[key];

      if (!data) {
        this.msg = 'Could not find corresponding view';
        this.msgType = 'danger';
        return;
      }

      if (!data.changed) {
        this.msg = 'This view has not changed';
        this.msgType = 'warning';
        return;
      }

      data.key = key;

      this.UserService.updateView(data)
        .then((response) => {
          // update view list
          this.views = response.views;
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
          // set the view as unchanged
          data.changed = false;
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    /**
     * Deletes a view given its name
     * @param {string} name The name of the view to delete
     */
    deleteView(name) {
      this.UserService.deleteView(name)
        .then((response) => {
          // remove the view from the view list
          this.views[name] = null;
          delete this.views[name];
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';

        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    /* CRON QUERIES -------------------------------------------------------- */
    /* creates a cron query given the name, expression, process, and tags */
    createCronQuery() {
      if (!this.newCronQueryName || this.newCronQueryName === '') {
        this.cronQueryFormError = 'No cron query name specified.';
        return;
      }

      if (!this.newCronQueryExpression || this.newCronQueryExpression === '') {
        this.cronQueryFormError = 'No cron query expression specified.';
        return;
      }

      if (!this.newCronQueryTags || this.newCronQueryTags === '') {
        this.cronQueryFormError = 'No cron query tags specified.';
        return;
      }

      let data = {
        enabled : true,
        name    : this.newCronQueryName,
        query   : this.newCronQueryExpression,
        action  : this.newCronQueryAction,
        tags    : this.newCronQueryTags,
        since   : this.newCronQueryProcess,
      };

      this.UserService.createCronQuery(data)
         .then((response) => {
           // add the cron query to the view
           this.cronQueryFormError = false;
           data.count = 0; // initialize count to 0
           this.cronQueries[response.key] = data;
           // reset fields
           this.newCronQueryName = null;
           this.newCronQueryTags = null;
           this.newCronQueryExpression = null;
           // display success message to user
           this.msg = response.text;
           this.msgType = 'success';
         })
         .catch((error) => {
           // display error message to user
           this.msg = error.text;
           this.msgType = 'danger';
         });
    }

    /**
     * Sets a cron query as having been changed
     * @param {string} key The unique id of the cron query
     */
    cronQueryChanged(key) {
      this.cronQueries[key].changed = true;
    }

    /**
     * Cancels a cron query change by retrieving the cron query
     * @param {string} key The unique id of the cron query
     */
    cancelCronQueryChange(key) {
      this.UserService.getCronQueries()
        .then((response) => {
          this.cronQueries[key] = response[key];
        })
        .catch((error) => {
          this.cronQueryListError = error;
        });
    }

    /**
     * Updates a cron query
     * @param {string} key The unique id of the cron query to update
     */
    updateCronQuery(key) {
      let data = this.cronQueries[key];

      if (!data) {
        this.msg = 'Could not find corresponding cron query';
        this.msgType = 'danger';
        return;
      }

      if (!data.changed) {
        this.msg = 'This cron query has not changed';
        this.msgType = 'warning';
        return;
      }

      data.key = key;

      this.UserService.updateCronQuery(data)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
          // set the cron query as unchanged
          data.changed = false;
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    /**
     * Deletes a cron query given its key
     * @param {string} key The cron query's key
     */
    deleteCronQuery(key) {
      this.UserService.deleteCronQuery(key)
        .then((response) => {
          // remove the cron query from the view
          this.cronQueries[key]  = null;
          delete this.cronQueries[key];
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }

    /* PASSWORD ------------------------------------------------------------ */
    /* changes the user's password given the current password, the new password,
     * and confirmation of the new password */
    changePassword() {
      if (!this.currentPassword || this.currentPassword === '') {
        this.changePasswordError = 'You must enter your current password';
        return;
      }

      if (!this.newPassword || this.newPassword === '') {
        this.changePasswordError = 'You must enter a new password';
        return;
      }

      if (!this.confirmNewPassword || this.confirmNewPassword === '') {
        this.changePasswordError = 'You must confirm your new password';
        return;
      }

      if (this.newPassword !== this.confirmNewPassword) {
        this.changePasswordError = 'Your passwords don\'t match';
        return;
      }

      this.UserService.changePassword(this.currentPassword, this.newPassword)
        .then((response) => {
          this.changePasswordError = false;
          this.currentPassword     = null;
          this.newPassword         = null;
          this.confirmNewPassword  = null;
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';
        })
        .catch((error) => {
          // display error message to user
          this.msg = error.text;
          this.msgType = 'danger';
        });
    }
  }

  SettingsController.$inject = ['$interval',
    'UserService', 'FieldService', 'ConfigService'];

  /**
   * ES Health Directive
   * Displays elasticsearch health status
   */
  angular.module('moloch')
     .component('molochSettings', {
       template  : require('html!./settings.html'),
       controller: SettingsController
     });

})();
