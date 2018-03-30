# Contributing to Moloch

:sparkles: Glad to see you here! :sparkles:

---

### Just have a question :question:

* Visit our [FAQs](https://github.com/aol/moloch/wiki/FAQ)
* Or talk to us directly in the [Moloch-FPC Slack](https://slackinvite.molo.ch/)

---

### Where do I start? :traffic_light:

First, checkout the main [Moloch README](README.rst) for information on how to build and run Moloch.

**Then, get some test data!**

* Start Elasticsearch
* Move to the Moloch tests directory
* Run `./tests.pl --viewer`

> **Note:** this will only work if viewer is not already running.

You should now have test data loaded, so let's **start the web app**:

* Move to the Moloch viewer directory
* Run `npm install`
* Move to the vueapp directory
* Run `npm install`
* Move back up to the viewer directory
* Run `npm run start:test`
* Now browse to the app at `http://localhost:8123`

> :clock1: _On first load, you will likely see this message: "No results or none that match your search within your time range." This is because the data that was loaded is from all time ranges, so make sure you search for ALL times ranges._

For more information about running the Moloch Viewer web application, visit the [viewer README](viewer/README.md).

---

### How do I contribute?

#### Documentation! :page_with_curl:

Documentation, READMEs, examples, and FAQs are important. Please help improve and add to them.

#### Bugs :bug: :beetle: :ant:

**Before submitting a bug report:**
* Ensure the bug was not already reported by searching for [existing issues in Moloch](https://github.com/aol/moloch/issues)
  * If an issues is already open, make a comment that you are experiencing the same thing and provide any additional details
* Check the [FAQs](https://github.com/aol/moloch/wiki/FAQ) for a list of common questions and problems

Bugs are tracked as [GitHub Issues](https://guides.github.com/features/issues/).
**Please follow these guidelines when submitting a bug:**
* Provide a clear and descriptive title
* Describe the exact steps to reproduce the problem
* Explain the expected behavior
* Fill out the [issue template](https://github.com/aol/moloch/issues/new) completely

#### Feature Requests :sparkles:

Feature requests include new features and minor improvements to existing functionality.

Feature requests are tracked as [GitHub Issues](https://guides.github.com/features/issues/).
**Please follow these guidelines when submitting a feature request:**
* Provide a clear and descriptive title
* Describe the suggested feature in as much detail as possible
* Use examples to help us understand the use case of the feature
* If you are requesting a minor improvement, describe the current behavior and why it is not sufficient
* If possible, provide examples of where this feature exists elsewhere in other tools
* Follow the directions in the [issue template](https://github.com/aol/moloch/issues/new)

#### Pull Requests :muscle:

**We welcome all collaboration!** If you can fix it or implement it, please do! :hammer:

**To better help us review your pull request, please follow these guidelines:**
* Provide a clear and descriptive title
* Clearly describe the problem and solution
* Include the relevant issue number(s) if applicable
* Ensure that all tests still pass by navigating to the `tests` directory and running `./tests.pl --viewer`
* If making changes to the client code, please run the unit tests by navigating to the `viewer` directory and running `npm test`

---

### THANKS! :heart:
