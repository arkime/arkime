# Moloch Viewer

Moloch viewer is a [Vue.js][vuejs] web app.

Read the main [Moloch README](../README.rst) for more information on how to build and run the app for demo or production. These instructions are for running in development mode out of the source tree.

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

**To run the web application, you must have an elasticsearch cluster running and already built and configured Moloch. Read the main [Moloch README](../README.rst) for more information.**


#### The simplest way to start the web app is:


```
npm run start:test
```

For this command to work, your `tests/config.test.ini` must be valid.

This command starts the node server and bundles all Vue app files into `viewer/vueapp/dist`.

Webpack watches for changes to relevant Vue files, and re-bundles the Vue app after each save.

Now browse to the app at `http://localhost:8123`.


#### To start the web app with a test admin user, run:

```
npm run addtestuser
npm run start:testuser
```

For this to work, your `tests/config.test.ini` must be valid.

These first command adds an "admin" user. The second command starts the node server and bundles all Vue app files into `viewer/vueapp/dist`.

Webpack watches for changes to relevant Vue files, and re-bundles the Vue app after each save.

Now browse to the app at `http://localhost:8123` and login using username "admin" and password "admin".


#### You can also start the app with an existing config file:

```
npm start
```

For this command to work, your `config.ini` must be valid.

This command starts the node server and bundles and minifies all Vue app files into `viewer/vueapp/dist`.

Now browse to the app at `http://localhost:8123`.

---

### Contributing

View the [contributing guide](../CONTRIBUTING.md) for more information.

[webpack]: https://webpack.github.io/
[node]: https://nodejs.org
[npm]: https://www.npmjs.org/
[vuejs]: https://vuejs.org/
