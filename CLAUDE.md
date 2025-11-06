# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Development Commands

### Initial Setup
```bash
# Build Arkime (C capture binary + install dependencies)
./easybutton-build.sh

# Load test data and run all tests
make check
```

### Running Tests
```bash
# Capture tests (protocol parsers, PCAP processing)
cd tests && ./tests.pl

# Viewer API tests (requires Elasticsearch running)
cd tests && ./tests.pl --viewer

# Viewer UI tests (Jest + Vue Testing Library)
cd viewer && npm test

# Lint all code
npm run lint
npm run lint-fix  # Auto-fix issues
```

### Development Modes

Each component has a dev mode that runs frontend (Vite with HMR) + backend (nodemon) concurrently:

```bash
# Viewer (port 8123, anonymous mode)
npm run viewer:test

# Viewer (port 8123, with admin user)
npm run viewer:dev

# Cont3xt
npm run cont3xt:dev

# Parliament
npm run parliament:dev

# WISE
npm run wise:dev
```

### Building Frontend Bundles
```bash
# Production builds for each component
npm run viewer:bundle
npm run cont3xt:bundle
npm run parliament:bundle
npm run wise:bundle

# Minified builds (add :min)
npm run viewer:bundle:min
```

## Architecture Overview

Arkime is a modular network analysis system with these components:

### Core Components

**capture/** - C application (threaded, low-level)
- Monitors network traffic, writes PCAP files
- Parses packets, sends metadata to Elasticsearch
- Plugin system: parsers/ (protocols), plugins/ (capture methods)

**viewer/** - Node.js web application
- Express.js backend + Vue 3 frontend
- Provides web UI and API for browsing sessions
- Fetches PCAP data from capture nodes
- Main files: viewer.js (~2,200 lines), db.js (Elasticsearch layer)

**Elasticsearch/OpenSearch**
- Stores session metadata (SPI - Session Profile Information)
- Indices: sessions2-*, sessions3-*, partial-sessions3-*

### Optional Components

**cont3xt/** - Contextual intelligence gathering
- 39+ integrations (VirusTotal, Shodan, etc.) in integrations/
- Main file: cont3xt.js

**parliament/** - Multi-cluster monitoring and management
- Dashboard for managing multiple Arkime deployments
- Main file: parliament.js

**wiseService/** - Threat intelligence enrichment
- Pluggable sources (source.*.js files)
- Caching strategies: Redis, Memcached, LMDB, memory
- Main file: wiseService.js

### Shared Code

**common/** - Shared across all components
- arkimeConfig.js - Multi-format config system (.ini, .json, .yaml)
- auth.js - Authentication (digest, basic, OIDC, header, form)
- user.js - User management
- vueapp/ - Shared Vue components (Search.vue, Users.vue, etc.)
- vueapp/locales/ - i18n translations (11 languages: en, es, fr, de, ja, ko, zh, etc.)

## Frontend Architecture

### Tech Stack
- **Vue 3** - Composition API ready
- **Vite 7.x** - Modern bundler with HMR (replaces Webpack)
- **Bootstrap Vue Next** - Bootstrap 5 components
- **Vue Router 4** - SPA routing
- **Vuex 4** - State management
- **Vue i18n 11** - Internationalization

### Structure Pattern (using viewer as example)
```
viewer/vueapp/
├── src/
│   ├── main.js           # Vue app initialization
│   ├── router.js         # Routes (sessions, stats, users, hunt, etc.)
│   ├── store.js          # Vuex global state
│   └── components/       # Page-specific components
│       ├── sessions/     # ~19 components (list, detail, export)
│       ├── search/       # Query builder UI
│       ├── stats/        # Statistics dashboards
│       ├── spiview/      # SPI data visualization
│       ├── hunt/         # Saved searches
│       └── [others]/
└── dist/                 # Built output (generated)
```

All services follow this pattern. Common components live in common/vueapp/.

### Development Workflow
- Frontend: Vite dev server (port 5173) with hot reload
- Backend: nodemon auto-restarts on changes
- Proxy: Vite proxies API requests to backend

## Backend Architecture

### API Module Pattern

Each service uses Express.js with modular API files:

**Viewer API modules** (viewer/api*.js):
- apiSessions.js - Session search/retrieval
- apiStats.js - Aggregations and statistics
- apiUsers.js - User management/auth
- apiHunts.js - Saved searches
- apiConnections.js - Connection-based queries
- apiCrons.js - Scheduled searches

Pattern:
```javascript
class SessionAPIs {
  static async getSessions(req, res) { ... }
  static async buildSessionQuery(req, callback) { ... }
}
// Routes registered in main app
app.get('/api/sessions', SessionAPIs.getSessions);
```

### Database Layer

**db.js** in each service:
- Manages Elasticsearch/OpenSearch connections
- LRU caching for frequent queries
- Abstracts query building
- Index naming conventions

Key methods: Db.initialize(), Db.search(), Db.indexName(), Db.healthCheck()

### Configuration System

Unified config via common/arkimeConfig.js:
- Multi-format: .ini, .json, .yaml
- Environment variable overrides (ARKIME__section__key)
- Hierarchical sections with fallback
- Per-application customization

Main config: `/opt/arkime/etc/config.ini` (production) or `tests/config.test.ini` (dev)

### Authentication

Multiple auth modes via common/auth.js:
- anonymous, basic, digest, header, OIDC, form
- Chaining: `header+digest`
- Role-based access control (RBAC)
- Elasticsearch-backed user store

## Data Flow

### Session Capture → View Flow
```
[Capture Binary]
  ↓ writes PCAP to disk
[Local Disk Storage]
  ↓ sends metadata
[Elasticsearch/OpenSearch]
  ↓ queried by
[Viewer Web Service]
  ↓ provides API
[Vue Frontend]
  ↓ fetches PCAP from
[Viewer PCAP Handler]
```

### Intelligence Enrichment Flow
```
[Viewer/Systems]
  ↓ queries
[WISE Service]
  ↓ aggregates from
[Threat Intel Sources: VirusTotal, Shodan, Splunk, ES, files, etc.]
  ↓ caches in
[Redis/Memcached/LMDB/Memory]
  ↓ returns to
[Viewer Display]
```

## Testing Infrastructure

### Test Framework
Custom Perl framework: tests/ArkimeTest.pm (~26,000 lines)
- 47 test files (*.t)
- Uses LWP::UserAgent for HTTP testing
- Helper functions: viewerGet(), viewerPost(), esGet(), countTest()

### Test Types
```
tests/
├── *.t files            # Individual test suites
│   ├── api-sessions.t   # Session API tests
│   ├── api-stats.t      # Stats API tests
│   ├── dns.t            # Protocol parser tests
│   └── [47 total]
├── config.test.ini      # Viewer test config
├── config.test.json     # WISE test config
└── cont3xt.ini          # Cont3xt test config
```

Run capture tests: `cd tests && ./tests.pl`
Run viewer tests: `cd tests && ./tests.pl --viewer`
Run single test: `cd tests && ./tests.pl api-sessions.t`

## Key Architectural Patterns

### Monorepo Structure
- Root package.json coordinates all components
- Each service has own package.json
- Shared code in common/
- Independent deployment possible

### Plugin Architecture
- **Capture**: Plugins for packet capture methods (pfring, daq, kafka, snf, lua)
- **Cont3xt**: 39+ intelligence integrations (integrations/)
- **WISE**: Pluggable threat sources (source.*.js)
- Standardized interface for each plugin type

### Caching Strategy
- Multi-layer: LRU in-memory, Redis, Memcached, LMDB
- Query result caching in db.js
- Frontend state caching with Vuex

### Component Communication
- Viewer ↔ Capture nodes: HTTP API (port 8005)
- All components ↔ Elasticsearch: REST API (ports 9200-920x)
- Frontend ↔ Backend: REST API + WebSocket (for live updates)

## Important Locations

### API Endpoints
- Viewer APIs: viewer/api*.js (apiSessions, apiStats, apiUsers, etc.)
- Cont3xt APIs: cont3xt/cont3xt.js
- Parliament APIs: parliament/parliament.js
- WISE APIs: wiseService/wiseService.js

### Vue Components
- Service-specific: [service]/vueapp/src/components/
- Shared components: common/vueapp/
- Key shared: Search.vue, Users.vue, Notifiers.vue

### Configuration
- Production: /opt/arkime/etc/config.ini
- Development: tests/config.test.ini, tests/config.test.json
- Templates: release/config.ini.sample

### Build System
- C compilation: Makefile (generated by configure.ac)
- Frontend bundling: Vite configs in each [service]/vueapp/
- Root orchestration: package.json scripts

## Internationalization

Arkime supports 11 languages through Vue i18n. Translation files in common/vueapp/locales/:
- en.json (English - default)
- es.json (Spanish), fr.json (French), de.json (German)
- ja.json (Japanese), ko.json (Korean), zh.json (Chinese)
- And more...

See INTERNATIONALIZATION.md for adding/updating translations.

## Development Notes

### Node.js Version
Requires Node 22.15.0 or higher (< 23). Use nvm to manage versions.

### Elasticsearch/OpenSearch
Local instance required for development. See CHANGELOG for compatible versions.

### Browser Access
- Viewer dev: http://localhost:8123
- Default test credentials: admin/admin (when using viewer:dev)
- Anonymous mode: Use viewer:test

### Nodemon & Vite
Development commands use nodemon (backend) + vite (frontend) concurrently. Changes to .js files restart backend, changes to .vue files trigger HMR.

### Git Hooks (Optional)
Install local hooks for pre-commit linting:
```bash
git config --local core.hooksPath .githooks/
```

## Common Development Patterns

### Adding a New API Endpoint
1. Create or modify api*.js file in appropriate service
2. Add route in main service file (viewer.js, cont3xt.js, etc.)
3. Document with JSDoc (METHOD, @name, @param, @returns)
4. Add test in tests/*.t file
5. Run `npm run [service]:doc` to update API docs

### Adding a New Vue Component
1. Create .vue file in appropriate components/ directory
2. If shared across services, put in common/vueapp/
3. Import in parent component or router
4. Use Composition API for new components
5. Add i18n translations to common/vueapp/locales/*.json

### Adding a Cont3xt Integration
1. Create new file in cont3xt/integrations/
2. Extend Integration class from integration.js
3. Implement doFetch() method
4. Register in cont3xt.js
5. Add tests

### Adding a WISE Source
1. Create source.*.js in wiseService/
2. Extend WISESource class
3. Implement required methods
4. Configure in config file
5. Add documentation
