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
        var x, y, containerHeight, pxFromRight;
        var elementLocation, elementLeft, elementBottom;

        var docWidth        = $document.width();
        var container       = element.find('.sticky-session-detail-container');

        // if the user gets close to the sticky session detail container
        // exapand the list of sticky session details
        $document.on('mousemove', (e) => {
          elementLocation = element[0].getBoundingClientRect();
          elementLeft     = elementLocation.left;
          elementBottom   = elementLocation.bottom + $window.scrollY;
          
          x = e.pageX, y = e.pageY - $window.scrollY;

          containerHeight = container.height();
          pxFromRight     = docWidth - x;

          // if approaching the top right corner where the sticky sessions are
          if (pxFromRight < (elementLeft + 400) &&
             (y - elementBottom + 150) < containerHeight) {
            container.addClass('open');
          } else {
            container.removeClass('open');
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

        // cleanup
        scope.$on('$destroy', () => {
          $document.off('mousemove');
        });

      }
    };
  }]);

})();
