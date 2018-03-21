# Moloch Viewer

Moloch viewer is an [AngularJS][angularjs] and [Vue.js][vuejs] web app.

Read the main [Moloch README](../README.rst) for more information on how to build and run the app for demo or production. These instructions are for running in development mode out of the source tree.

---

## Development

The viewer uses a number of node.js tools for initialization and testing.
You must have node.js and its package manager (npm) installed.
You can get them from [http://nodejs.org/][node].

**Currently, there are two separate web applications: an Angular application and a Vue application. The Angular application includes all pages except the Stats page. This will change as pages are implemented in Vue.**

---

### Install Dependencies

The viewer mostly uses development dependencies that are all bundled using [webpack][webpack].
We get dependencies via `npm`, the [node package manager][npm].

In the viewer directory, execute:

```
npm install
```

You should find that you have a new folder:

* `node_modules` - contains the npm packages for the dependencies

_For now, the Vue app needs to be installed and built separately. This is because the Vue portion of the application is using different versions of some packages (e.g. [Bootstrap 4][bootstrap4] vs [Bootstrap 3][bootstrap3])._

To install dependencies for the Vue application, execute:

```
cd vueapp
npm install
```

---

### Run the Application

**To run the web application, you must have an elasticsearch cluster running and already built and configured Moloch. Read the main [Moloch README](../README.rst) for more information.**


#### The simplest way to start the web app is:


```
npm run start:test
```

For this command to work, your `tests/config.test.ini` must be valid.

This command starts the node server, bundles all Angular app files into `viewer/bundles`, and bundles all Vue app files into `viewer/vueapp/dist`.

Now browse to the app at `http://localhost:8123`.


#### To start the web app with a test admin user, run:

```
npm run addtestuser
npm run start:testuser
```

For this to work, your `tests/config.test.ini` must be valid.

These first command adds an "admin" user. The second command starts the node server, bundles all Angular app files into `viewer/bundles`, and bundles all Vue app files into `viewer/vueapp/dist`.

Now browse to the app at `http://localhost:8123` and login using username "admin" and password "admin".


#### To start the web app for **Vue** development and testing, run:

```
npm run start:vuewatch
```

For this command to work, your `tests/config.test.ini` must be valid.

This command starts the node server, bundles all Vue app files into `viewer/vueapp/dist`, and bundles all Angular app files into `viewer/bundles`.

Webpack watches for changes to relevant Vue files, and re-bundles the Vue app after each save.

Now browse to the app at `http://localhost:8123`.


#### To start the web app for **Angular** development and testing, run:

```
npm run start:ngwatch
```

For this command to work, your `tests/config.test.ini` must be valid.

This command starts the node server, bundles all Angular app files into `viewer/bundles`, and bundles all Vue app files into `viewer/vueapp/dist`.

Webpack watches for changes to relevant Angular files, and re-bundles the Angular app after each save.

Now browse to the app at `http://localhost:8123`.


#### You can also start the app with an existing config file:

```
npm start
```

For this command to work, your `config.ini` must be valid.

This command starts the node server, bundles and minifies all Angular app files into `viewer/bundles`, and bundles and minifies all Vue app files into `viewer/vueapp/dist`.

Now browse to the app at `http://localhost:8123`.

---

### Running Unit Tests

Moloch viewer includes many unit tests. These are written in [Jasmine][jasmine], which are run with the [Karma Test Runner][karma].

* the configuration is found at `viewer/karma.conf.js`
* the unit tests are found near the code they are testing and are named as `*.test.js`.

The easiest way to run the unit tests is to use the supplied npm script:

```
npm test
```

This script will start the Karma test runner to execute the unit tests. Before running the test, the script makes sure that all JavaScript is linted. The tests will not execute if the linter returns errors in the JavaScript.

---

### Contributing

View the [contributing guide](../CONTRIBUTING.md) for more information.

[angularjs]: http://angularjs.org/
[webpack]: https://webpack.github.io/
[jasmine]: http://jasmine.github.io/
[karma]: https://karma-runner.github.io
[node]: https://nodejs.org
[npm]: https://www.npmjs.org/
[vuejs]: https://vuejs.org/
[bootstrap4]: https://getbootstrap.com/
[bootstrap3]: https://getbootstrap.com/docs/3.3/
