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
    constructor($interval, UserService, FieldService) {
      this.$interval    = $interval;
      this.UserService  = UserService;
      this.FieldService = FieldService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading  = true;
      this.error    = false;
      this.view     = 'general';

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
           // TODO views error
         });

      this.FieldService.get(true)
        .then((response) => {
          this.fields     = response;
          this.fieldsPlus = response;
          this.fieldsPlus.push({dbField: "ip.dst:port", exp: "ip.dst:port"});
        })
        .catch((error) => {
          // TODO fields error
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
            this.views[name] = null;
            delete this.views[name];
          }
        })
        .catch((err) => {
          // TODO
        });
    }

  }

  SettingsController.$inject = ['$interval', 'UserService', 'FieldService'];

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
