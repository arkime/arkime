# Contributing to Arkime

:sparkles: Glad to see you here! :sparkles:

---

### Just have a question :question:

* Visit our [FAQs](https://arkime.com/faq)
* Or talk to us directly in the [Arkime Slack](https://slackinvite.arkime.com/)

---

### Where do I start? :traffic_light:

First, checkout the main [Arkime README](README.md) for information on how to build and run Arkime. We do all development on MacBook Pros.

**Then, get some test data!**

* If using a VM/docker make sure your dev host has at least 2-3G of memory
* Make sure `node` is in your path, currently main supports Node version 18 or 20 (Use the latest version in "Node Versions" section of [CHANGELOG](CHANGELOG)), we recommend using [nvm](https://github.com/nvm-sh/nvm) to manage what version of node is installed
* Install [OpenSearch](https://opensearch.org/downloads.html) OR [Elasticsearch](https://www.elastic.co/downloads/past-releases#elasticsearch) (Use the latest version in "OpenSearch Versions" or "Elasticsearch versions" section of [CHANGELOG](CHANGELOG))
* Start OpenSearch/Elasticsearch
* If on a Mac install either [Homebrew](https://brew.sh) or [MacPorts](https://www.macports.org/)
* Run `./easybutton-build.sh`
* Run `make check` from the top level directory, this will
  * run `npm ci` everywhere (to install all the necessary dependencies)
  * run `tests.pl` and `tests.pl --viewer` in the tests directory (to load PCAPs)

> **Note:** this will only work if viewer is not already running.

You should now have test data loaded, so let's **start the web app**:

* Run `npm run viewer:test`
* Now browse to the app at `http://localhost:8123`

If you want to run Arkime in non-anonymous mode:
* Move to the top level Arkime directory
* run `npm run viewer:dev` this will automatically add an admin user (username: admin, password: admin)

> :clock1: _On first load, you will likely see this message: "No results or none that match your search within your time range." This is because the data that was loaded is from all time ranges, so make sure you search for ALL times ranges._

For more information about running the Arkime Viewer web application, visit the [viewer README](viewer/README.md).

**To contribute to [Parliament](parliament/README.md) or [WISE](wiseService/README.md), read their READMEs for information on how to build them for development**

---

### How do I contribute?

#### Documentation! :page_with_curl:

Documentation, READMEs, examples, and FAQs are important. Please help improve and add to them.

#### Bugs :bug: :beetle: :ant:

**Before submitting a bug report:**
* Ensure the bug was not already reported by searching for [existing issues in Arkime](https://github.com/arkime/arkime/issues)
  * If an issues is already open, make a comment that you are experiencing the same thing and provide any additional details
* Check the [FAQs](https://arkime.com/faq) for a list of common questions and problems

Bugs are tracked as [GitHub Issues](https://guides.github.com/features/issues/).
**Please follow these guidelines when submitting a bug:**
* Provide a clear and descriptive title
* Describe the exact steps to reproduce the problem
* Explain the expected behavior
* Fill out the [issue template](https://github.com/arkime/arkime/issues/new) completely

#### Feature Requests :sparkles:

Feature requests include new features and minor improvements to existing functionality.

Feature requests are tracked as [GitHub Issues](https://guides.github.com/features/issues/).
**Please follow these guidelines when submitting a feature request:**
* Provide a clear and descriptive title
* Describe the suggested feature in as much detail as possible
* Use examples to help us understand the use case of the feature
* If you are requesting a minor improvement, describe the current behavior and why it is not sufficient
* If possible, provide examples of where this feature exists elsewhere in other tools
* Follow the directions in the [issue template](https://github.com/arkime/arkime/issues/new)

#### Pull Requests :muscle:

**We welcome all collaboration!** If you can fix it or implement it, please do! :hammer:
To implement something new, please create an issue first so we can discuss it together.

**To better help us review your pull request, please follow these guidelines:**
* Provide a clear and descriptive title
* Clearly describe the problem and solution
* Include the relevant issue number(s) if applicable
* If changes are made to the capture component, verify all tests in the tests direction pass by running `./tests.pl`
* Run `npm run lint` from the top level directory and correct any errors
* For any viewer or parliament changes, verify that all API tests pass by running `./tests.pl --viewer`
* For any viewer changes, verify that all UI tests pass by running `npm run viewer:testui`
* The README file in the tests directory provides additional information on the test cases
* When creating a Pull Request please follow [best practices](https://github.com/trein/dev-best-practices/wiki/Git-Commit-Best-Practices) for creating git commits.
* When your code is ready to be submitted, submit a Pull Request to begin the code review process.
* Please use a [fork](https://guides.github.com/activities/forking/) to submit a [pull request](https://help.github.com/articles/creating-a-pull-request/) for your contribution.

We only seek to accept code that you are authorized to contribute to the project. We have added a pull request template on our projects so that your contributions are made with the following confirmation:
> I confirm that this contribution is made under an Apache 2.0 license and that I have the authority necessary to make this contribution on behalf of its copyright owner.

#### API Documentation

The API is documented using [jsdoc](https://jsdoc.app/) and [jsdoc2md](https://github.com/jsdoc2md/jsdoc-to-markdown).

If you update or create an API, please document it thoroughly with these items:
1. METHOD - endpoint (e.g. GET - /api/sessions)
2. Description
3. Name the endpoint with `@name endpointname` (e.g. sessions). This is the name that will be reflected in the title of the endpoint and the table of contents.
4. List the parameters including any defaults with `@param` (e.g. `@param {number} date=1 - The number of hours of data to return (-1 means all data). Defaults to 1`)
5. Describe the return value(s) with `@returns` (e.g. `@returns {array} data - The list of sessions with the requested fields`)

The documentation lives on our website at [arkime.com/api](https://arkime.com/api). It is generated using the code documentation. Follow these steps to update the API documentation:
1. Check out the [arkimeweb repository](https://github.com/arkime/arkimeweb) in the same parent directory as the Arkime repository.
2. Run `npm run viewer:doc` from the `arkime` directory. If you checked out the arkimeweb repository into a different location, you can run `npm run viewer:doc-location -- <path/to/arkimeweb/_wiki/api_docs.md>`.
3. Run the arkime website locally (find out how [here](https://github.com/arkime/arkimeweb/blob/main/CONTRIBUTING.md)) to make sure all of your documentation is correct.
4. Make a PR to [arkime](https://github.com/arkime/arkime/blob/master/CONTRIBUTING.md) and [arkimeweb](https://github.com/arkime/arkimeweb/blob/main/CONTRIBUTING.md) with your changes.

---

### Upgrading Cyberchef
1. Find the latest version of [Cyberchef from GitHub](https://github.com/gchq/CyberChef/releases).
2. Inside the release directory run `./new_cyber_chef.pl <versionnumber>`
3. Test new CyberChef works
4. Commit changes

---

### Code of Conduct

We encourage inclusive and professional interactions on our project. We welcome everyone to open an issue, improve the documentation, report a bug or submit a pull request. By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md). If you feel there is a conduct issue related to this project, please raise it per the Code of Conduct process and we will address it.

---

### Building a release
1. Create a branch
   1. Update CHANGELOG with correct date
   2. Update AC_INIT in configure.ac with correct version
   3. Update ARKIME_API_VERSION in capture/arkime.h when incompatible C API changes
   4. commit and push branch
2. Merge branch on https://github.com/arkime/arkime
3. Update local area `git checkout main && git pull`
4. Tag using version from configure.ac including leading v, `git tag vN.N.N`
5. `git push --tags`
6. Visit https://github.com/arkime/arkime/actions/workflows/release.yml
7. Select "Run workflow" upper right and fill in the version tag *WITHOUT* the leading v
8. Click "Run workflow" button
9. AFTER everything finishes, update the release notes https://github.com/arkime/arkime/releases/
10. Profit!
11. Edit CHANGELOG and configure.ac with the next version when ready

---

### Adding a new build OS
1. In arkime/arkime-build repo
  1. Add a new directory, usually starting with a previous version
  2. Update the makeDockerBuild.sh file in the build and copy sections
  3. Update the Dockerfile for new OS
  4. Run the 2 exports and the docker image build manually
  5. Repeat steps 3-5 until successful
  6. Run docker push
2. Update .github/workflows/build\*.yml
  1. In matrix section add a new section for the OS
  2. Add the name of the new section from 1 to matrix.version array
  3. Test
3. Update .github/workflows/release
  1. In matrix section add a new section for the OS
  2. Add the name of the new section from 2 to matrix.version array
4. Update CHANGELOG


---

### :heart: Thanks,
Andy & Elyse
