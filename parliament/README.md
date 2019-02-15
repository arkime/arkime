# Moloch Parliament

Moloch Parliament is an [Vue.js][vue] web app to view and monitor multiple Moloch clusters.

This project was generated with [Vue CLI][vuecli].

## What is Parliament and how can I use it?

The Parliament dashboard contains a grouped list of your Moloch clusters with links, ES health, and issues for each. You can search for Molochs in your Parliament, change the data refresh time (15 seconds is the default), and hover over issues and ES health statuses for more information.

The app can be run in three ways: 
1. with a password
2. read only mode (without a password, but it can be configured later)
3. dashboard only mode (no password or ability to configure one)

_**If your Parliament has a password (via option 1 or 2), you can interact with it in the ways enumerated below.**_

#### Parliament Page
The main Parliament page allows a user to view and interact with the Molochs in your Parliament. Once logged in, a user can acknowledge and ignore issues for each cluster as well as update the Parliament when in **Edit Mode**. To enter this mode, toggle the switch on the top right (below the navbar). Now you can add, update, delete, or reorder groups and clusters in your Parliament.

#### Issues Page
The issues page contains a list of issues that your Parliament is experiencing. Here, you can ignore, acknowledge, and remove acknowledged issues. 

Acknowledged issues will not show up on the main Parliament page, but will remain here (but grayed out) to be removed (via the trashcan button or waiting 15 minutes for them to be removed automatically).

Ignored issues will not show up on the main Parliament page, but will remain here (but grayed out) to be unignored (via the ignore dropdown button or automatically after the set ignore time has expired).

#### Settings Page
The settings page has 3 sections as described below:

**General:** this section has a few settings that pertain to issues in your Parliament.
1. The `capture nodes must check in this often` setting controls how behind a node's cluster's timestamp can be from the current time. If the timestamp exceeds this time setting, an `Out Of Date` issue is added to the cluster. _The default for this setting is 30 seconds._
2. The `Elasticsearch query timeout` setting controls the maxiumum Elasticsearch status query duration. If the query exceeds this time setting, an `ES Down` issue is added to the cluster. _The default for this setting is 5 seconds._
3. The `Low Packets Threshold` setting controls the minumum number of packets that the capture node must receive. If the capture node is not receiving enough packets, a `Low Packets` issue is added to the cluster. You can set this value to `-1` to ignore this issue altogether. This setting also includes a time range for how long this problem must persist before adding an issue to the cluster. _The default for this setting is 0 packets for 10 seconds._
4. The `remove all issues after` setting controls when an issue is removed if it has not occurred again. The issue is removed from the cluster after this time expires as long as the issue has not occurred again. _The default for this setting is 60 minutes._
5. The `remove acknowledged issues after` setting controls when an acknowledged issue is removed. The issue is removed from the cluster after this time expires (so you don't have to remove issues manually with the trashcan button). _The default for this setting is 15 minutes._

**Password:** this section allows a user to update the Parliament password or create a new password if the Parliament was started without one.

**Notifiers:** this section provides the ability to configure alerts for your Parliament. Users can be alerted via:
1. Slack
2. Twilio
3. Email

Each different notifier can alert on different types of issues.


## Running from RPM/DEB
If using prepackaged version of Moloch, use "Configure --parliament" to setup.  It will use port 8008 by default.

It is meant to sit behind a reverse proxy such as apache, with config like the following added.
```
ProxyPassMatch   ^/$ http://localhost:8008/parliament retry=0
ProxyPass        /parliament/ http://localhost:8008/parliament/ retry=0
```


### Install Dependencies

The app uses dependencies that are all bundled and minified using [webpack][webpack] via `npm run build`. This compiles the application into an output directory, in this case `parliament/vueapp/dist`. This is done automatically when starting the application with `npm start`.

The app uses a number of node.js tools for initialization. You must have node.js and its package manager (npm) installed. You can get them from [http://nodejs.org/][node].

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
npm start -s -- --pass somepassword --port 8765 -c ./absolute/path/to/parliament.json --key ./absolute/path/to/keyFile.pem --cert ./absolute/path/to/certFile.pem
```
This command starts the app, passing in the password, port, config file location, and key and cert file locations. It also bundles the application files into the `parliament/vueapp/dist` folder.

_**Important**: when using `npm start` the leading `--`, before the parameters is essential._

You can also run the app by building then starting the app. Like so:
```
npm run build
```
```
node server.js --pass somepassword --port 8765 -c ./absolute/path/to/parliament.json --key ./absolute/path/to/keyFile.pem --cert ./absolute/path/to/certFile.pem
```

**The parameters are defined as follows:**

| Parameter       | Default | Description |
| --------------- | ------- | ----------- |
| --pass          | EMPTY   | Password will be used to login to update the parliament. If it is not set, the app runs in read only mode. **IMPORTANT:** passing in a password will overwrite any password already configured in your parliament. You can always configure a password later in the UI. |
| --port          | 8008    | Port for the web app to listen on. |
| -c, --config    | ./parliament.json | Absolute path to the JSON file to store your parliament information. |
| --key           | EMPTY   | Private certificate to use for https, if not set then http will be used. **certfile** must also be set. |
| --cert          | EMPTY   | Public certificate to use for https, if not set then http will be used. **keyFile** must also be set. |
| --dashboardOnly | EMPTY   | This flag runs the parliament in dashboard only mode. A user will not be allowed to configure settings, a password, or interact with the parliament via the UI. |

_Note: if you do not pass in the port or file arguments, the defaults are used._

Now browse to the app at `http://localhost:8765`, or whichever port you passed into the `npm start` command.

To login, use the password that you passed into the `npm start` command. If you did not supply a password, you can view the parliament in read only mode or configure one by navigating to the settings page.

#### Development

To start the app for development and testing, simply run:
```
npm run dev
```

This command starts the app with the necessary config options set (`--pass admin --port 8008 -c ./parliament.dev.json`) and bundles the unminified application files into into the `parliament/vueapp/dist` folder.

`npm run dev` uses webpack to package the files then watches for changes to relevant files, and re-bundles the app after each save.

Now browse to the app at `http://localhost:8008`.

To login, use the password, 'admin'.

#### Further help with running the application

For a detailed explanation on how things work, check out the [vue webpack guide](http://vuejs-templates.github.io/webpack/) and [docs for vue-loader](http://vuejs.github.io/vue-loader).


### Contributing

Check out our [contributing guide](../CONTRIBUTING.md) for more information about contributing to Moloch.

Before submitting a pull request with your contribution, please run `npm run lint`, and correct any errors. This runs [eslint][eslint], a static code analysis tool for finding problematic patterns or code that doesn’t adhere to our style guidelines. Check out `parliament/.eslintrc.js` to view this project's rules.

:octocat: Please use a fork to submit a [pull request](https://help.github.com/articles/creating-a-pull-request/) for your contribution.


### Parliament Definition
`parliament.json` (or whatever you pass into the -c config option when starting Parliament) is the file that describes your parliament. You can create this by hand or use the Parliament UI to create, edit, and delete groups and clusters. View the supplied `parliament.example.json` to view an example parliament configuration.

### Issues
`parliament.issues.json` will be created to store issues pertaining to the clusters in your parliament.

##### Parliament model:
```javascript
{                   // parliament object
  version: x,       // version (number)
  password: 'hash', // hashed password
  groups: [ ... ],  // list of groups in the parliament
  settings: {       // parliament settings
    general: {      // general settings

      // capture nodes need to check in at least this often (number of seconds)
      // if a capture node has not checked in, an Out Of Date issue will be added to the node's cluster
      outOfDate: 30,

      // Elasticsearch query timeout (number of seconds)
      // Aborts the queries and adds an ES Down issue if no response is received
      esQueryTimeout: 5,

      // Remove all issues after (number of minutes)
      // Removes issues that have not been seen again after the specified time
      removeIssuesAfter: 60,

      // Remove acknowledged issues after (number of minutes)
      // Removes acknowledged issues that have not been seen again after the specified time
      removeAcknowledgedAfter: 15

    },
    notifiers: {    // notifiers (defined in parliament/notifiers/provider.notifme.js)
      notifierX: {  // notifier (object)

        // name of the notifier displayed in the UI (string)
        name: 'slack',

        // turns on/off this notifier (boolean)
        on: false,

        // fields necessary to notify via this notifier (object)
        // (defined in parliament/notifiers/provider.notifme.js)
        fields: {},

        // which issues to alert on via this notifier (object)
        alerts: {}

      }
    }
  }
}
```
**Note:** The password is hashed using [bcrypt][bcrypt].

##### Group model:
```javascript
{                                   // group object
  title: 'Group Title',             // group title (string, *required)
  description: 'Group description', // group description (string)
  clusters: [ ... ]                 // list of clusters in the group
}
```
##### Cluster model:
```javascript
{ // cluster object

  // cluster title (string, *required)
  title: 'Cluster title',

  // cluster description (string)
  description: 'Cluster description',

  // cluster external url for links in the UI (string, *required)
  url: 'https://somewhere.com',

  // cluster local url for fetching health/stats data (string, defaults to url if not supplied)
  localUrl: 'https://localhost:port',
  
  // which type of cluster this is. types include:
  // noAlerts - no alerts, stats, health, link to cluster
  // mutiviewer - no alerts, no stats, health, link to cluster
  // disabled - no alets, no stats, no health, no link to cluster
  // (defaults to undefined)
  type: 'multiviewer',

  // whether to hide delta bytes per second stats (defaults to false)
  hideDeltaBPS: false,

  // whether to hide delta packet drops per second (defaults to false)
  hideDeltaTDPS: false,

  // whether to hide number of nodes (defaults to false)
  hideDataNodes: false,

  // whether to hide the total number of nodes (defaults to false)
  hideTotalNodes: false

}
```
##### Issue model:
```javascript
{ // issue object

  // the type of issue: esDown, esRed, esDropped, outOfDate, or noPackets (string)
  type: 'esDown',

  // the specific error encountered (string)
  value: 'Error: Issue Error',

  // human readable text to describe the type of issue (string)
  text: 'ES is down',

  // human readable title to be displayed in the UI instead of type (string)
  title: 'ES Down',

  // how severe the issue is: red or yellow (string)
  severity: 'red',

  // the ID of the cluster that the issue pertains to (string)
  clusterId: '1',

  // more verbose info to be displayed in the UI (string)
  // concatenation of issue title and value
  message: 'ES is down: Error: Issue error',

  // time that the issue was first noticed in ms (number)
  firstNoticed: 1234567890,

  // time that the issue was last noticed in ms (number)
  lastNoticed: 1234567890,

  // time that parliament issued an alert in ms (number)
  alerted: 1234567890,

  // time that the issue was acknowledged by a user in ms (number)
  acknowledged: 1234567890,

  // time that the issue will be ignored until in ms (number)
  // once the current time has passed this value, the issue will alert again
  ignoreUntil: 1234567890
}
```

[vue]: https://vuejs.org/
[vuecli]: https://cli.vuejs.org/
[webpack]: https://webpack.github.io/
[node]: https://nodejs.org
[npm]: https://www.npmjs.org/
[eslint]: https://eslint.org/
[bcrypt]: https://github.com/kelektiv/node.bcrypt.js#readme
