Fixtures for api-dashboards.t, listed after release/dashboards in config.test.ini's dashboardsDir:

- http.dashboard.json: same name as a shipped one, so it is ignored (first directory wins)
- zz-test.dashboard.json: a valid extra dashboard in a Test folder
- broken.dashboard.json: not JSON, skipped
- noname.dashboard.json: no name, skipped
- badfolder.dashboard.json: a 65 character folder, skipped
- requires-missing.dashboard.json: requires a field that doesn't exist, hidden
- requires-known.dashboard.json: requires fields that exist, listed with its requires
