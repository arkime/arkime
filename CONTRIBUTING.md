# Contributing to Arkime

:sparkles: Glad to see you here! :sparkles:

---

### Just have a question :question:

* Visit our [FAQs](https://arkime.com/faq)
* Or talk to us directly in the [Arkime Slack](https://slackinvite.arkime.com/)

---

### Where do I start? :traffic_light:

First, checkout the main [Arkime README](README.md) for information on how to build and run Arkime.

**Then, get some test data!**

* Make sure node 10 is in your path
* Install and Start Elasticsearch
* Use `easybutton-build.sh` or `configure` and `make` to build everything
* Run `make check` from the top level directory, this will
  * run `npm ci` everywhere
  * run `tests.pl` and `tests.pl --viewer` in the tests directory

> **Note:** this will only work if viewer is not already running.

You should now have test data loaded, so let's **start the web app**:

* Move to the Arkime viewer directory
* Run `npm ci`
* Move to the vueapp directory
* Run `npm ci`
* Move back up to the viewer directory
* Run `npm run start:test`
* Now browse to the app at `http://localhost:8123`

> :clock1: _On first load, you will likely see this message: "No results or none that match your search within your time range." This is because the data that was loaded is from all time ranges, so make sure you search for ALL times ranges._

For more information about running the Arkime Viewer web application, visit the [viewer README](viewer/README.md).

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

**To better help us review your pull request, please follow these guidelines:**
* Provide a clear and descriptive title
* Clearly describe the problem and solution
* Include the relevant issue number(s) if applicable
* If changes are made to the capture component, verify all tests in the tests direction pass by running `./tests.pl`
* Run `npm run lint` from the top level directory and correct any errors
* Additionally, for any viewer or parliament changes, verify that all UI tests pass by runnning `./tests.pl --viewer`
* The README file in the tests directory provides additional information on the test cases

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
2. Run `npm run viewerdoc` from the `arkime` directory. If you checked out the arkimeweb repository into a different location, you can run `npm run viewerdoc-location -- <path/to/arkimeweb/_wiki/api_docs.md>`.
3. Run the arkime website locally (find out how [here](https://github.com/arkime/arkimeweb/blob/main/CONTRIBUTING.md)) to make sure all of your documentation is correct.
4. Make a PR to [arkime](https://github.com/arkime/arkime/blob/master/CONTRIBUTING.md) and [arkimeweb](https://github.com/arkime/arkimeweb/blob/main/CONTRIBUTING.md) with your changes.

---

### Upgrading Cyberchef
1. Update the CyberChef version ("CYBERCHEFVERSION") in `viewer/viewer.js `
2. Update the CyberChef version in `viewer/Makefile.in` (there are two version numbers on line 23)
3. Run `make` in viewer or download the new version of the CyberChef zip file manually to the `viewer/public` directory
5. Unzip and copy `CyberChef_v*.html` to `viewer/public/CyberChef_v*.html`
6. Add `<base href="./cyberchef/" />` in the `<head>` of `viewer/public/CyberChef_v*.html`
7. Add the script section from the previous `viewer/public/cyberchef.html` file before the end `</body></html>` tags at the end of `viewer/public/CyberChef_v*.html`
8. Add `<meta name="referrer" content="no-referrer">` to `viewer/public/CyberChef_v*.html`
9. Delete the old CyberChef zip file
10. Delete the old CyberChef html file
11. Rename `viewer/public/CyberChef_v*.html` file to `viewer/public/cyberchef.html`

---

### :heart: Thanks,
Andy & Elyse
