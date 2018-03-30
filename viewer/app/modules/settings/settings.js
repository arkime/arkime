(function() {

  'use strict';

  let customCols = require('../session/components/custom.columns.json');

  let bodyElem = $(document.body);

  let interval;

  const defaultSpiviewConfig = { fields: ['dstIp','protocol','srcIp'] };
  const defaultColConfig = {
    order   : [['firstPacket', 'asc']],
    columns : ['firstPacket','lastPacket','src','srcPort','dst','dstPort','totPackets','dbby','node','info']
  };

  /**
   * @class SettingsController
   * @classdesc Interacts with moloch settings page
   * @example
   * '<moloch-settings></moloch-settings>'
   */
  class SettingsController {

    /**
     * Initialize global variables for this controller
     * @param $window         Angular's reference to the browser's window object
     * @param $document       Angular's jQuery wrapper for window.document object
     * @param $interval       Angular's wrapper for window.setInterval
     * @param $location       Exposes browser address bar URL (window.location)
     * @param $routeParams    Retrieves the current set of route parameters
     * @param UserService     Transacts users and user data with the server
     * @param FieldService    Transacts fields with the server
     * @param ConfigService   Transacts app configurations with the server
     * @param SessionService  Transacts sessions with the server
     *
     * @ngInject
     */
    constructor($window, $document, $interval, $location, $routeParams,
                UserService, FieldService, ConfigService, SessionService) {
      this.$window        = $window;
      this.$document      = $document;
      this.$interval      = $interval;
      this.$location      = $location;
      this.$routeParams   = $routeParams;
      this.UserService    = UserService;
      this.FieldService   = FieldService;
      this.ConfigService  = ConfigService;
      this.SessionService = SessionService;
    }

    /* Callback when component is mounted and ready */
    $onInit() {
      this.loading  = true;
      this.error    = false;

      this.visibleTab = 'general'; // default tab

      this.defaultColConfig = defaultColConfig;
      this.defaultSpiviewConfig = defaultSpiviewConfig;

      // does the url specify a tab in hash
      let tab = this.$location.hash();
      if (tab) { // if there is a tab specified and it's a valid tab
        if (tab === 'general' || tab === 'views' || tab === 'cron' ||
            tab === 'col' || tab === 'theme' || tab === 'password' ||
            tab === 'spiview') {
          this.visibleTab = tab;
        }
      }

      this.newCronQueryProcess  = '0';
      this.newCronQueryAction   = 'tag';

      this.getThemeColors();
      this.themeDisplays = [
        { name: 'Purp-purp', class: 'default-theme' },
        { name: 'Blue', class: 'blue-theme' },
        { name: 'Green', class: 'green-theme' },
        { name: 'Cotton Candy', class: 'cotton-candy-theme' },
        { name: 'Green on Black', class: 'dark-2-theme' },
        { name: 'Dark Blue', class: 'dark-3-theme' }
      ];

      this.UserService.getCurrent()
        .then((response) => {
          this.displayName = response.userId;
          // only admins can edit other users' settings
          if (response.createEnabled && this.$routeParams.userId) {
            if (response.userId === this.$routeParams.userId) {
              // admin editing their own user so the routeParam is unnecessary
              this.$location.search('userId', null);
            } else { // admin editing another user
              this.userId = this.$routeParams.userId;
              this.displayName = this.$routeParams.userId;
            }
          } else { // normal user has no permission, so remove the routeParam
            // (even if it's their own userId because it's unnecessary)
            this.$location.search('userId', null);
          }

          // always get the user's settings because current user is cached
          // so response.settings might be stale
          this.getSettings();

          // get all the other things!
          this.getViews();
          this.getCronQueries();
          this.getColConfigs();
          this.getSpiviewConfigs();
        })
        .catch((error) => {
          this.error    = error.text;
          this.loading  = false;
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

          // add custom columns to the fields array
          for (let key in customCols) {
            if (customCols.hasOwnProperty(key)) {
              this.fields.push(customCols[key]);
            }
          }

          // build fields map for quick lookup by dbField
          this.fieldsMap = {};
          for (let i = 0, len = this.fields.length; i < len; ++i) {
            let field = this.fields[i];
            this.fieldsMap[field.dbField] = field;
          }

          this.SessionService.getState('sessionsNew')
             .then((response) => {
               this.setupColumns(response.data.visibleHeaders);
               // if the sort column setting does not match any of the visible
               // headers, set the sort column setting to last
               if (response.data.visibleHeaders.indexOf(this.settings.sortColumn === -1)) {
                 this.settings.sortColumn = 'last';
               }
             })
             .catch(() => {
               this.setupColumns(['firstPacket','lastPacket','src','srcPort','dst','dstPort','totPackets','dbby','node','info']);
             });
        });
    }

    /* fired when controller's containing scope is destroyed */
    $onDestroy() {
      if (interval) { this.$interval.cancel(interval); }
    }


    /* service functions --------------------------------------------------- */
    /* retrieves the specified user's settings */
    getSettings() {
      this.UserService.getSettings(this.userId)
        .then((response) => {
          this.settings = response;
          this.loading  = false;

          this.setTheme();

          this.startClock();
        })
        .catch((error) => {
          this.loading      = false;
          this.error        = error.data.text;
          this.displayName  = '';
        });
    }

    /* retrieves the specified user's views */
    getViews() {
      this.UserService.getViews(this.userId)
        .then((response) => {
          this.views = response;
        })
        .catch((error) => {
          this.viewListError = error.text;
        });
    }

    /* retrieves the specified user's cron queries */
    getCronQueries() {
      this.UserService.getCronQueries(this.userId)
        .then((response) => {
          this.cronQueries = response;
        })
        .catch((error) => {
          this.cronQueryListError = error.text;
        });
    }

    /* retrieves the specified user's custom column configurations */
    getColConfigs() {
      this.UserService.getColumnConfigs(this.userId)
        .then((response) => {
          this.colConfigs = response;
        })
        .catch((error) => {
          this.colConfigError = error.text;
        });
    }

    /* retrieves the specified user's custom spiview fields configurations.
     * dissects the visible spiview fields for view consumption */
    getSpiviewConfigs() {
      this.UserService.getSpiviewFields(this.userId)
         .then((response) => {
           this.spiviewConfigs = response;

           for (let x = 0, xlen = this.spiviewConfigs.length; x < xlen; ++x) {
             let config = this.spiviewConfigs[x];
             let spiParamsArray = config.fields.split(',');

             // get each field from the spi query parameter and issue
             // a query for one field at a time
             for (let i = 0, len = spiParamsArray.length; i < len; ++i) {
               let param = spiParamsArray[i];
               let split = param.split(':');
               let fieldID = split[0];
               let count = split[1];

               let field;

               for (let key in this.fields) {
                 if (this.fields[key].dbField === fieldID) {
                   field = this.fields[key];
                   break;
                 }
               }

               if (field) {
                 if (!config.fieldObjs) { config.fieldObjs = []; }

                 field.count = count;
                 config.fieldObjs.push(field);
               }
             }
           }
         })
         .catch((error) => {
           this.spiviewConfigError = error.text;
         });
    }



    /* page functions ------------------------------------------------------ */
    /* opens a specific settings tab */
    openView(tabName) {
      this.visibleTab = tabName;

      this.$location.hash(tabName);
    }

    /* remove the message when user is done with it or duration ends */
    messageDone() {
      this.msg = null;
      this.msgType = null;
    }

    /* starts the clock for the timezone setting */
    startClock() {
      this.tick();
      interval = this.$interval(() => { this.tick(); }, 1000);
    }


    /* GENERAL ------------------------------------------------------------- */
    /**
     * saves the user's settings and displays a message
     * @param updateTheme whether to update the UI theme
     */
    update(updateTheme) {
      this.UserService.saveSettings(this.settings, this.userId)
        .then((response) => {
          // display success message to user
          this.msg = response.text;
          this.msgType = 'success';

          if (updateTheme) {
            let now = Date.now();
            if ($('link[href^="user.css"]').length) {
              $('link[href^="user.css"]').remove();
            }
            $('head').append(`<link rel="stylesheet"
                              href="user.css?v${now}" type="text/css" />`);
          }
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

    /**
     * Setup this.columns with a list of field objects
     * @param {array} colIdArray The array of column ids
     */
    setupColumns(colIdArray) {
      this.columns = [];
      for (let i = 0, len = colIdArray.length; i < len; ++i) {
        this.columns.push(this.getField(colIdArray[i]));
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

      this.UserService.createView(data, this.userId)
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
     * Deletes a view given its name
     * @param {string} name The name of the view to delete
     */
    deleteView(name) {
      this.UserService.deleteView(name, this.userId)
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
      this.UserService.getViews(this.userId)
        .then((response) => {
          this.views[key] = response[key];
        })
        .catch((error) => {
          this.viewListError = error.text;
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

      this.UserService.updateView(data, this.userId)
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

      this.UserService.createCronQuery(data, this.userId)
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
     * Deletes a cron query given its key
     * @param {string} key The cron query's key
     */
    deleteCronQuery(key) {
      this.UserService.deleteCronQuery(key, this.userId)
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
      this.UserService.getCronQueries(this.userId)
        .then((response) => {
          this.cronQueries[key] = response[key];
        })
        .catch((error) => {
          this.cronQueryListError = error.text;
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

      this.UserService.updateCronQuery(data, this.userId)
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


    /* THEMES -------------------------------------------------------------- */
    setTheme() {
      // default to default theme if the user has not set a theme
      if (!this.settings.theme) { this.settings.theme = 'default-theme'; }
      if (this.settings.theme.startsWith('custom')) {
        this.settings.theme = 'custom-theme';
        this.creatingCustom = true;
      }
    }

    /* changes the ui theme (picked from existing themes) */
    changeTheme() {
      bodyElem.removeClass();
      bodyElem.addClass(this.settings.theme);

      this.update();

      this.getThemeColors();
    }

    /* changes a color value of a custom theme and applies the theme */
    changeColor() {
      bodyElem.removeClass();
      bodyElem.addClass('custom-theme');

      this.setThemeString();

      this.settings.theme = `custom1:${this.themeString}`;

      this.update(true);
    }

    /* retrievs the theme colors from the document body's property values */
    getThemeColors() {
      let styles  = this.$window.getComputedStyle(this.$document[0].body);

      this.background       = styles.getPropertyValue('--color-background').trim() || '#FFFFFF';
      this.foreground       = styles.getPropertyValue('--color-foreground').trim() || '#333333';
      this.foregroundAccent = styles.getPropertyValue('--color-foreground-accent').trim();

      this.primary = styles.getPropertyValue('--color-primary').trim();
      this.primaryLightest = styles.getPropertyValue('--color-primary-lightest').trim();

      this.secondary = styles.getPropertyValue('--color-secondary').trim();
      this.secondaryLightest = styles.getPropertyValue('--color-secondary-lightest').trim();

      this.tertiary = styles.getPropertyValue('--color-tertiary').trim();
      this.tertiaryLightest = styles.getPropertyValue('--color-tertiary-lightest').trim();

      this.quaternary = styles.getPropertyValue('--color-quaternary').trim();
      this.quaternaryLightest = styles.getPropertyValue('--color-quaternary-lightest').trim();

      this.water  = styles.getPropertyValue('--color-water').trim();
      this.land   = styles.getPropertyValue('--color-land').trim() || this.primary;

      this.src = styles.getPropertyValue('--color-src').trim() || '#CA0404';
      this.dst = styles.getPropertyValue('--color-dst').trim() || '#0000FF';

      this.setThemeString();
    }

    updateThemeString() {
      let colors = this.themeString.split(',');

      this.background = colors[0];
      this.foreground = colors[1];
      this.foregroundAccent = colors[2];

      this.primary = colors[3];
      this.primaryLightest = colors[4];

      this.secondary = colors[5];
      this.secondaryLightest = colors[6];

      this.tertiary = colors[7];
      this.tertiaryLightest = colors[8];

      this.quaternary = colors[9];
      this.quaternaryLightest = colors[10];

      this.water = colors[11];
      this.land = colors[12];

      this.src = colors[13];
      this.dst = colors[14];

      this.changeColor();
    }

    setThemeString() {
      this.themeString = `${this.background},${this.foreground},${this.foregroundAccent},${this.primary},${this.primaryLightest},${this.secondary},${this.secondaryLightest},${this.tertiary},${this.tertiaryLightest},${this.quaternary},${this.quaternaryLightest},${this.water},${this.land},${this.src},${this.dst}`;
    }


    /* COLUMN CONFIGURATIONS ----------------------------------------------- */
    /**
     * Deletes a previously saved custom column configuration
     * @param {string} name The name of the column config to remove
     * @param {int} index   The index in the array of the column config to remove
     */
    deleteColConfig(name, index) {
      this.UserService.deleteColumnConfig(name, this.userId)
        .then((response) => {
          this.colConfigs.splice(index, 1);
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


    /* SPIVIEW FIELD CONFIGURATIONS ----------------------------------------- */
    /**
     * Deletes a previously saved custom spiview field configuration
     * @param {string} name The name of the field config to remove
     * @param {int} index   The index in the array of the field config to remove
     */
    deleteSpiviewConfig(name, index) {
      this.UserService.deleteSpiviewFieldConfig(name, this.userId)
         .then((response) => {
           this.spiviewConfigs.splice(index, 1);
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
      if (!this.userId && (!this.currentPassword || this.currentPassword === '')) {
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

      let data = {
        newPassword     : this.newPassword,
        currentPassword : this.currentPassword
      };

      this.UserService.changePassword(data, this.userId)
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

  SettingsController.$inject = ['$window','$document','$interval','$location',
    '$routeParams','UserService','FieldService','ConfigService','SessionService'];

  /**
   * ES Health Directive
   * Displays elasticsearch health status
   */
  angular.module('moloch')
     .component('molochSettings', {
       template  : require('./settings.html'),
       controller: SettingsController
     });

})();
