# SupplyGuard â€” `src/` Documentation

> Complete reference for the architecture, components, and data flows inside the `src/` directory.

---

## Table of Contents

1. [Directory Structure](#1-directory-structure)
2. [Technology Stack](#2-technology-stack)
3. [Backend Architecture](#3-backend-architecture)
   - [HTTP Server (`main.cpp`)](#31-http-server-maincpp)
   - [Database Layer](#32-database-layer)
   - [Domain Models](#33-domain-models)
   - [Service Layer](#34-service-layer)
   - [API Handler](#35-api-handler)
4. [Frontend Architecture](#4-frontend-architecture)
   - [HTML Structure](#41-html-structure)
   - [CSS Design System](#42-css-design-system)
   - [JavaScript Application](#43-javascript-application)
5. [Complete API Reference](#5-complete-api-reference)
6. [Core Workflows](#6-core-workflows)
   - [Startup Sequence](#61-startup-sequence)
   - [Dashboard Load](#62-dashboard-load)
   - [Impact Score Computation](#63-impact-score-computation)
   - [Route Recommendation](#64-route-recommendation)
   - [Cold-Chain Classification](#65-cold-chain-classification)
   - [Simulation Scenarios](#66-simulation-scenarios)
   - [Approve / Reject Flow](#67-approve--reject-flow)
7. [Database Schema](#7-database-schema)
8. [Test Suite](#8-test-suite)
9. [Build & Deploy](#9-build--deploy)

---

## 1. Directory Structure

```
src/
â”œâ”€â”€ backend/
â”‚   â”œâ”€â”€ main.cpp                    # TCP HTTP server + route registration
â”‚   â”œâ”€â”€ api/
â”‚   â”‚   â”œâ”€â”€ api_handler.h           # Handler function declarations
â”‚   â”‚   â””â”€â”€ api_handler.cpp         # All REST endpoint implementations
â”‚   â”œâ”€â”€ database/
â”‚   â”‚   â”œâ”€â”€ database.h              # RAII SQLite wrapper interface
â”‚   â”‚   â””â”€â”€ database.cpp            # Schema creation, seed data, query helpers
â”‚   â”œâ”€â”€ models/
â”‚   â”‚   â”œâ”€â”€ shipment.h              # Shipment struct + enums
â”‚   â”‚   â”œâ”€â”€ disruption.h            # Disruption struct + enums
â”‚   â”‚   â”œâ”€â”€ route.h                 # Route struct
â”‚   â”‚   â”œâ”€â”€ carrier.h               # Carrier struct
â”‚   â”‚   â”œâ”€â”€ fleet_asset.h           # FleetAsset struct + enums
â”‚   â”‚   â”œâ”€â”€ cold_chain.h            # ColdChainAlert, SensorReading, Config
â”‚   â”‚   â””â”€â”€ recommendation.h        # Recommendation struct + enums
â”‚   â””â”€â”€ services/
â”‚       â”œâ”€â”€ shipment_service.cpp/.h      # Impact scoring, affected shipment logic
â”‚       â”œâ”€â”€ disruption_service.cpp/.h    # Active disruption queries + mutations
â”‚       â”œâ”€â”€ route_service.cpp/.h         # Route recommendation algorithm
â”‚       â”œâ”€â”€ carrier_service.cpp/.h       # Carrier recommendation algorithm
â”‚       â”œâ”€â”€ fleet_service.cpp/.h         # Fleet utilisation + redeployment
â”‚       â”œâ”€â”€ cold_chain_service.cpp/.h    # Temperature excursion classification
â”‚       â”œâ”€â”€ recommendation_service.cpp/.h# Aggregates all recommendation types
â”‚       â””â”€â”€ simulation_service.cpp/.h    # What-if scenario engine
â”œâ”€â”€ frontend/
â”‚   â”œâ”€â”€ index.html                  # Single-page app shell
â”‚   â”œâ”€â”€ css/dashboard.css           # Complete design system
â”‚   â””â”€â”€ js/dashboard.js             # All UI logic, state, API calls
â”œâ”€â”€ tests/
â”‚   â”œâ”€â”€ test_main.cpp               # Custom TAP runner + entry point
â”‚   â”œâ”€â”€ test_disruption.cpp
â”‚   â”œâ”€â”€ test_routing.cpp
â”‚   â”œâ”€â”€ test_carrier.cpp
â”‚   â”œâ”€â”€ test_fleet.cpp
â”‚   â””â”€â”€ test_cold_chain.cpp
â”œâ”€â”€ third_party/
â”‚   â”œâ”€â”€ nlohmann/json.hpp           # Header-only JSON library
â”‚   â””â”€â”€ sqlite/sqlite3.c/.h         # Bundled SQLite amalgamation
â”œâ”€â”€ build.bat                       # Windows MinGW build script
â””â”€â”€ build.sh                        # Linux/macOS build script
```

> **Deploy note:** `src/build/bin/frontend/` is the directory the backend actually serves. After editing `src/frontend/` files, copy them to `src/build/bin/frontend/`.

---

## 2. Technology Stack

| Layer | Technology | Notes |
|---|---|---|
| Backend language | C++17 | Compiled with MinGW g++ on Windows |
| HTTP server | Hand-rolled TCP (raw sockets) | No external framework |
| Database | SQLite 3 (`:memory:`) | Bundled amalgamation; resets on every restart |
| JSON | nlohmann/json (header-only) | Used only in `api_handler.cpp` |
| Frontend | Vanilla HTML / CSS / JS | No framework, no bundler |
| Charts | Chart.js 4.4 (CDN) | Doughnut + line charts |
| Tests | Custom TAP-style runner | No gtest or catch2 |

---

## 3. Backend Architecture

### 3.1 HTTP Server (`main.cpp`)

The server is built entirely from BSD sockets with no HTTP framework. It is **single-threaded** â€” one connection is handled at a time.

#### Core types

```cpp
struct HttpRequest  { string method, path, body; map<string,string> headers; };
struct HttpResponse { int status; string content_type, body; };
using  Handler = function<HttpResponse(const HttpRequest&)>;
```

#### Request lifecycle

```
accept() â†’ handleConnection()
              â†’ recv() loop (reads until \r\n\r\n + Content-Length bytes)
              â†’ parseRequest()   â€” splits method/path/headers/body
              â†’ dispatch()       â€” linear scan of g_routes vector
                   â†’ matchPath() â€” segments URL by '/', matches {param} placeholders
                   â†’ route.handler(req)
              â†’ buildResponse()  â€” serialises status line + CORS headers
              â†’ send()
              â†’ CLOSE_SOCKET()
```

#### Path parameter extraction

Matched params are injected into the request headers under special keys:
- `req.headers["_param_shipment_id"]`
- `req.headers["_param_id"]`

#### Static file serving

Three hardcoded routes serve frontend files relative to `frontend_dir` (defaults to `./frontend`, resolved from CWD at launch):

```
GET /                    â†’ frontend_dir/index.html
GET /css/dashboard.css   â†’ frontend_dir/css/dashboard.css
GET /js/dashboard.js     â†’ frontend_dir/js/dashboard.js
```

Adding a new frontend file requires a new `addRoute()` call in `main.cpp`.

#### Startup sequence

```
1. Parse CLI args (--port, --db, --frontend)
2. WSAStartup() on Windows
3. initDB(db_path)         â€” creates schema + seeds all data
4. Instantiate 8 services  â€” all take db::Database& by reference
5. Register ~20 API routes + 4 static file routes
6. bind() â†’ listen() â†’ accept() loop
```

---

### 3.2 Database Layer

**Files:** [`database.h`](../src/backend/database/database.h), [`database.cpp`](../src/backend/database/database.cpp)

#### Class interface

```cpp
class Database {
    void exec(const string& sql);                          // fire-and-forget
    void query(const string& sql, RowCallback callback);   // row-by-row callback
    void seedData();                                       // called in constructor
    void resetToSeedState();                               // used by simulation reset
    sqlite3* handle();                                     // raw handle (rarely needed)
};
Database& getDB();           // singleton accessor â€” throws if initDB() not called
void initDB(const string&);  // must be called before getDB()
```

#### Query callback signature

```cpp
// callback(column_count, values[], column_names[])
db_.query("SELECT * FROM shipments", [&](int c, const char** v, const char** n) {
    string id = v[0] ? v[0] : "";
    // ...
});
```

#### Key behaviours

- **Always `:memory:`** in production â€” all data is lost on server restart
- Constructor calls `createSchema()` â†’ `seedData()` every time
- `resetToSeedState()` executes `DELETE FROM <table>` + re-runs all seed functions
- All raw SQL is concatenated strings (no prepared statement API exposed to services)
- `exec()` throws `std::runtime_error` on any SQLite error

---

### 3.3 Domain Models

All models live in `backend/models/` as **header-only plain structs**. They contain no database logic and no JSON serialisation â€” that belongs to the service and API layers respectively.

Every model that uses enums provides static converter methods:

```cpp
static string statusToString(ShipmentStatus s);
static string priorityToString(ShipmentPriority p);
static string riskLabel(int score);   // 0-24â†’LOW, 25-49â†’MEDIUM, 50-74â†’HIGH, 75+â†’CRITICAL
```

#### Model summary

| Model | Notable fields |
|---|---|
| `Shipment` | `impact_score` (0â€“100), `is_affected`, `risk_level`, `impact_reasons` â€” computed at query time, not stored in DB |
| `Disruption` | `affected_routes: vector<string>` â€” parsed from comma-separated DB column |
| `Route` | `is_disrupted`, `risk_score`, `refrigerated_capable` |
| `Carrier` | `reliability_score`, `cold_chain_capable`, `is_disrupted` |
| `FleetAsset` | `FleetAssetType` (TRUCK/CONTAINER/VESSEL/AIR_FREIGHT), `utilisation_pct` |
| `ColdChainAlert` | `excursion_duration_minutes`, `excursion_count`, `ExcursionSeverity` (NORMAL/WARNING/HIGH/CRITICAL) |
| `Recommendation` | `RecommendationType` (6 types), `RecommendationPriority` (4 levels) |

---

### 3.4 Service Layer

Eight services, each taking `db::Database&` by reference. They are **stateless between calls** â€” no member caching, no shared mutable state.

---

#### `ShipmentService` â€” Impact Engine

The most central service. Called on every dashboard and recommendation request.

**`getAffectedShipments(active_disruptions)`**

```
1. getAllShipments()
2. Collect all disrupted route IDs from active_disruptions
3. Query DB for all carriers WHERE is_disrupted=1
4. For each non-DELIVERED shipment:
     if route_hit OR carrier_hit OR expected_delay_hours > 0:
         computeImpactScore() â†’ if score > 0: mark affected
5. Sort by impact_score DESC
```

**Impact score formula (0â€“100 cap):**

| Condition | Score |
|---|---|
| Carrier is disrupted | +20 |
| Current route in disruption's affected_routes | +30 |
| Delay â‰¥ 48h | +20 |
| Delay â‰¥ 24h | +12 |
| Delay â‰¥ 6h | +6 |
| CRITICAL priority | +15 |
| HIGH priority | +10 |
| Cold-chain shipment | +10 |
| Status == DISRUPTED | +5 |

---

#### `RouteService` â€” Recommendation Algorithm

**`recommend(shipment_id, current_route, origin, destination, needs_refrigeration)`**

```
1. Fetch current route from DB
2. Query: SELECT * FROM routes WHERE is_disrupted=0 AND id != current
          [AND refrigerated=1 if cold-chain]
          ORDER BY risk_score ASC, estimated_hours ASC
3. Filter candidates: origin AND destination must match current route's values
4. Fallback: if no exact match, accept any route sharing origin OR destination (â‰¤3)
5. Score candidates: risk_score Ã— 100 + estimated_hours â†’ pick lowest
6. Return: recommended route + additional_hours = max(0, best.hours - current.hours)
```

---

#### `CarrierService` â€” Recommendation Algorithm

Scores available carriers by:
```
score = (reliability / 100.0) * 50
      + (available_capacity >= required ? 30 : 0)
      + (cold_chain_capable && needed ? 20 : 0)
      - (avg_delay_hours * 2)
```
Skips disrupted carriers and those below required capacity.

---

#### `ColdChainService` â€” Temperature Excursion Classifier

**`getAlert(shipment_id)`** pipeline:

```
1. getConfig()    â€” min/max temp + warning/critical margins + allowed_excursion_minutes
2. getReadings()  â€” ordered sensor readings (assumed 30-min intervals)
3. Compute: peak_temp, excursion_duration_minutes, excursion_count
4. classify() â†’ ExcursionSeverity
5. Build severity_reason string + recommended_action text
```

**Severity classification logic:**

```
CRITICAL if: magnitude â‰¥ critical_margin (default 3Â°C above/below range)
          OR excursion_minutes â‰¥ allowed Ã— 3
          OR â‰¥ 3 excursion events AND exceeded allowed
          OR minutes_to_delivery < 120 AND exceeded allowed
HIGH     if: magnitude â‰¥ warning_margin Ã— 2
          OR excursion_minutes â‰¥ allowed
          OR â‰¥ 2 excursion events
WARNING  if: magnitude â‰¥ warning_margin OR any excursion > 0
NORMAL   if: no excursion
```

---

#### `RecommendationService` â€” Aggregator

**`generateAll()`** runs 4 passes on every call:

| Pass | Source | Output type |
|---|---|---|
| 1 | Affected shipments with `impact_score â‰¥ 25` | `REROUTE_SHIPMENT`, `CHANGE_CARRIER`, `PRIORITISE_SHIPMENT` |
| 2 | `FleetService.getRedeploymentRecommendations()` | `REDEPLOY_FLEET` |
| 3 | Non-NORMAL cold-chain alerts | `COLD_CHAIN_REVIEW` |
| 4 | All active disruptions | `MONITOR_DISRUPTION` |

Results sorted CRITICAL â†’ HIGH â†’ MEDIUM â†’ LOW.

**`generateBobSummary()`** builds the natural-language text blob for the AI assistant â€” plain string concatenation using the same data sources as `generateAll()`.

---

#### `SimulationService` â€” What-If Engine

**`applyScenario(scenario_type, target)`** snapshots KPIs before, mutates the DB, snapshots after, returns a diff.

| Scenario | SQL mutations |
|---|---|
| `weather_disruption` | Insert disruption; mark R05+R13 `is_disrupted=1`; `UPDATE shipments SET status='DISRUPTED', delay+=24; C02 carrier shipments AT_RISK; Ahmedabad fleet MAINTENANCE |
| `road_closure` | Insert disruption; mark R06; `status=DISRUPTED, delay+=12`; R10 delay+=4; Delhi fleet utilisation +20% |
| `port_strike` | Insert disruption; mark R09+R14; `status=DISRUPTED, delay+=72; R15 AT_RISK; Chennai fleet IDLE` |
| `carrier_unavailable` | `UPDATE carriers SET is_disrupted=1`; affected shipments â†’ `AT_RISK` |
| `fleet_unavailable` | `UPDATE fleet_assets SET status='MAINTENANCE', utilisation_pct=0` |
| `demand_increase` | `INSERT OR IGNORE INTO shipments` â€” 5 new rows (SH2001â€“SH2005) |
| `temperature_excursion` | `DELETE` existing readings; `INSERT` out-of-range sequence for target shipment |
| `reset` | `db_.resetToSeedState()` â€” full re-seed |

---

#### `DisruptionService`

- `getActiveDisruptions()` â€” `WHERE status != 'RESOLVED'`
- `getAffectedShipmentIds(disruption)` â€” joins shipments on `current_route IN (affected_routes)`
- `addDisruption(d)` â€” inserts new row, returns new id
- `setRouteDisrupted(route_id, bool, reason)` â€” `UPDATE routes SET is_disrupted=â€¦`
- `setCarrierDisrupted(carrier_id, bool, reason)` â€” `UPDATE carriers SET is_disrupted=â€¦`

---

#### `FleetService`

- `getAllAssets()` / `getIdleAssets()` â€” `WHERE status='IDLE'`
- `getUtilisation()` â€” aggregates total/active/idle/maintenance counts + avg `utilisation_pct`
- `getRedeploymentRecommendations()` â€” finds idle assets in cities that have â‰¥2 disrupted/delayed shipments
- `updateStatus(asset_id, status)` â€” direct UPDATE

---

### 3.5 API Handler

**File:** [`api_handler.cpp`](../src/backend/api/api_handler.cpp)

Pure translation layer â€” no business logic. Each handler:
1. Parses JSON body via `nlohmann/json` (POST only)
2. Calls one or more services
3. Serialises using private static `*ToJson()` helpers
4. Returns `std::string`

**Bob AI handler** (`handleBobQuery`) â€” keyword-based NLP, no ML:
```
toLower(question) â†’ string::find() on keywords:
  "summary" / "what should we do"   â†’ rec_svc.generateBobSummary()
  "affected" + "shipment"            â†’ list affected shipments
  "highest risk" / "worst"           â†’ top affected shipment detail
  "route" + "alternative/recommend"  â†’ route rec for top shipment
  "carrier" + "change/switch"        â†’ carrier rec for top shipment
  "idle" / "redeploy"                â†’ fleet idle list
  "cold" / "temperature"             â†’ cold-chain alert summary
  (fallback)                         â†’ generateBobSummary()
```

---

## 4. Frontend Architecture

### 4.1 HTML Structure

[`index.html`](../src/frontend/index.html) is a single-page app with 8 `<section>` elements. Only one is `display:block` at a time.

```
<header>           â€” fixed top bar (logo, status, buttons)
<nav>              â€” fixed below header (tab buttons + Ask Bob)
<main>
  #section-overview        â€” KPI cards, active disruptions, top shipments, priority recs
  #section-disruptions     â€” disruption cards grid
  #section-shipments       â€” filterable table + slide-in detail panel
  #section-fleet           â€” fleet stats + asset table + redeployment list + doughnut chart
  #section-coldchain        â€” cold-card grid + temperature line chart
  #section-recommendations  â€” route rec panel, carrier rec panel, all recs list
  #section-simulation      â€” scenario cards
  #section-bob             â€” chat interface
```

The shipment detail panel (`#shipmentDetail`) is a fixed right-side drawer with a **sticky header** â€” `.detail-header` is `flex-shrink:0` so the close button never scrolls away, and `#detailContent` scrolls independently.

---

### 4.2 CSS Design System

**File:** [`dashboard.css`](../src/frontend/css/dashboard.css)

CSS custom properties (design tokens):

```css
--navy:    #0f172a   /* header background */
--accent:  #2563eb   /* primary blue */
--success: #16a34a
--warning: #d97706
--danger:  #dc2626
--critical:#7c3aed
--cold:    #0891b2
--orange:  #ea580c
--header-h: 60px
--nav-h:    46px
```

Key layout rules:
- Header: `position:fixed; background:var(--navy)` â€” dark navy
- Nav: `position:fixed; top:var(--header-h); background:white` â€” white bar
- Main: `margin-top: calc(var(--header-h) + var(--nav-h)); padding:28px 24px`
- KPI cards: `flex-direction:column` with `::before` pseudo-element as top colour strip
- Panel titles: `border-left:3px solid var(--accent)` â€” blue left accent
- Badges: `border-radius:5px` â€” slightly square
- Detail panel: `display:flex; flex-direction:column` â€” header pinned, content scrolls

---

### 4.3 JavaScript Application

**File:** [`dashboard.js`](../src/frontend/js/dashboard.js)

#### State object

```js
let state = {
  dashboard:      {},      // /api/dashboard response
  allShipments:   [],      // /api/shipments â€” all shipments; patched locally on approval
  disruptions:    [],
  fleet:          [],
  coldAlerts:     [],
  recommendations:[],
  shipmentFilter: 'all',
  decisions: {             // keyed "route:SH1001" or "carrier:SH1001"
    "route:SH1001": {
      status:    "approved" | "rejected",
      value:     "R15",          // new route/carrier value
      label:     "Route name",
      at:        "14:32:07",     // toLocaleTimeString()
      note:      "Route changed to R15 (Mumbaiâ€“Chennai Rail Express)"
    }
  },
  tempChart:  null,        // Chart.js instance
  fleetChart: null,
  activeTempShipment: null
}
```

#### Key functions

| Function | Purpose |
|---|---|
| `init()` | Health check â†’ `refreshAll()` â†’ hide loading overlay; shows offline screen on failure |
| `refreshAll()` | 6 parallel `fetch()` calls â†’ populates state â†’ calls all 6 render functions |
| `showSection(name, btn)` | Toggles `.active` class on sections and nav buttons |
| `renderOverview()` | Writes KPI values, disruption list, top shipment table, priority recs |
| `renderShipments()` | Applies `state.shipmentFilter`, renders table rows with decision badges |
| `showShipmentDetail(id)` | Opens slide-in panel, fetches route rec, shows decision history |
| `renderFleet()` | Fleet stats bar, asset table with utilisation bars, redeployment list, doughnut chart |
| `renderColdChain()` | Cold cards, selects first shipment for temp chart |
| `renderRecommendations()` | Populates both dropdowns (all shipments, affected first with âš ) |
| `loadRouteRecommendation()` | Fetches rec, shows Approve/Reject buttons or locked banner |
| `loadCarrierRecommendation()` | Same pattern as route rec |
| `approveRec(type, id, value, label)` | Records decision, patches `state.allShipments`, re-renders |
| `rejectRec(type, id)` | Records decision, re-renders panel with locked banner |
| `runSimulation(scenario, target)` | POST to `/api/simulation`, shows before/after diff, calls `refreshAll()` |
| `sendBobMessage()` | POST to `/api/bob/query`, renders response in chat |
| `escapeHtml(str)` | For DOM text injection |
| `escapeAttr(str)` | For inline `onclick="..."` attribute values â€” different from `escapeHtml` |

#### Approve/Reject decision flow

```
User clicks "âœ“ Approve Route Change"
  â†“
approveRec('route', 'SH1001', 'R15', 'Mumbaiâ€“Chennai Rail Express')
  â†“
state.decisions['route:SH1001'] = { status:'approved', value:'R15', at:'14:32:07', ... }
  â†“
Patch state.allShipments[SH1001]:
  .current_route = 'R15'
  .status        = 'ON_TIME'
  .expected_delay_hours = 0
  .risk_level    = 'LOW'
  .impact_score  = 0
  â†“
renderShipments()   â† table row shows green âœ“ badge next to route
loadRouteRecommendation()  â† panel shows locked "âœ“ Approved" banner
```

> **Important:** Decisions and patched shipment state exist only in `state` for the browser session. The backend never receives these changes. Page refresh resets everything.

---

## 5. Complete API Reference

| Method | Path | Handler | Description |
|---|---|---|---|
| GET | `/api/health` | `handleHealth` | Service status check |
| GET | `/api/dashboard` | `handleDashboard` | KPIs + disruptions + affected shipments + fleet + cold-chain + recommendations |
| GET | `/api/shipments` | `handleGetShipments` | All shipments with computed impact fields |
| GET | `/api/shipments/affected` | `handleGetAffectedShipments` | Affected shipments only |
| GET | `/api/disruptions` | `handleGetDisruptions` | All disruptions |
| GET | `/api/routes` | `handleGetRoutes` | All routes |
| POST | `/api/routes/recommend` | `handlePostRouteRecommend` | `{shipment_id}` â†’ route recommendation |
| GET | `/api/carriers` | `handleGetCarriers` | All carriers |
| POST | `/api/carriers/recommend` | `handlePostCarrierRecommend` | `{shipment_id, required_capacity_kg}` â†’ carrier recommendation |
| GET | `/api/fleet` | `handleGetFleet` | All fleet assets |
| GET | `/api/fleet/idle` | `handleGetFleetIdle` | Idle assets only |
| GET | `/api/fleet/utilisation` | `handleGetFleetUtilisation` | Aggregated utilisation stats |
| POST | `/api/fleet/redeploy` | `handlePostFleetRedeploy` | `{asset_id, target_location}` â†’ initiates redeployment |
| GET | `/api/cold-chain/alerts` | `handleGetColdChainAlerts` | All cold-chain alerts sorted by severity |
| GET | `/api/cold-chain/{shipment_id}` | `handleGetColdChainShipment` | Single shipment alert |
| GET | `/api/cold-chain/{shipment_id}/temperature` | `handleGetColdChainTemperature` | Sensor readings + config |
| POST | `/api/simulation` | `handlePostSimulation` | `{scenario, target?}` â†’ apply scenario, return before/after diff |
| GET | `/api/recommendations` | `handleGetRecommendations` | All active recommendations |
| POST | `/api/bob/query` | `handleBobQuery` | `{question}` â†’ natural language answer |
| GET | `/` | serveFile | `index.html` |
| GET | `/css/dashboard.css` | serveFile | Stylesheet |
| GET | `/js/dashboard.js` | serveFile | Application script |

---

## 6. Core Workflows

### 6.1 Startup Sequence

```
main()
  â”œâ”€â”€ Parse CLI: --port (8080), --db (:memory:), --frontend (./frontend)
  â”œâ”€â”€ WSAStartup()  [Windows only]
  â”œâ”€â”€ db::initDB(":memory:")
  â”‚     â””â”€â”€ Database::Database()
  â”‚           â”œâ”€â”€ sqlite3_open(":memory:")
  â”‚           â”œâ”€â”€ PRAGMA journal_mode=WAL
  â”‚           â”œâ”€â”€ PRAGMA foreign_keys=ON
  â”‚           â”œâ”€â”€ createSchema()   â€” CREATE TABLE IF NOT EXISTS Ã— 7
  â”‚           â””â”€â”€ seedData()
  â”‚                 â”œâ”€â”€ seedDisruptions()   â€” 5 disruptions
  â”‚                 â”œâ”€â”€ seedRoutes()        â€” 15+ routes
  â”‚                 â”œâ”€â”€ seedCarriers()      â€” 7 carriers
  â”‚                 â”œâ”€â”€ seedShipments()     â€” 24 base shipments
  â”‚                 â”œâ”€â”€ seedFleetAssets()   â€” 30 assets
  â”‚                 â”œâ”€â”€ seedColdChainConfig()â€” 20 configs
  â”‚                 â””â”€â”€ seedSensorReadings()â€” readings per cold-chain shipment
  â”œâ”€â”€ Instantiate 8 service objects (each receives db& ref)
  â”œâ”€â”€ Register ~24 routes via addRoute()
  â””â”€â”€ socket() â†’ bind() â†’ listen() â†’ accept() loop
```

---

### 6.2 Dashboard Load

```
Browser: GET /api/dashboard
  â†“
handleDashboard(db, dis, ship, fleet, cold, rec)
  â”œâ”€â”€ dis.getActiveDisruptions()      â†’ SELECT WHERE status != 'RESOLVED'
  â”œâ”€â”€ ship.getAffectedShipments()     â†’ computeImpactScore() per shipment
  â”œâ”€â”€ fleet.getUtilisation()          â†’ aggregate fleet stats
  â”œâ”€â”€ fleet.getIdleAssets()           â†’ WHERE status='IDLE'
  â”œâ”€â”€ cold.getAllAlerts()             â†’ classify() per cold-chain shipment
  â””â”€â”€ rec.generateAll()               â†’ 4-pass recommendation engine
  â†“
Serialise to JSON:
  kpi.{active_disruptions, affected_shipments, fleet_utilisation_pct, ...}
  disruptions[]
  affected_shipments[]
  idle_fleet[]
  cold_chain_alerts[]
  recommendations[]
  fleet_utilisation.{total, active, idle, average_pct, redeployment_opportunities}
```

---

### 6.3 Impact Score Computation

Called once per non-DELIVERED shipment in `getAffectedShipments()`:

```
computeImpactScore(shipment, active_disruptions)
  score = 0

  // Carrier check (1 DB query per shipment)
  if carrier.is_disrupted:  score += 20

  // Route check (in-memory, no extra query)
  for each active_disruption:
    if shipment.current_route in disruption.affected_routes:
      score += 30

  // Delay severity
  if delay >= 48h: score += 20
  elif delay >= 24h: score += 12
  elif delay >= 6h: score += 6

  // Priority
  if CRITICAL: score += 15
  elif HIGH: score += 10

  // Cold chain
  if cold_chain: score += 10

  // Status bump
  if status == DISRUPTED: score += 5

  return min(score, 100)
```

---

### 6.4 Route Recommendation

```
POST /api/routes/recommend  { "shipment_id": "SH1001" }
  â†“
ship.getById("SH1001")   â†’ current_route, origin, destination, cold_chain
  â†“
route.recommend("SH1001", "R01", "Mumbai", "Delhi", false)
  â†“
  1. Fetch current route details (estimated_hours, risk_score)
  2. SQL: SELECT * FROM routes
         WHERE is_disrupted=0 AND id != 'R01'
         [AND refrigerated=1 if cold-chain]
         ORDER BY risk_score ASC, estimated_hours ASC
  3. Filter: keep only routes where
         origin  == "Mumbai" AND destination == "Delhi"
  4. Fallback: if empty, accept routes sharing origin OR destination (â‰¤3 candidates)
  5. Score each: risk_score Ã— 100 + estimated_hours
  6. Return lowest-scored candidate
  â†“
Response:
  { found, current_route, recommended_route, recommended_route_name,
    additional_hours, cost_usd, risk_score, reason }
```

---

### 6.5 Cold-Chain Classification

```
GET /api/cold-chain/alerts
  â†“
cold.getAllAlerts()
  â†“ for each shipment in cold_chain_config:
  getConfig(id)    â†’ min_temp, max_temp, warning_margin(1Â°C), critical_margin(3Â°C),
                      allowed_excursion_minutes(30)
  getReadings(id)  â†’ ordered sensor readings (30-min intervals assumed)
  â†“
  Compute:
    peak_temp           = max temperature in all readings
    excursion_minutes   = total minutes outside [min_temp, max_temp]
    excursion_count     = number of separate excursion events
    excursion_magnitude = |peak_temp - boundary|
  â†“
  classify():
    CRITICAL â†’ magnitude â‰¥ 3Â°C  OR  minutes â‰¥ allowedÃ—3
               OR (â‰¥3 events AND minutes > allowed)
               OR (delivery < 2h AND minutes > allowed)
    HIGH     â†’ magnitude â‰¥ 2Â°C  OR  minutes â‰¥ allowed  OR  â‰¥2 events
    WARNING  â†’ magnitude â‰¥ 1Â°C  OR  any excursion
    NORMAL   â†’ no excursion
```

---

### 6.6 Simulation Scenarios

```
POST /api/simulation  { "scenario": "weather_disruption" }
  â†“
sim.applyScenario("weather_disruption", "")
  â†“
  Snapshot BEFORE:
    affected_shipments_before = ship.getAffectedShipments().size()
    idle_assets_before        = fleet.getIdleAssets().size()
    cold_chain_alerts_before  = count non-NORMAL cold alerts
  â†“
  Apply mutations (direct SQL):
    INSERT INTO disruptions ...
    UPDATE routes SET is_disrupted=1 WHERE id IN ('R05','R13')
    UPDATE shipments SET status='DISRUPTED',
           expected_delay_hours = expected_delay_hours + 18
           WHERE current_route IN ('R05','R13') AND status != 'DELIVERED'
  â†“
  Snapshot AFTER (same queries)
  â†“
  Return SimulationResult { success, message, before{}, after{} }
  â†“
Frontend: refreshAll() â€” reloads all data to reflect mutations
```

---

### 6.7 Approve / Reject Flow

This flow is **entirely frontend-only**. No backend endpoint exists for decisions.

```
User: select shipment SH1001 in Recommendations tab
  â†“
loadRouteRecommendation()
  â†’ POST /api/routes/recommend { shipment_id: "SH1001" }
  â†’ check state.decisions["route:SH1001"]
  â†’ if no decision: render Approve + Reject buttons
  â†’ if already decided: render locked decision banner
  â†“
User clicks "âœ“ Approve Route Change"
  â†“
approveRec('route', 'SH1001', 'R15', 'Mumbaiâ€“Chennai Rail Express')
  â”œâ”€â”€ state.decisions["route:SH1001"] = { status:'approved', value:'R15', at:'...' }
  â”œâ”€â”€ Find SH1001 in state.allShipments, patch:
  â”‚     current_route = 'R15'
  â”‚     status = 'ON_TIME'
  â”‚     expected_delay_hours = 0
  â”‚     risk_level = 'LOW'
  â”‚     impact_score = 0
  â”œâ”€â”€ renderShipments()  â†’ table row shows green âœ“ badge
  â””â”€â”€ loadRouteRecommendation()  â†’ panel now shows locked banner
  â†“
User opens Shipments tab â†’ clicks SH1001
  â†“
showShipmentDetail('SH1001')
  â†’ reads state.decisions['route:SH1001']
  â†’ renders "ðŸ“‹ Decision History" section
  â†’ shows "Route Recommendation â€” âœ“ Approved" card
  â†’ route rec section shows Approve button (since decision exists â†’ locked banner)
```

---

## 7. Database Schema

```sql
CREATE TABLE disruptions (
    id              INTEGER PRIMARY KEY,
    type            TEXT,           -- 'Weather Event','Port Strike','Road Closure',
                                    --   'Geopolitical Crisis','Carrier Disruption'
    name            TEXT,
    location        TEXT,
    severity        TEXT,           -- 'LOW','MEDIUM','HIGH','CRITICAL'
    start_time      TEXT,           -- 'YYYY-MM-DD HH:MM'
    end_time        TEXT,
    affected_routes TEXT,           -- comma-separated: 'R01,R02,R08'
    description     TEXT,
    status          TEXT            -- 'ACTIVE','MONITORING','RESOLVED'
);

CREATE TABLE routes (
    id              TEXT PRIMARY KEY,  -- 'R01','R02',...
    name            TEXT,
    origin          TEXT,
    destination     TEXT,
    estimated_hours REAL,
    distance_km     REAL,
    cost_usd        REAL,
    capacity_kg     INTEGER,
    is_disrupted    INTEGER,           -- 0 or 1
    disruption_reason TEXT,
    risk_score      INTEGER,           -- 0â€“100
    refrigerated    INTEGER            -- 0 or 1
);

CREATE TABLE carriers (
    id                  TEXT PRIMARY KEY,  -- 'C01','C02',...
    name                TEXT,
    type                TEXT,
    available_capacity  INTEGER,
    total_capacity      INTEGER,
    reliability         REAL,              -- 0.0â€“1.0
    cost_per_km         REAL,
    avg_delay_hours     INTEGER,
    cold_chain          INTEGER,           -- 0 or 1
    is_disrupted        INTEGER,           -- 0 or 1
    disruption_reason   TEXT,
    service_region      TEXT
);

CREATE TABLE shipments (
    id                   TEXT PRIMARY KEY,  -- 'SH1001','SH1002',...
    origin               TEXT,
    destination          TEXT,
    current_route        TEXT,
    carrier              TEXT,
    fleet_asset          TEXT,
    status               TEXT,    -- 'ON_TIME','DELAYED','AT_RISK','DISRUPTED','DELIVERED'
    priority             TEXT,    -- 'LOW','MEDIUM','HIGH','CRITICAL'
    cargo_type           TEXT,
    cold_chain           INTEGER,
    planned_departure    TEXT,
    planned_arrival      TEXT,
    current_eta          TEXT,
    expected_delay_hours INTEGER
    -- Note: impact_score, risk_level, impact_reasons are computed at query time
);

CREATE TABLE fleet_assets (
    id                  TEXT PRIMARY KEY,  -- 'CN401','T201',...
    type                TEXT,    -- 'TRUCK','CONTAINER','VESSEL','AIR_FREIGHT'
    location            TEXT,
    status              TEXT,    -- 'IN_TRANSIT','IDLE','AVAILABLE','MAINTENANCE'
    capacity_kg         INTEGER,
    current_assignment  TEXT,    -- shipment ID or empty
    availability_time   TEXT,
    utilisation_pct     INTEGER,
    refrigerated        INTEGER
);

CREATE TABLE sensor_readings (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    shipment_id   TEXT,
    timestamp     TEXT,          -- 'YYYY-MM-DD HH:MM'
    temperature   REAL,
    humidity      REAL,
    sensor_status TEXT           -- 'OK','FAULT'
);

CREATE TABLE cold_chain_config (
    shipment_id               TEXT PRIMARY KEY,
    min_temp                  REAL,
    max_temp                  REAL,
    warning_margin            REAL,   -- default 1.0Â°C
    critical_margin           REAL,   -- default 3.0Â°C
    allowed_excursion_minutes INTEGER -- default 30
);
```

---

## 8. Test Suite

**Files:** `src/tests/`

No external test framework. `test_main.cpp` defines:

```cpp
void check(bool condition, const string& test_name);
// Prints "  âœ“  test_name" or "  âœ—  test_name  <<< FAIL"
// Increments g_passed / g_failed
```

Each `test_*.cpp` defines one function and creates its own independent `db::Database(":memory:")`.

| File | Suites covered |
|---|---|
| `test_disruption.cpp` | Seed data loaded, cyclone has CRITICAL severity, R01 shipments affected |
| `test_routing.cpp` | Finds alternative route, cold-chain routes are refrigerated-capable |
| `test_carrier.cpp` | Avoids disrupted carrier, scores by reliability + capacity |
| `test_fleet.cpp` | Idle asset detection, utilisation aggregation |
| `test_cold_chain.cpp` | Excursion detection, severity thresholds, NORMAL when no excursion |

**Build and run:**
```bat
cd src
build.bat
build\bin\run_tests.exe
```

**To run a single test suite:** comment out the unwanted `testXxx()` calls in `test_main.cpp::main()` and rebuild.

**Required compiler flag:** all test files must be compiled with `-DTESTING_MODE`.

---

## 9. Build & Deploy

### Build (Windows)

```bat
cd src        â† must be in src/ (script self-cd's but is safest from here)
build.bat
```

Build order:
1. `gcc -c third_party/sqlite/sqlite3.c` â€” compiled with **gcc**, not g++
2. `g++ -c backend/**/*.cpp` â€” all backend files with `-std=c++17`
3. Link â†’ `build/bin/supply_chain_backend.exe` (links `-lws2_32 -lmswsock`)
4. Compile + link tests â†’ `build/bin/run_tests.exe`
5. `xcopy /E /Y frontend build\bin\frontend` â€” copies frontend source to serve directory

> **Gotcha:** `build.bat` prints `=== Build Complete ===` even when the linker fails (e.g. the `.exe` is running and locked). Check stderr for `Permission denied`.

### Run server

```bat
REM From anywhere â€” use the root launcher:
start_server.bat

REM Or manually (must cd first):
cd src\build\bin
supply_chain_backend.exe --frontend .\frontend
```

### Deploy frontend changes

```powershell
Copy-Item "src\frontend\index.html"        "src\build\bin\frontend\index.html"        -Force
Copy-Item "src\frontend\css\dashboard.css" "src\build\bin\frontend\css\dashboard.css" -Force
Copy-Item "src\frontend\js\dashboard.js"   "src\build\bin\frontend\js\dashboard.js"   -Force
```

> The backend serves from `src/build/bin/frontend/`, **not** from `src/frontend/` directly.

