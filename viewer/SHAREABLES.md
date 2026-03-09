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

Summary/Arkime tab widget configuration.

**Used by:** `viewer/vueapp/src/components/arkime/Arkime.vue`, `SummaryConfigDropdown.vue`, `SummaryConfigSaveModal.vue`

```json
{
  "type": "summaryConfig",
  "data": {
    "fields": [
      {
        "field": "source.ip",
        "viewMode": "bar",
        "metricType": "sessions"
      }
    ],
    "resultsLimit": 20,
    "order": "desc"
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `data.fields[].field` | string | Arkime field expression (e.g. `source.ip`, `protocol`) |
| `data.fields[].viewMode` | string | Widget display mode (`bar`, etc.). Default: `bar` |
| `data.fields[].metricType` | string | Metric to display (`sessions`, `packets`, `bytes`). Default: `sessions` |
| `data.resultsLimit` | number | Max results per widget (10, 20, 50, 100) |
| `data.order` | string | Sort order: `asc` or `desc`. Default: `desc` |

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
    "order": [["nodeName", "asc"]]
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

### 5. `files-columns` — Files Tab Column Layout

Configures visible columns and sort order for the Files tab (PCAP file browser).

**Used by:** Alkeme TUI (Files tab column editor / layout popup)

#### Data Format

| Field | Type | Description |
|-------|------|-------------|
| `data.columns` | string[] | Ordered list of file field names to display |
| `data.order` | [[string, string]] | Sort: `[[sortField, "asc"\|"desc"]]` |

**Available columns (18):** `num`, `node`, `name`, `locked`, `first`, `filesize`, `lastTimestamp`, `encoding`, `packetPosEncoding`, `packets`, `packetsSize`, `uncompressedBits`, `cratio`, `compression`, `startTimestamp`, `finishTimestamp`, `sessionsStarted`, `sessionsPresent`

**Default columns (6):** `num`, `node`, `name`, `locked`, `first`, `filesize`
