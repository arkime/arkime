(function() {

  'use strict';

  /**
   * Sticky Sessions Directive
   * Displays open sessions on the right of the sesions table page
   */
  angular.module('moloch')
  .directive('stickySessions', ['$window', '$document', '$location', '$anchorScroll', '$timeout',
  function($window, $document, $location, $anchorScroll, $timeout) {
    return {
      scope   : { sessions: '=' },
      template: require('html!../templates/session.sticky.html'),
      link    : function(scope, element, attr) {

        /* exposed functions ----------------------------------------------- */
        /**
         * Scrolls to specified session
         * @param {Object} event  The click event that initiated scrollTo
         * @param {string} id     The id of the sessino to scroll to
         */
        scope.scrollTo = function(event, id) {
          event.preventDefault();

          var old = $location.hash();
          $location.hash('session' + id);
          $anchorScroll();

          // reset to old to keep any additional routing logic from kicking in
          $location.hash(old);
        };

        /**
         * Closes the display of the session detail for the specified session
         * @param {Object} session The session to collapse details
         */
        scope.closeSessionDetail = function(session) {
          session.expanded = false;

          var index = scope.sessions.indexOf(session);
          if (index >= 0) { scope.sessions.splice(index, 1); }

          if (!scope.sessions || scope.sessions.length <= 0) {
            scope.state.open = false;
          }
        };

        /* Closes all the open sessions and the panel */
        scope.closeAll = function() {
          for (var i = 0, len = scope.sessions.length; i < len; ++i) {
            scope.sessions[i].expanded = false;
          }

          scope.sessions    = [];
          scope.state.open  = false;
        };

        /* Opens/closes the opened sessions panel */
        scope.state = { open:false };
        scope.toggleStickySessions = function() {
          scope.state.open = !scope.state.open;
        };

        /* Sorting */
        scope.sortOrder = 'desc';
        /* Orders the sessions by start or stop time
         * Triggered when sortBy/sortOrder is changed and when a session
         * is added to the sticky sessions list*/
        scope.sort = function() {
          if (scope.sortBy) {
            scope.sessions = scope.sessions.sort(function (a, b) {
              let result;
              if (scope.sortOrder === 'desc') {
                result = b[scope.sortBy] - a[scope.sortBy];
              } else {
                result = a[scope.sortBy] - b[scope.sortBy];
              }
              return result;
            });
          }
        };
        /* Orders the sessions ascending or descending
         * Triggered when sortOrder is changed */
        scope.toggleSortOrder = function() {
          if (scope.sortOrder === 'asc') {
            scope.sortOrder = 'desc';
          } else {
            scope.sortOrder = 'asc';
          }

          scope.sort();
        };


        /* LISTEN! */
        // watch for session array to change -> bounces button
        var oldLength = 0;
        scope.$watchCollection('sessions', (data) => {
          var newLength = data.length;
          if (newLength > oldLength) {
            element.removeClass('bounce');

            $timeout(() => {
              element.addClass('bounce');
            });

            $timeout(() => {
              element.removeClass('bounce');
            }, 1000);

            scope.sort();
          } else {
            if (!scope.sessions || scope.sessions.length <= 0) {
              scope.state.open = false;
            }
          }

          oldLength = newLength;
        });

      }
    };
  }]);

})();
