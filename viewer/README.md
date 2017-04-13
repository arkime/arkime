# Moloch Viewer

Moloch viewer is an [AngularJS][angularjs] web app.

Read the main Moloch README for more information on how to build and run the app
for demo or production. These instructions are for running in development mode out
of the source tree.


## Development

The viewer uses a number of node.js tools for initialization and testing.
You must have node.js and its package manager (npm) installed.
You can get them from [http://nodejs.org/][node].


### Install Dependencies

The viewer mostly uses development dependencies that are all bundled using
[webpack][webpack].

* We get dependencies via `npm`, the [node package manager][npm].

In the viewer directory, execute:

```
npm install
```

You should find that you have a new folder:

* `node_modules` - contains the npm packages for the dependencies


### Run the Application

The simplest way to start the web app for development and testing is:

```
npm run start:test
```

You must have an elasticsearch cluster running, have already built and
configured Moloch, and your `tests/config.test.ini` must be valid.

This command starts the node server and bundles all app files into
`viewer/bundles/app.bundle.js` and `viewer/bundles/vendor.bundle.js`.

Webpack watches for changes to relevant files, and re-bundles the app after each save.

Now browse to the app at `http://localhost:8123`.

---

You can also start the app with an existing `config.ini` file:

```
npm start
```

As above, you must have an elasticsearch cluster running, have already built and
configured Moloch, and your `config.ini` must be valid.

This command starts the node server and bundles and minifies all app files into
`viewer/bundles/app.bundle.js` and `viewer/bundles/vendor.bundle.js`.

Now browse to the app at `http://localhost:8123`.

---

Lastly, you can start the app without test data by creating `viewer/config.dev.ini`,
then executing:

```
npm run start:dev
```

As above, you must have an elasticsearch cluster running, have already built and
configured Moloch, and your `viewer/config.dev.ini` must be valid.

This command starts the node server and bundles all app files into
`viewer/bundles/app.bundle.js` and `viewer/bundles/vendor.bundle.js`.

Webpack watches for changes to relevant files, and re-bundles the app after each save.

Now browse to the app at `http://localhost:8123`.


### Running Unit Tests

Moloch viewer includes many unit tests. These are written in [Jasmine][jasmine],
which are run with the [Karma Test Runner][karma].

* the configuration is found at `viewer/karma.conf.js`
* the unit tests are found near the code they are testing and are named as `*.test.js`.

The easiest way to run the unit tests is to use the supplied npm script:

```
npm test
```

This script will start the Karma test runner to execute the unit tests. Before
running the test, the script makes sure that all JavaScript is linted. The tests
will not execute if the linter returns errors in the JavaScript.


### Directory Layout

```
app/                    --> all of the source files for the application
  app.js                --> main application module
  app.css               --> main stylesheet - imports other stylesheets
  modules/              --> all app specific modules
    index.js              --> webpack entry file listing all components to be included in bundle
    index.test.js         --> webpack test entry file listing all tests to be included in testing bundle
    module1/                --> module1 logic, views, styles, and tests
      components/           --> logic controllers for views
      services/             --> services to interact with the server
      styles/               --> styles that pertain to this module's views
      templates/            --> html views
      tests/                --> jasmine test files
  bundle/               --> where webpack stores the app bundles
    app.bundle.js         --> main app bundle
    vendor.bundle.js      --> bundled dependencies
    app.bundle.js.map     --> main app map file for debugging
    vendor.bundle.js.map  --> dependencies map file for debugging
karma.conf.js         --> config file for running unit tests with Karma
webpack.config.js     --> config file for webpack to bundle files
webpack.loaders.js    --> config file for webpack to initialize loaders for different types of files
components/           --> reusable small components
  index.js              --> webpack entry file listing all components to be included in bundle
  index.test.js         --> webpack test entry file listing all tests to be included in testing bundle
```

[angularjs]: http://angularjs.org/
[webpack]: https://webpack.github.io/
[jasmine]: http://jasmine.github.io/
[karma]: https://karma-runner.github.io
[node]: https://nodejs.org
[npm]: https://www.npmjs.org/
