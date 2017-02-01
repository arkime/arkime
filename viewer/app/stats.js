(function () {
  'use strict';

  let root = angular.element(document.getElementsByTagName('html'));

  let countWatchers = function() {
    let watchers = [];

    let f = function (element) {
      angular.forEach(['$scope', '$isolateScope'], function (scopeProperty) {
        if (element.data() && element.data().hasOwnProperty(scopeProperty)) {
          angular.forEach(element.data()[scopeProperty].$$watchers, function (watcher) {
            watchers.push(watcher);
          });
        }
      });

      angular.forEach(element.children(), function (childElement) {
        f(angular.element(childElement));
      });
    };

    f(root);

    // Remove duplicate watchers
    let watchersWithoutDuplicates = [];
    angular.forEach(watchers, function(item) {
      if(watchersWithoutDuplicates.indexOf(item) < 0) {
        watchersWithoutDuplicates.push(item);
      }
    });

    // remove on time binding
    let filtered = watchersWithoutDuplicates.filter(function(obj) {
      return ((String(obj.exp)).indexOf('oneTimeWatch') <= 0);
    });

    return(filtered.length);
  };

  console.log(countWatchers());

  setInterval(() => {
    console.log(countWatchers());
  }, 10000);

})();