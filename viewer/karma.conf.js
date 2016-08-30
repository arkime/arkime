// Karma configuration
module.exports = function(config) {
  config.set({

    // base path that will be used to resolve all patterns (eg. files, exclude)
    basePath: '',

    // frameworks to use
    // available frameworks: https://npmjs.org/browse/keyword/karma-adapter
    frameworks: ['jasmine'],


    // list of files / patterns to load in the browser
    files: [
      // jquery
      'node_modules/jquery/dist/jquery.js',
      // angular dependencies
      'node_modules/angular/angular.js',
      'node_modules/angular-resource/angular-resource.js',
      'node_modules/angular-route/angular-route.js',
      'node_modules/angular-mocks/angular-mocks.js',
      'node_modules/angular-ui-bootstrap/dist/ui-bootstrap.js',
      'node_modules/angular-animate/angular-animate.js',
      // datatables dependencies
      'node_modules/datatables/media/js/jquery.dataTables.js',
      'node_modules/datatables.net-colreorder/js/dataTables.colReorder.js',
      'node_modules/angular-datatables/dist/angular-datatables.js',
      'node_modules/angular-datatables/dist/plugins/bootstrap/angular-datatables.bootstrap.js',
      'node_modules/angular-datatables/dist/plugins/colreorder/angular-datatables.colreorder.js',

      // app files
      'app/app.js',
      '{app,components}/**/*.js',
      '{app,components}/**/*.html',
      { pattern:'public/*.png', watched: false, included: false, served: true }
    ],


    // list of files to exclude
    exclude: [],


    // preprocess matching files before serving them to the browser
    // available preprocessors: https://npmjs.org/browse/keyword/karma-preprocessor
    preprocessors: {
      '{app,components}/**/*.html': ['ng-html2js']
    },


    ngHtml2JsPreprocessor: {
      stripPrefix: 'app/',
      moduleName: 'templates'
    },


    // test results reporter to use
    // possible values: 'dots', 'progress'
    // available reporters: https://npmjs.org/browse/keyword/karma-reporter
    reporters: ['spec'],


    // web server port
    port: 8008,


    // enable / disable colors in the output (reporters and logs)
    colors: true,


    // level of logging
    // possible values: config.LOG_DISABLE || config.LOG_ERROR
    //    || config.LOG_WARN || config.LOG_INFO || config.LOG_DEBUG
    logLevel: config.LOG_INFO,


    // enable / disable watching file and executing tests whenever any file changes
    autoWatch: true,


    // start these browsers
    // available launchers: https://npmjs.org/browse/keyword/karma-launcher
    browsers: ['Chrome'],


    // Continuous Integration mode
    // if true, Karma captures browsers, runs the tests and exits
    singleRun: false,

    // Concurrency level
    // how many browser should be started simultaneous
    concurrency: Infinity,


    // Configure Spec Reporter
    // https://github.com/mlex/karma-spec-reporter
    specReporter: {
      suppressErrorSummary: true,
      suppressFailed: false,
      suppressPassed: false,
      suppressSkipped: true,
      showSpecTiming: true
    },


    proxies :  {
      '/header_logo.png': '/base/public/header_logo.png'
    }

  })
}
