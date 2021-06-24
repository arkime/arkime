# Arkime Viewer

Arkime viewer is a [Vue.js][vuejs] web app.

Read the main [Arkime README](../README.md) for more information on how to build and run the app for demo or production. These instructions are for running in development mode out of the source tree.

---

## Development

The viewer uses a number of node.js tools for initialization and testing.
You must have node.js and its package manager (npm) installed.
You can get them from [http://nodejs.org/][node].

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

---

### Run the Application

**To run the web application, you must have an elasticsearch cluster running and already built and configured Arkime. Read the main [Arkime README](../README.md) for more information.**

#### The simplest way to start the web app is:

```
npm run start:dev
```

For this command to work, your `tests/config.test.ini` must be valid.

This command adds an admin user (if it doesn't already exists) and starts the node server and bundles all Vue app files into `viewer/vueapp/dist`.

Webpack watches for changes to relevant Vue files, and re-bundles the Vue app after each save.

Now browse to the app at `http://localhost:8123` and login using username "admin" and password "admin".


#### You can run the app as an anonymous user:

```
npm run start:test
```

For this command to work, your `tests/config.test.ini` must be valid.

This command starts the node server and bundles all Vue app files into `viewer/vueapp/dist`.

Webpack watches for changes to relevant Vue files, and re-bundles the Vue app after each save.

Now browse to the app at `http://localhost:8123`.


#### You can also start the app with an existing config file:

```
npm start
```

For this command to work, your `config.ini` must be valid.

This command starts the node server and bundles and minifies all Vue app files into `viewer/vueapp/dist`.

Now browse to the app at `http://localhost:8123`.

---

### Running the UI Tests:

```
npm test
```

The UI tests use [Jest](https://jestjs.io) and [Vue Testing Library](https://testing-library.com/docs/vue-testing-library/intro). 

---

### External File Fixes:

[cubism](https://github.com/square/cubism) has a [bug](https://github.com/square/cubism/issues/16) where it hijacks the entire window's keydown listener. This caused issues with other window keydown listeners throughout the application. The current solution is to remove cubism's window keydown listener entirely. Since cubism is no longer being supported, we have opted to host a minified stable version in the public directory that includes this change.

---

### Contributing

View the [contributing guide](../CONTRIBUTING.md) for more information.

[webpack]: https://webpack.github.io/
[node]: https://nodejs.org
[npm]: https://www.npmjs.org/
[vuejs]: https://vuejs.org/
