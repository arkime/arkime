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
     * TODO
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
           this.viewError = error;
         });

      this.UserService.getCronQueries()
        .then((response) => {
          this.cronQueries = response;
        })
        .catch((error) => {
          this.cronQueryError = error;
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
            dbField : "ip.dst:port",
            exp     : "ip.dst:port",
            help    : 'Destination IP:Destination Port'
          });
        });
    }

    openView(view) {
      this.view = view;
    }

    /* GENERAL ------------------------------------------------------------- */
    /* TODO */
    update() {
      this.UserService.saveSettings(this.settings)
        .then((response) => {
          // TODO doooo stuff?
        })
        .catch((error) => {
          this.error = error;
        });
    }

    /* TODO */
    tick() {
      this.date = new Date();
      if (this.settings.timezone === 'gmt') {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss\'Z\'';
      } else {
        this.dateFormat = 'yyyy/MM/dd HH:mm:ss';
      }
    }

    /* TODO */
    updateTime() {
      this.tick();
      this.update();
    }

    /** TODO */
    formatField(model) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (model === this.fields[i].dbField) {
          return this.fields[i].exp;
        }
      }
    }

    /** TODO */
    getField(dbField) {
      for (let i = 0, len = this.fields.length; i < len; i++) {
        if (dbField === this.fields[i].dbField) {
          return this.fields[i];
        }
      }
    }

    /* VIEWS --------------------------------------------------------------- */
    /** TODO */
    createView() {
      if (!this.newViewName || this.newViewName === '') {
        this.viewError = 'No view name specified.';
        return;
      }

      if (!this.newViewExpression || this.newViewExpression === '') {
        this.viewError = 'No expression specified.';
        return;
      }

      let data = {
        viewName  : this.newViewName,
        expression: this.newViewExpression
      };

      this.UserService.createView(data)
        .then((response) => {
          // TODO: display success
          this.viewError = false;
          this.views = response.data.views;
        })
        .catch((error) => {
          this.viewError = error;
        });
    }

    /** TODO */
    deleteView(name) {
      this.UserService.deleteView(name)
        .then((response) => {
          // TODO: display success
          if (response.success) {
            this.viewError = false;
            this.views[name] = null;
            delete this.views[name];
          }
        })
        .catch((error) => {
          this.viewError = error;
        });
    }

    /* CRON QUERIES -------------------------------------------------------- */
    /** TODO */
    createCronQuery() {
      if (!this.newCronQueryName || this.newCronQueryName === '') {
        this.cronQueryError = 'No cron query name specified.';
        return;
      }

      if (!this.newCronQueryExpression || this.newCronQueryExpression === '') {
        this.cronQueryError = 'No expression specified.';
        return;
      }

      if (!this.newCronQueryTags || this.newCronQueryTags === '') {
        this.cronQueryError = 'No tags specified.';
        return;
      }

      let data = {
        key     : '_create_',
        enabled : 'true',
        name    : this.newCronQueryName,
        query   : this.newCronQueryExpression,
        action  : this.newCronQueryAction,
        tags    : this.newCronQueryTags,
        since   : this.newCronQueryProcess,
      };

      this.UserService.createCronQuery(data)
         .then((response) => {
           // TODO: display success
           if (response.data.success) {
             this.cronQueryError = false;
             data.count = 0;
             this.cronQueries[response.data.key] = data;
           }
         })
         .catch((error) => {
           this.cronQueryError = error;
         });
    }

    /** TODO */
    deleteCronQuery(key) {
      this.UserService.deleteCronQuery(key)
         .then((response) => {
           // TODO: display success
           if (response.success) {
             this.cronQueryError = false;
             this.cronQueries[key] = null;
             delete this.cronQueries[key];
           }
         })
         .catch((error) => {
           this.cronQueryError = error;
         });
    }

    /* PASSWORD ------------------------------------------------------------ */
    /** TODO */
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
           // TODO: display success
           this.changePasswordError = false;
           this.currentPassword = null;
           this.newPassword = null;
           this.confirmNewPassword = null;
         })
         .catch((error) => {
           this.changePasswordError = error;
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
