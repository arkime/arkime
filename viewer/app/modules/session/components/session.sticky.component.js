(function() {

  'use strict';

  /**
   * Sticky Sessions Directive
   * Displays open sessions on the right of the sesions table page
   */
  angular.module('moloch')
  .directive('stickySessions', ['$window', '$document', '$location', '$anchorScroll',
  function($window, $document, $location, $anchorScroll) {
    return {
      scope   : { sessions: '=' },
      template: require('html!../templates/session.sticky.html'),
      link    : function(scope, element, attr) {
        var x, y, docWidth  = $document.width();
        var container       = element.find('.sticky-session-detail-container');

        // if the user gets close to the sticky session detail container
        // exapand the list of sticky session details
        $document.on('mousemove', (e) => {
          x = e.pageX;
          y = e.pageY - $window.scrollY;

          if (x + 100 > docWidth && y > 100 && y < 200) {
            container.addClass('cursor-close');
          } else {
            container.removeClass('cursor-close');
          }
        });

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
        };

        scope.state = { open:false };
        scope.toggleStickySessions = function() {
          scope.state.open = !scope.state.open;
        }


        /* LISTEN! */
        // watch for graph data to change to update the graph
        var oldLength = 0;
        scope.$watchCollection('sessions', (data) => {
          console.log(data);
          var newLength = data.length;
          if (newLength > oldLength) {
            container.removeClass('cursor-close');
            container.addClass('cursor-close');
          }

          oldLength = newLength;
        });


        // cleanup
        scope.$on('$destroy', () => {
          $document.off('mousemove');
        });

      }
    };
  }]);

})();
