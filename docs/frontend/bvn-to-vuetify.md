# Bootstrap Vue Next → Vuetify Migration Reference

**Scope:** living cheat sheet for the BVN → Vuetify migration on the `vuetify-migration` branch.
See `docs/superpowers/specs/2026-04-29-arkime-7-ui-ideas.md` for context and the migration plan.
The reference implementation is **cont3xt** — when in doubt, look there first.

---

## Reference files in cont3xt (read these first)

- `cont3xt/vueapp/vite.config.mjs` — `vite-plugin-vuetify` setup with `treeShake: true` and `styles.configFile`
- `cont3xt/vueapp/src/main.js` (lines 25–63) — `createVuetify()` config with shared `defaults` block and theme registration
- `cont3xt/vueapp/src/theme.js` — pattern for defining themes; `createCont3xtTheme(variant)` returns a Vuetify theme object
- `cont3xt/vueapp/src/utils/Navbar.vue` (around lines 223–230) — runtime theme toggle with `useTheme()` + Vuex + localStorage
- `cont3xt/vueapp/src/vuetify-settings.scss` — Vuetify SCSS customization entry point

---

## Wiring Vuetify into a new app

For each app being migrated (viewer, parliament, wiseService), Vuetify is added **alongside** Bootstrap Vue Next during the transition. Both libraries are mounted at the same time. BVN is removed only at the per-app cutover.

### Install

```bash
# From the app's directory (e.g., viewer/)
npm install vuetify vite-plugin-vuetify
```

### vite.config.mjs

Mirror the cont3xt setup. Add:

```js
import Vuetify from 'vite-plugin-vuetify';

// inside plugins:
plugins: [
  vue({}),
  Vuetify({
    treeShake: true,
    styles: {
      configFile: '<app>/vueapp/src/vuetify-settings.scss'
    }
  }),
  // existing BVN resolver stays — both libraries coexist for the duration of the migration
  Components({ resolvers: [BootstrapVueNextResolver()] }),
],

// inside top level:
css: {
  preprocessorOptions: {
    sass: { api: 'modern' }
  }
}
```

### vuetify-settings.scss

Start with an empty file (or one `@use` for the cont3xt-style minimal customization). Add overrides only when needed.

### theme.js

Start with a minimal scaffold while the theme question is deferred (per migration plan). For viewer specifically, keep the existing 8 theme CSS files loading via the body class toggle. Vuetify's theme API gets a minimal light/dark scaffolding that the legacy CSS overlays on top of:

```js
export function createAppTheme (variant) {
  return {
    dark: variant === 'dark',
    colors: {
      // Add only what's needed at runtime — the 8 viewer theme CSS files
      // continue to provide the actual visual treatment via --color-* vars.
    }
  };
}
```

### main.js

Add Vuetify alongside the existing `createBootstrap()` call. **Do not remove createBootstrap during the per-component migration.** Both libraries mounted in parallel is the planned interim state.

```js
import { createVuetify } from 'vuetify/lib/framework.mjs';
import 'vuetify/styles';
import { createAppTheme } from './theme.js';

const vuetify = createVuetify({
  defaults: {
    // Match cont3xt's defaults — analyst UI is dense, density: 'compact' everywhere
    VTextField: { density: 'compact', variant: 'outlined', hideDetails: true },
    VSelect: { density: 'compact', variant: 'outlined', hideDetails: true },
    VCheckbox: { density: 'compact', hideDetails: true },
    VTooltip: { location: 'top', delay: 50, maxWidth: 400 },
    VBtn: { density: 'compact', variant: 'flat' }
  },
  theme: {
    options: { customProperties: true },
    defaultTheme: 'appLightTheme',
    themes: {
      appLightTheme: createAppTheme('light'),
      appDarkTheme: createAppTheme('dark')
    }
  }
});

app.use(vuetify);
// existing app.use(createBootstrap(...)) stays during the migration
```

---

## Component substitution table

Most-used BVN components in the codebase are listed first. Both PascalCase (`<BButton>`) and legacy hyphenated (`<b-button>`) BVN syntaxes coexist in the codebase — search for both when migrating a file.

### Tooltip — most common (232+ uses in viewer alone)

| BVN | Vuetify |
|---|---|
| `<BTooltip target="elementId" title="text" />` | `<v-tooltip activator="#elementId" text="text" />` |
| `<b-tooltip :target="ref">Slot text</b-tooltip>` | `<v-tooltip :activator="ref"><span>Slot text</span></v-tooltip>` |

**Gotchas:**
- BVN's `target` prop becomes Vuetify's `activator`.
- BVN supports `title="..."` as a shortcut; Vuetify uses `text="..."` for the same purpose.
- Vuetify defaults set `location="top"`, `delay=50`, `maxWidth=400` (matching BVN behavior). Override per-tooltip if needed.
- The most idiomatic Vuetify pattern uses `activator="parent"` + scoped slot:
  ```vue
  <v-btn>
    Click me
    <v-tooltip activator="parent" location="top">Helpful hint</v-tooltip>
  </v-btn>
  ```

### Buttons

| BVN | Vuetify |
|---|---|
| `<BButton variant="primary">Save</BButton>` | `<v-btn color="primary">Save</v-btn>` |
| `<BButton variant="outline-secondary">` | `<v-btn variant="outlined" color="secondary">` |
| `<BButton size="sm">` | `<v-btn density="compact">` (also `size="small"` works) |
| `<BButton :disabled="loading">` | `<v-btn :disabled="loading">` (same) |
| `<BButton @click="...">` | `<v-btn @click="...">` (same) |

**Gotchas:**
- BVN's `variant="primary"` becomes Vuetify's `color="primary"`. Vuetify's `variant` is a different concept (visual style: `flat`/`outlined`/`tonal`/`text`/`elevated`).
- BVN's `variant="outline-primary"` becomes `color="primary" variant="outlined"`.

### Forms

| BVN | Vuetify |
|---|---|
| `<BForm>` | `<v-form>` |
| `<BFormInput v-model="x">` | `<v-text-field v-model="x">` |
| `<BFormTextarea v-model="x">` | `<v-textarea v-model="x">` |
| `<BFormSelect v-model="x" :options="opts">` | `<v-select v-model="x" :items="opts">` |
| `<BFormCheckbox v-model="x">` | `<v-checkbox v-model="x">` |
| `<BFormRadio v-model="x" :value="v">` | `<v-radio v-model="x" :value="v">` (wrap in `<v-radio-group>`) |
| `<BFormGroup label="...">` | wrap inputs directly; use `label="..."` prop on the input |

**Gotchas:**
- BVN's `:options="[{value, text}]"` becomes Vuetify's `:items="[{value, title}]"`. Note the prop name change (`text` → `title`).
- Vuetify v-text-field/v-select default to `variant="filled"` — override to `variant="outlined"` per cont3xt's `defaults` (already set in the suggested main.js).
- Vuetify form fields show a "details" area below by default (validation messages, hints). Use `hideDetails="auto"` (already set in cont3xt defaults) to hide unless validation fires.
- For checkbox-bound booleans, Vuetify's `v-checkbox` works the same as BVN's `b-form-checkbox`.

### Input Groups (121+ uses in viewer)

BVN: `<BInputGroup>` with optional `<BInputGroupText>` prepend/append wrappers around a form input.

| BVN | Vuetify |
|---|---|
| `<BInputGroup><BInputGroupText>$</BInputGroupText><BFormInput /></BInputGroup>` | `<v-text-field><template #prepend-inner>$</template></v-text-field>` |
| `<BInputGroup append="USD"><BFormInput /></BInputGroup>` | `<v-text-field suffix="USD" />` (prefix/suffix props) |

**Pattern:** most BVN input groups in the codebase exist to add a prepend icon or label. Vuetify's `v-text-field` has built-in `prepend-inner`, `append-inner`, `prefix`, `suffix` slots/props that cover the same ground without a wrapper element. Output is usually cleaner.

### Modals

| BVN | Vuetify |
|---|---|
| `<BModal v-model="show" title="X">body</BModal>` | `<v-dialog v-model="show"><v-card><v-card-title>X</v-card-title><v-card-text>body</v-card-text></v-card></v-dialog>` |
| `<b-modal>` with footer `<template #modal-footer>` | `<v-card-actions>` slot at the bottom of `<v-card>` |
| `id="foo" + this.$bvModal.show('foo')` | switch to `v-model`-driven open state |

**Gotchas:**
- Vuetify's modal is `v-dialog` and is **un-styled by default** — it's just an overlay. You compose it with a `v-card` for chrome (title/body/actions).
- BVN's imperative `this.$bvModal.show('id')` API does not exist in Vuetify. Refactor to a reactive boolean bound via `v-model`. This may require small refactors in the few files that use the imperative API.

### Dropdowns

| BVN | Vuetify |
|---|---|
| `<BDropdown text="Menu">` | `<v-menu><template #activator="{ props }"><v-btn v-bind="props">Menu</v-btn></template><v-list>...</v-list></v-menu>` |
| `<BDropdownItem @click="...">` | `<v-list-item @click="...">` |
| `<BDropdownDivider />` | `<v-divider />` |

**Gotchas:**
- Vuetify decomposes BVN's monolithic dropdown into menu + list. More verbose, but more flexible.
- The `activator` slot pattern is unfamiliar coming from BVN — copy from cont3xt examples.

### Tables (BTable)

| BVN | Vuetify |
|---|---|
| `<BTable :items="rows" :fields="cols">` | `<v-data-table :items="rows" :headers="cols">` |

**Gotchas:**
- BVN's `:fields` becomes Vuetify's `:headers`. Field config keys differ: BVN uses `{key, label}`, Vuetify uses `{value, title}`.
- Vuetify's `<v-data-table>` includes built-in pagination, sorting, and selection — review whether the existing implementation depends on BVN's `b-table` slot patterns and adapt. The viewer has a custom `Table.vue` wrapper (`viewer/vueapp/src/components/utils/Table.vue`) that may already handle this; check first.

### Tabs

| BVN | Vuetify |
|---|---|
| `<BTabs v-model="active">` | `<v-tabs v-model="active">` (just the tab strip) |
| `<BTab title="Tab1">content</BTab>` | `<v-tab value="tab1">Tab1</v-tab>` (nav) + `<v-window>` / `<v-window-item>` (content) |

**Gotchas:**
- BVN couples nav + content in `BTab`. Vuetify decouples them: `<v-tabs>` for the strip, `<v-tabs-window>` (or `<v-window>` + `<v-window-item>`) for the panels.

### Cards

| BVN | Vuetify |
|---|---|
| `<BCard title="X">body</BCard>` | `<v-card><v-card-title>X</v-card-title><v-card-text>body</v-card-text></v-card>` |

### Alerts

| BVN | Vuetify |
|---|---|
| `<BAlert variant="danger" :show="x">msg</BAlert>` | `<v-alert v-model="x" type="error">msg</v-alert>` |

**Variant→type mapping:** `danger` → `error`, `success` → `success`, `warning` → `warning`, `info` → `info`.

### Spinners

| BVN | Vuetify |
|---|---|
| `<BSpinner />` | `<v-progress-circular indeterminate />` |
| `<BSpinner small />` | `<v-progress-circular indeterminate size="20" />` |

### Badges

| BVN | Vuetify |
|---|---|
| `<BBadge variant="primary">3</BBadge>` | `<v-chip color="primary" size="small">3</v-chip>` (for inline labels) |
| `<BBadge>` as overlay | `<v-badge content="3"><icon /></v-badge>` (for overlays on something else) |

**Gotchas:**
- BVN's `BBadge` is mostly used as an inline label, which maps better to Vuetify's `v-chip` than `v-badge`. Vuetify's `v-badge` is specifically for overlay-on-element badges (e.g., notification dots on icons).

### Collapse

| BVN | Vuetify |
|---|---|
| `<BCollapse v-model="show">content</BCollapse>` | `<v-expand-transition><div v-show="show">content</div></v-expand-transition>` |

### Popover

| BVN | Vuetify |
|---|---|
| `<BPopover target="x">popover</BPopover>` | `<v-menu activator="#x"><v-card>popover</v-card></v-menu>` (or use `v-tooltip` for hover popovers) |

### Navbar (per-app)

Each app has its own Navbar.vue using `<BNavbar>`. Map to Vuetify's `<v-app-bar>`:

| BVN | Vuetify |
|---|---|
| `<BNavbar>` | `<v-app-bar>` |
| `<BNavbarNav>` | `<v-tabs>` (for top-level tab nav) |
| `<BNavItem>` | `<v-tab>` |
| `<BNavbarBrand>` | `<v-app-bar-title>` |

---

## Common conventions

### Density

Cont3xt uses `density="compact"` on all form fields and buttons by default (set in the `defaults` block of `createVuetify()`). Match this — Arkime is a dense analyst UI, not a marketing site.

### Colors

| BVN variant | Vuetify color | Notes |
|---|---|---|
| `primary` | `primary` | same |
| `secondary` | `secondary` | same |
| `success` | `success` | same |
| `danger` | `error` | **note rename** |
| `warning` | `warning` | same |
| `info` | `info` | same |
| `light` | `surface` or custom | Vuetify has no `light` by default — define in theme if needed |
| `dark` | `on-surface` or custom | similar |
| `outline-X` | `color="X" variant="outlined"` | two props instead of one variant |

### Class shortcuts

BVN supports utility class shortcuts (`size="sm"`, `pill`, `block`, etc.). Vuetify equivalents:

| BVN | Vuetify |
|---|---|
| `size="sm"` | `density="compact"` or `size="small"` |
| `size="lg"` | `density="default"` + `size="large"` |
| `block` | `block` (same) |
| `pill` | `rounded="pill"` |
| `squared` | `rounded="0"` |

### Slots

Vuetify slot names are usually `kebab-case` and align with the BEM-ish structure of the component (`#prepend-inner`, `#append-inner`, `#activator`). When migrating, search for `<template #...>` in the file and check the Vuetify docs for the new slot name.

### Tooltip-on-icon idiom

Most BVN tooltips in the codebase wrap an icon button. Idiomatic Vuetify version using `activator="parent"`:

```vue
<v-btn icon="fa-filter" density="compact" variant="text">
  <v-tooltip activator="parent" location="top">Filter sessions</v-tooltip>
</v-btn>
```

This avoids needing IDs / refs to wire up the activator.

### Icons stay on Font Awesome

Per the migration plan, viewer/parliament/wiseService keep Font Awesome 4.7 throughout this migration (icon library swap is deferred). Vuetify components that take an `icon` prop accept any string class — pass FA classes:

```vue
<v-btn icon variant="text">
  <i class="fa fa-trash"></i>
</v-btn>
```

Or use the `icon` prop with a string the way BVN did. Avoid switching to MDI mid-migration.

---

## Gotchas learned during the migration

These came up in actual smoke testing — they're avoidable if you know about them up front.

### Don't put `v-btn` inside a Bootstrap `input-group`

The default `density: 'compact'` on `VBtn` (set in `main.js`) applies a class that does `height: calc(var(--v-btn-height) - 12px)`. Inside a `.input-group`, the surrounding inputs and addon spans are taller, so the v-btn ends up visibly shorter than its siblings — gaps at top and bottom.

**Pattern:** for clear / submit / icon buttons that sit inside an `input-group`, use a **raw `<button>` with Bootstrap classes**:

```vue
<div class="input-group input-group-sm">
  <span class="input-group-text">...</span>
  <input class="form-control" v-model="..." />
  <button
    type="button"
    class="btn btn-outline-secondary btn-clear-input"
    @click="clear"
    :disabled="!query.filter">
    <span class="fa fa-close" />
  </button>
</div>
```

This is consistent with how unmigrated parts of the codebase already do it (e.g., the second clear button in `History.vue`).

### Vuetify components have built-in icons — Font Awesome must be configured as the iconset

Many Vuetify v3 components render icons by default (`v-file-input` shows a paperclip, `v-select` shows a chevron, `v-text-field` clearable shows an X, `v-data-table` shows sort arrows and pagination chevrons, `v-checkbox` checkmark, etc.). Out of the box, Vuetify uses **Material Design Icons (MDI)** for these defaults.

**Arkime uses Font Awesome 4.7, not MDI.** Without an iconset config, Vuetify renders empty `<i class="mdi mdi-paperclip ...">` elements — the classes are present but no font is loaded, so the icon is invisible AND takes phantom layout space, making components look broken/oversized.

**The fix is global**, set once in `viewer/vueapp/src/main.js`:

```js
import { createVuetify } from 'vuetify/lib/framework.mjs';
import { aliases as faAliases, fa as faSet } from 'vuetify/iconsets/fa4';

const vuetify = createVuetify({
  icons: {
    defaultSet: 'fa',
    aliases: faAliases,
    sets: { fa: faSet }
  },
  // ... defaults, theme, etc.
});
```

After this, Vuetify's symbolic aliases (`$file`, `$close`, `$next`, `$prev`, `$expand`, `$dropdown`, `$checkboxOn`/`$checkboxOff`, etc.) resolve to FA 4 classes. The `fa4` iconset maps:

- `$file` → `fa-paperclip` (v-file-input prepend)
- `$close` → `fa-times`
- `$next`/`$prev` → `fa-chevron-right`/`fa-chevron-left`
- `$expand` → `fa-chevron-down`
- `$dropdown` → `fa-caret-down`
- `$checkboxOn`/`$checkboxOff` → `fa-check-square`/`fa-square-o`
- `$success`/`$error`/`$warning`/`$info` → corresponding FA glyph
- `$sortAsc`/`$sortDesc` → `fa-arrow-up`/`fa-arrow-down`
- And more — see `node_modules/vuetify/lib/iconsets/fa4.js` for the full list.

**Diagnostic:** if a Vuetify component looks oddly sized or has a visible-but-blank glyph slot, inspect the DOM and check for `<i class="mdi ...">` — that's the broken-icon symptom. The iconset config above resolves it everywhere at once.

**For your own icons in templates,** you can still use plain `<span class="fa fa-...">` exactly as before, or use `<v-icon icon="fa fa-trash" />` / `<v-icon icon="$delete" />`. All three work.

### Don't use `v-checkbox` for unlabeled toggles in tables

`v-checkbox` is a labeled-input component. Even with `hide-details` and `density="compact"`, it still has internal padding and a min-height that inflates the parent cell — bad in a `<th>` or any tight context.

**Pattern:** for unlabeled toggles, use a raw `<input type="checkbox">`:

```vue
<input
  type="checkbox"
  id="seeAll"
  class="checkbox d-inline me-2"
  @change="toggleSeeAll"
  v-if="..." >
<v-tooltip activator="#seeAll" location="bottom">
  <span v-html="$t('history.seeAllTipHtml')" />
</v-tooltip>
```

This matches existing project convention (`History.vue` already uses raw checkboxes for the per-column "exists" toggles). Reserve `v-checkbox` for forms where you actually want the labeled input + validation surface.

### Vuetify components don't inherit the legacy --color-* theme

The viewer's 8 theme CSS files in `src/themes/*` set `--color-*` custom properties on the body and target Bootstrap classes (`.table`, `.dropdown-menu`, etc.). Vuetify renders different DOM (`.v-table`, `.v-list-item`, `.v-data-table-footer`) so those existing rules don't reach.

Rather than per-component scoped CSS for every migrated file, **the bridge lives in `viewer/vueapp/src/overrides.css`** at the bottom in the "Vuetify overrides" section. That file is imported once by `main.js` and applies globally:

- `.v-table` — transparent backgrounds + `--color-foreground` text so the body theme shows through
- `.v-data-table-footer` — compressed pagination chrome (Vuetify's defaults are too tall for analyst UI)
- `.v-list` / `.v-list-item` — matched to the existing `.dropdown-menu` styling (28px row height, 0.85rem font, --color-* hover/active states)

When you migrate a new component and find it doesn't pick up the theme, **prefer extending `overrides.css`** over per-component styles. Per-component scoped styles are the right tool for one-off layout fixes; the overrides file is for app-wide Vuetify integration with the existing theme system.

A proper Vuetify-native theme rework (translating the 8 CSS theme files into Vuetify theme objects) is deferred to a post-migration redesign pass. The override approach is intentionally a stopgap.

### Density chrome on `v-data-table` / `v-list-item`

Even at `density="compact"`, Vuetify's defaults are spacious for an analyst dense UI:
- `v-list-item` defaults to `min-height: 40px` at compact density.
- `v-data-table` shows a full-size pagination footer regardless of dataset size.

For dropdowns / menus: the override CSS in `overrides.css` brings `v-list-item` to ~28px. No per-component override needed.

For data tables with bounded data (e.g., summary cards with `resultsLimit: 20`):

```vue
<v-data-table
  density="compact"
  :items-per-page="-1"
  hide-default-footer
  ...
/>
```

`hide-default-footer` removes the chrome entirely; `:items-per-page="-1"` shows all rows. Reserve the default footer for tables with unbounded server-side pagination.

### Table header vertical alignment after migration

Bootstrap's default `<th>` is `vertical-align: middle`. When a header cell contains a tooltip activator, sort icon, filter input, AND label text, the middle alignment looks awkward — text floats away from the row's bottom edge. Add scoped style:

```css
table thead tr th {
  vertical-align: bottom;
  padding-top: 4px;
}
```

## Per-PR checklist

For each component being migrated:

1. Search the file for both `<B*>` (PascalCase) and `<b-*>` (legacy hyphenated) BVN tags.
2. Replace each with the Vuetify equivalent from the table above.
3. If the file uses anything from `common/vueapp/` that's still BVN, migrate that common component in the same PR (this is how `common/vueapp/` gets migrated during Phase 1).
4. Manual smoke test: `npm run viewer:dev` (or relevant app), render the affected page, exercise the primary interactions, check console for errors/warnings.
5. Verify i18n keys still resolve.
6. `npm run lint` passes.
7. Commit on `vuetify-migration` branch with message `vuetify: migrate <Component>` for traceability.

---

## When you don't know what to do

1. Look at how cont3xt does the same thing.
2. Check this doc for a substitution.
3. If still stuck, check Vuetify v3 docs at https://vuetifyjs.com/en/components/all/.
4. Add the resolved pattern back to this doc so the next person doesn't have to figure it out twice.
