# Vuetify Migration — Analyst Smoke Test Readiness

Date: 2026-05-08
Branch: `vuetify-migration` (HEAD `e60c9144`)
Status: **Viewer is fully BVN-free.** Ready for analyst smoke test next week.

## What this document is

A checklist + risk map for validating the Vuetify migration on the
`vuetify-migration` branch before merging to `main`. Designed to be
runnable by analysts with `npm run viewer:dev` in dev mode against a
pre-loaded test cluster.

## Branch scope

131 commits ahead of `main`, 110 files changed. Big-picture phases:

1. **Phase 1**: Common components, navbar, login flow, sidebar (small files,
   broad reach). Older sessions.
2. **Phase 2**: Visualizations (timeline + map), Flot → uPlot, jvectormap → D3.
   Older sessions, shipped 2026-05-06.
3. **Phase 3** (this session, 2026-05-08): All analyst pages — Sessions,
   Connections, Spiview, Spigraph, Stats, Hunt, plus the pug session-detail
   templates and the supporting plumbing (sanitizer, vite config, main.js).
4. **Phase 4** (this session, end of day): Strip BVN package usage from viewer
   entirely. Zero `<b-*>` tags, zero `bootstrap-vue-next` imports, zero
   `createBootstrap()` calls in viewer/. WISE and Parliament still use BVN.

## Smoke test checklist — by route

Run `npm run viewer:dev` against a test cluster with PCAP loaded. Hit each
route and verify the listed surface.

### `/sessions` — HIGH RISK
Most-used analyst page. Largest migration scope. Pug-rendered session-detail
expand view is brand new architecture (Vuetify components in runtime
templates).

- [ ] Sessions table loads and paginates
- [ ] Search bar accepts expressions, clears with X
- [ ] Time range selector works, custom ranges work
- [ ] Click a session → expand panel renders below the row
  - [ ] Field labels (Src, Dst, IP/Port, MAC, etc.) are clickable buttons
  - [ ] Click any field label → menu opens with Export Unique / Open SPI Graph / etc.
  - [ ] Click a field value (e.g., an IP) → it adds to the search expression
  - [ ] Column resize grip on left of each `<dl>` works
  - [ ] Cards in expand view are bordered, theme-aware (light + dark mode)
- [ ] Top-of-row toolbar (All Sessions / Download Segment PCAP / Source raw / Link / Columns / Actions)
  - [ ] All buttons render as `<v-btn variant="text">` Vuetify-native, not bootstrap nav-pills
  - [ ] Columns dropdown opens with One/Two/Three Column options
  - [ ] Actions dropdown opens with Export PCAP / Add Tags / Remove Tags / Send Session entries
- [ ] Packet options below packets render as compact 28px-tall row
  - [ ] Two `<v-select>` (# packets, display type) — text vertically centered, dropdowns open
  - [ ] "Packet Options" button menu opens
  - [ ] Src/Dst toggle group (`<v-btn-toggle>`) toggles correctly
  - [ ] Decoding buttons toggle, decoding-form fields appear when needed
- [ ] Sticky-sessions sidebar still works (open multiple sessions, toggle)
- [ ] Column visibility menu (gear icon)
- [ ] Field config save/load menu

### `/spiview` — HIGH RISK
Whole-file migration this session.

- [ ] Page loads, shows accordion-style category cards
- [ ] Cards have visible borders in BOTH light and dark themes (not too bright/too dark)
- [ ] Expand a category — fields appear as a row of compact buttons
- [ ] +/− toggle on the FAR right of each card header
- [ ] Load All / Unload All buttons work
- [ ] Per-category search input (28px tall) doesn't get clipped
- [ ] Click a field button (split-style) — value loads underneath
- [ ] Click the caret on a field button — menu opens with Export Unique / Open SPI Graph / field actions
- [ ] Active fields show `is-active` highlight on their button
- [ ] Field-config save/load menu (column icon, top right) works

### `/spigraph` — MEDIUM RISK
Subnav grid migrated this session, content body unchanged.

- [ ] Subnav reads as inline flex row (not bootstrap nav-pills)
- [ ] Field typeahead works, max-elements / sort / refresh selects work
- [ ] Graph type selector switches between timeline/donut/table/treemap/sankey
- [ ] **Tooltip on Max Elements** — hover the label, after 300ms shows
      "Maximum number of elements returned ..."
- [ ] No `?` cursors on labels that don't have tooltips (field/graphType/sortBy/refreshEvery)
- [ ] Table/treemap/sankey/donut all render
- [ ] Click into the SPI graph → drill-down popup appears with badges (Count / Src IPs / Dst IPs)

### `/connections` — MEDIUM RISK
Whole subnav migrated this session.

- [ ] Subnav reads as inline flex row
- [ ] **All 8 subnav tooltips show on hover** — querySize, Src field, Dst field,
      Src&dst color, minConn, weight, baselineDate, baselineVis
- [ ] Node fields and Link fields menus (round buttons) open
- [ ] Search input inside each menu filters fields
- [ ] Subheaders for field groups (DNS / TLS / etc.) appear non-interactive
- [ ] Click a field in the list — `:active` highlight appears on it
- [ ] Reset to default works
- [ ] D3 force graph renders, nodes draggable, zoom/text-size/distance buttons work
- [ ] Node and link popups open on click

### `/stats` — LOW RISK
Only a comment was touched. Tabs were already Vuetify.

- [ ] Stats tabs (Capture / ES Stats / Indices / etc.) all render
- [ ] Switching tabs lazy-mounts the child component

### `/hunt`, `/files`, `/users`, `/roles`, `/history`, `/settings`, `/arkime`, `/upload`, `/help`
**LOW RISK** — touched only in earlier phases. Should already be solid since
they've had more time to soak. Quick visual once-over recommended.

## Cross-cutting checks

### Theme support
- [ ] Toggle light theme in user settings → all pages still readable, borders subtle
- [ ] Toggle dark theme → borders visible, no transparent panels, text readable
- [ ] Themed CSS overrides (purp / blue / green / cotton-candy / dark-2 / dark-3 / arkime-light / arkime-dark) all still apply

### Browser console
Open DevTools console on each page and verify NO:
- [ ] "Failed to resolve component: ..." warnings
- [ ] "Invalid prop type" warnings
- [ ] Uncaught TypeErrors during page load or interaction
- [ ] CSP errors

### Bundle size
Current build artifact: `viewer/vueapp/dist/assets/main-l_uJokZB.js` ≈ 4.3 MB.
Rough comparison vs pre-migration if needed via `git checkout main; npm run viewer:bundle; ls -lh viewer/vueapp/dist/assets/main-*.js`.

## Known caveats

- **WISE and Parliament still on BVN** — these are separate frontends sharing
  the same `bootstrap-vue-next` package. Not touched in this branch.
- **Sub-path imports of Vuetify components** in `sessionDetailData.js` and
  `main.js` (`vuetify/components/VMenu` etc.) — these are necessary because
  vite-plugin-vuetify's tree-shake auto-import only sees `.vue` SFCs, and the
  pug session-detail templates are runtime-compiled. Documented in
  `viewer/vueapp/src/components/sessions/sessionDetailData.js` comments.
- **Server-side HTML sanitizer** in `viewer/apiSessions.js` allowlist must be
  kept in sync with any new tags / classes / attributes added to the pug
  session-detail templates. Memory entry
  `project_vuetify_runtime_template_gotcha.md` captures the rule.

## Out of scope for the analyst smoke test

- Bundle-size optimization (deferred until after ship)
- WISE / Parliament migration (separate branches eventually)
- Multi-language regression — only English smoke-tested in dev so far
- IE11 — not supported

## How to file regression bugs

If something looks wrong:
1. Note the route, the action, the component (right-click → Inspect to find).
2. Check browser console for warnings.
3. Reproduce at HEAD on `main` to confirm it's a migration regression vs.
   a pre-existing bug.
4. File against `vuetify-migration` branch with the URL, screenshot, and
   console output.
