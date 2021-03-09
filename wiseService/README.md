# Arkime WISE

Arkime WISE is a data feed aggregator. External information can get matched with existing Arkime data for better analysis.

Learn more about [WISE](https://arkime.com/wise).

### Install Dependencies

The app uses dependencies that are all bundled and minified using [webpack](https://webpack.js.org) via `npm run build`. This compiles the application into an output directory, in this case `wiseService/vueapp/dist`. This is done automatically when starting the application with `npm start`.

The app uses a number of node.js tools for initialization. You must have node.js and its package manager (npm) installed. You can get them from [node](http://nodejs.org/).

* We get dependencies via `npm`, the [node package manager][npm].

In the app directory, execute:

```
npm install
```

You should find that you have a new folder:

* `node_modules` - contains the npm packages for the dependencies


### Run the Application

#### Production

To start the app for production:
* Move to the top level Arkime directory
* run `npm run wise:start`

This command starts the app. It assumes that you are using the config in `/tests/config.test.ini`. It also bundles the application files into the `wiseService/vueapp/dist` folder.

You can also run the app by building then starting the app. Like so:
* Move to the top level Arkime directory
* run `npm run wise:build`
* Move to the wiseService directory
* run `node wiseService.js -c ./absolute/path/to/wise.ini`

**The parameters are defined as follows:**

| Parameter       | Default | Description |
| --------------- | ------- | ----------- |
| -c, --config    | ./wiseService.ini | Absolute path to the JSON file OR redis source (redis://host:port/dbNum/configKey) that has your wise config rules stored |


_Note: if you do not pass in the arguments, the defaults are used._

Now browse to the app at `http://localhost:8081`, or whichever port you passed into the `npm start` command.

#### Development

To start the app for development and testing:
* Move to the top level Arkime directory
* run `npm run wise:dev`

This command starts the app with the necessary config options set (`-c ../tests/config.test.ini`) and bundles the unminified application files into into the `wiseService/vueapp/dist` folder.

`npm run wise:dev` uses webpack to package the files then watches for changes to relevant files, and re-bundles the app after each save.

Now browse to the app at `http://localhost:8081`.

### Contributing

Check out our [contributing guide](../CONTRIBUTING.md) for more information about contributing to Arkime.

Before submitting a pull request with your contribution, please move to the top level Arkime directory and run `npm run lint`, and correct any errors. This runs [eslint][eslint], a static code analysis tool for finding problematic patterns or code that doesn’t adhere to our style guidelines. Check out `../.eslintrc.js` to view this project's rules.

:octocat: Please use a [fork](https://guides.github.com/activities/forking/) to submit a [pull request](https://help.github.com/articles/creating-a-pull-request/) for your contribution.
