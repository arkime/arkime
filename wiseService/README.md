# Moloch Wise

Moloch Wise is a data feed aggregator. External information can get matched with existing moloch data for better analysis.

Learn more about [Wise](https://molo.ch/wise).

### Install Dependencies

The app uses dependencies that are all bundled and minified using [webpack][webpack] via `npm run build`. This compiles the application into an output directory, in this case `wise/vueapp/dist`. This is done automatically when starting the application with `npm start`.

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

To start the app for production, simply run:
```
npm start
```
This command starts the app. It also bundles the application files into the `wise/vueapp/dist` folder.

You can also run the app by building then starting the app. Like so:
```
npm run build
node wiseService.js -c ./absolute/path/to/wise.json
```

**The parameters are defined as follows:**

| Parameter       | Default | Description |
| --------------- | ------- | ----------- |
| -c, --config    | ./wiseService.ini | Absolute path to the JSON file OR redis source (redis://host:port/dbNum/configKey) that has your wise config rules stored |


_Note: if you do not pass in the arguments, the defaults are used._

Now browse to the app at `http://localhost:8081`, or whichever port you passed into the `npm start` command.

#### Development

To start the app for development and testing, simply run:
```
npm run dev
```

This command starts the app with the necessary config options set (`-c ../tests/config.test.json`) and bundles the unminified application files into into the `wise/vueapp/dist` folder.

`npm run dev` uses webpack to package the files then watches for changes to relevant files, and re-bundles the app after each save.

Now browse to the app at `http://localhost:8081`.

### Contributing

Check out our [contributing guide](../CONTRIBUTING.md) for more information about contributing to Moloch.

Before submitting a pull request with your contribution, please run `npm run lint`, and correct any errors. This runs [eslint][eslint], a static code analysis tool for finding problematic patterns or code that doesn’t adhere to our style guidelines. Check out `../.eslintrc.js` to view this project's rules.

:octocat: Please use a fork to submit a [pull request](https://help.github.com/articles/creating-a-pull-request/) for your contribution.
