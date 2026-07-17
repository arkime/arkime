# Arkime Shareables API

Generic key-value store with type-based namespacing and role/user-based sharing.

## REST API

| Method | Endpoint | Auth | Description |
|--------|----------|------|-------------|
| GET | `/api/shareables?type={type}` | cookie | List all shareables of a type the user can view |
| GET | `/api/shareable/{id}` | cookie | Get a single shareable by ID |
| POST | `/api/shareable` | cookie + CSRF | Create a new shareable |
| PUT | `/api/shareable/{id}` | cookie + CSRF | Update an existing shareable (must have edit permission) |
| DELETE | `/api/shareable/{id}` | cookie + CSRF | Delete a shareable (creator or arkimeAdmin only) |

## Common Envelope

Every shareable stored in Elasticsearch has this structure:

```json
{
  "id": "string (auto-generated)",
  "name": "string (required)",
  "description": "string (optional)",
  "type": "string (required, immutable after creation)",
  "creator": "string (userId, set by server)",
  "created": "date (set by server)",
  "updated": "date (set by server on every save)",
  "data": { ... },
  "viewRoles": ["roleId", ...],
  "viewUsers": ["userId", ...],
  "editRoles": ["roleId", ...],
  "editUsers": ["userId", ...]
}
```

Response objects also include computed fields:
- `canEdit` — whether the requesting user can edit this shareable
- `canDelete` — whether the requesting user can delete this shareable

## Permission Model

| Action | Who |
|--------|-----|
| View | Creator, viewUsers, viewRoles |
| Edit | Creator, editUsers, editRoles |
| Delete | Creator, arkimeAdmin |

---

## Shareable Types

### `summaryConfig` (Viewer Vue app)

Summary/Arkime tab modular dashboard configuration (layout + per-widget settings).

**Used by:** `viewer/vueapp/src/components/arkime/Arkime.vue`, `SummaryConfigDropdown.vue`, `SummaryConfigSaveModal.vue`

```json
{
  "type": "summaryConfig",
  "data": {
    "colorScheme": "rainbow",
    "widgets": [
      {
        "id": "ab12cd34",
        "field": "source.ip",
        "viewMode": "bar",
        "metricType": "sessions",
        "length": 20,
        "order": "desc",
        "expression": "protocols == tls",
        "view": "",
        "height": 1,
        "width": 1,
        "title": ""
      }
    ]
  }
}
```

The grid is a fixed 4 columns wide; widgets span 1-4 columns and 1-8 rows (each
row is a 160px unit, so a chart's default 3 rows ≈ 480px and short widgets like
capture stats / time fit a single ~160px row).

| Field | Type | Description |
|-------|------|-------------|
| `data.colorScheme` | string | Dashboard chart palette (`rainbow`, `tableau10`, `category10`, `viridis`, `cool`, `warm`, `spectral`). Default: `rainbow` |
| `data.widgets[].id` | string | Stable per-widget id (allows two widgets on one field) |
| `data.widgets[].field` | string | Primary Arkime field expression (e.g. `source.ip`, `protocols`) = `fields[0]`. For `map` it is a geo field (e.g. `country.src`). Empty (`""`) for session-wide types (`timeline`, `stats`, `time`), which describe the whole result set rather than one field. |
| `data.widgets[].fields` | string[] | 1–3 field exps. `pie`/`treemap`/`intersection`/`table` accept up to 3 (nested combinations for pie/treemap/intersection; side-by-side per-field columns for table). Other types use a single field (`= [field]`). |
| `data.widgets[].viewMode` | string | Visualization type. Field-bound: `bar`, `pie`, `table`, `intersection` (spigraph unique-values table), `heatmap`, `treemap`, `map` (world choropleth of a geo field). Session-wide: `timeline`, `stats` (capture statistics), `time` (time information). Default: `bar` |
| `data.widgets[].metricType` | string | Quantity the widget visualizes (bar height, pie slice, table column, heatmap intensity, treemap size, or the `timeline` series plotted over time): `sessions` (session count) or any numeric (integer) field exp (e.g. `bytes`, `packets`, `databytes`, `dns.query.cnt`), summed per value. Top/Bottom N is ordered by this metric. Not used by `intersection` (count), `map`, `stats`, or `time`. Default: `sessions` |
| `data.widgets[].length` | number | Max results for this widget (10, 20, 50, 100). Default: `20` |
| `data.widgets[].order` | string | Sort order: `asc` (Bottom) or `desc` (Top). Default: `desc` |
| `data.widgets[].expression` | string | Optional local filter expression, ANDed with the global search (and with `view` if set) |
| `data.widgets[].view` | string | Optional saved-view id used as a local filter; its expression is ANDed with the global search |
| `data.widgets[].width` | number | Column span, 1-4. Default: `2` |
| `data.widgets[].height` | number | Row span, 1-8 (160px row unit). Default: `3` (≈480px). Short session widgets (stats/time) use `1`. |
| `data.widgets[].title` | string | Optional custom widget title (overrides the field name) |

> **Legacy shape (v6):** the v6 summary page uses `data.fields[] + data.resultsLimit + data.order`
> (no `widgets`/`colorScheme`). A per-user copy of the active dashboard is also stored via
> `POST /api/user/savestate?stateName=summary`, and the user's default landing
> dashboard id is stored on `user.settings.defaultDashboardId`.
>
> **v6 ↔ v7 compatibility:**
> - *Reading a v6 config in v7:* the Arkime tab migrates `data.fields[]` (+ the
>   global `resultsLimit`/`order`) into per-widget objects on load. This migration
>   is **lossy**: v6 rendered the timeline/map/capture-stats/time as a fixed band
>   that was never part of `fields[]`, so a migrated v6 config contains only the
>   field widgets — those session-wide widgets are **not** restored and must be
>   re-added. (A v6 user with *no* saved config gets the full default dashboard,
>   which seeds them.)
> - *Reading a v7 config in v6:* v7 **dual-writes** the legacy shape — it emits
>   `data.fields[]/resultsLimit/order` alongside `widgets[]/colorScheme` on every
>   save (savestate, shareable, export). A v6 viewer reads `fields[]` and ignores
>   the v7-only keys, so dashboards saved by v7 stay readable across a mixed-version
>   cluster or a downgrade. Field-bound widgets project to `fields[]` (v7-only view
>   modes map to the nearest v6 mode: `intersection`→`table`, `heatmap`/`treemap`→`bar`);
>   session-wide widgets have no v6 equivalent and are omitted, and `resultsLimit`/`order`
>   are taken from the first field widget.

---

### `capture-columns` (Alkeme TUI)

Capture Stats tab column layout.

**Used by:** `alkeme/src/app/types.rs`, `alkeme/src/app/mod.rs`

```json
{
  "type": "capture-columns",
  "data": {
    "columns": ["nodeName", "currentTime", "monitoring", "freeSpaceM", "cpu", "memory", "deltaPackets", "deltaBytesPerSec", "deltaSessions", "deltaDropped"],
    "order": [["deltaPacketsPerSec", "desc"]]
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data.columns` | string[] | Ordered list of capture stat field names to display |
| `data.order` | [[string, string]] | Sort: `[[sortField, "asc"\|"desc"]]` |

**Available columns (37):** `nodeName`, `currentTime`, `monitoring`, `freeSpaceM`, `cpu`, `memory`, `packetQueue`, `diskQueue`, `esQueue`, `deltaPackets`, `deltaBytesPerSec`, `deltaSessions`, `deltaDropped`, `deltaMS`, `totalPackets`, `totalSessions`, `totalDropped`, `totalK`, `deltaPacketsPerSec`, `deltaSessionsPerSec`, `deltaDroppedPerSec`, `overloadDropped`, `deltaOverloadDropped`, `esHealthShort`, `closeQueue`, `needSave`, `fragsQueue`, `frags`, `deltaFragsDroppedPerSec`, `deltaOverloadDroppedPerSec`, `deltaESDroppedPerSec`, `deltaWrittenBytesPerSec`, `deltaUnwrittenBytesPerSec`, `startTime`, `runningTime`, `tcpSessions`, `udpSessions`

**Default columns (13):** `nodeName`, `currentTime`, `monitoring`, `freeSpaceM`, `cpu`, `memory`, `packetQueue`, `diskQueue`, `esQueue`, `deltaPackets`, `deltaBytesPerSec`, `deltaSessions`, `deltaDropped`

---

### `esnodes-columns` (Alkeme TUI)

DB Nodes (Elasticsearch nodes) tab column layout.

**Used by:** `alkeme/src/app/types.rs`, `alkeme/src/app/mod.rs`

```json
{
  "type": "esnodes-columns",
  "data": {
    "columns": ["name", "docs", "storeSize", "freeSize", "heapSize", "load", "cpu", "read", "write", "searches"],
    "order": [["name", "asc"]]
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data.columns` | string[] | Ordered list of ES node field names to display |
| `data.order` | [[string, string]] | Sort: `[[sortField, "asc"\|"desc"]]` |

**Available columns (27):** `name`, `docs`, `storeSize`, `freeSize`, `heapSize`, `load`, `cpu`, `read`, `write`, `searches`, `searchesTime`, `version`, `nonHeapSize`, `fielddata`, `ip`, `nodeExcluded`, `ipExcluded`, `shards`, `segments`, `segmentsMemory`, `diskRead`, `diskWrite`, `networkIn`, `networkOut`, `mergeTime`, `indexingTime`, `queryCache`

**Default columns (10):** `name`, `docs`, `storeSize`, `freeSize`, `heapSize`, `load`, `cpu`, `read`, `write`, `searches`

---

### `esindices-columns` (Alkeme TUI)

DB Indices (Elasticsearch indices) tab column layout.

**Used by:** `alkeme/src/app/types.rs`, `alkeme/src/app/mod.rs`

```json
{
  "type": "esindices-columns",
  "data": {
    "columns": ["index", "docs.count", "store.size", "pri", "segmentsCount", "rep", "memoryTotal", "health", "status"],
    "order": [["index", "asc"]]
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data.columns` | string[] | Ordered list of ES index field names to display |
| `data.order` | [[string, string]] | Sort: `[[sortField, "asc"\|"desc"]]` |

**Available columns (16):** `index`, `docs.count`, `store.size`, `pri`, `segmentsCount`, `rep`, `memoryTotal`, `health`, `status`, `docs.deleted`, `pri.store.size`, `id`, `cd`, `segmentsVersion`, `ip`, `node`

**Default columns (9):** `index`, `docs.count`, `store.size`, `pri`, `segmentsCount`, `rep`, `memoryTotal`, `health`, `status`

---

### `files-columns` (Alkeme TUI)

Configures visible columns and sort order for the Files tab (PCAP file browser).

**Used by:** Alkeme TUI (Files tab column editor / layout popup)

#### Data Format

| Field | Type | Description |
|-------|------|-------------|
| `data.columns` | string[] | Ordered list of file field names to display |
| `data.order` | [[string, string]] | Sort: `[[sortField, "asc"\|"desc"]]` |

**Available columns (18):** `num`, `node`, `name`, `locked`, `first`, `filesize`, `lastTimestamp`, `encoding`, `packetPosEncoding`, `packets`, `packetsSize`, `uncompressedBits`, `cratio`, `compression`, `startTimestamp`, `finishTimestamp`, `sessionsStarted`, `sessionsPresent`

**Default columns (6):** `num`, `node`, `name`, `locked`, `first`, `filesize`
