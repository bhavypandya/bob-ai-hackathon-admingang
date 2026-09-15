# Architecture

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         BROWSER / CLIENT                            │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │              Frontend Dashboard (HTML/CSS/JS)                │   │
│  │                                                              │   │
│  │  ┌─────────┐ ┌──────────┐ ┌───────┐ ┌──────────┐ ┌──────┐  │   │
│  │  │Overview │ │Shipments │ │ Fleet │ │ColdChain │ │ Bob  │  │   │
│  │  └────┬────┘ └────┬─────┘ └───┬───┘ └────┬─────┘ └──┬───┘  │   │
│  │       └───────────┴───────────┴───────────┴──────────┘      │   │
│  │                         REST API calls (fetch)               │   │
│  └──────────────────────────────┬───────────────────────────────┘   │
└─────────────────────────────────┼───────────────────────────────────┘
                                  │ HTTP/JSON
                                  ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    C++ BACKEND (supply_chain_backend)               │
│                                                                     │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                    HTTP Router (main.cpp)                    │   │
│  │  GET  /api/dashboard    GET  /api/shipments/affected         │   │
│  │  GET  /api/disruptions  POST /api/routes/recommend           │   │
│  │  GET  /api/fleet/idle   POST /api/carriers/recommend         │   │
│  │  GET  /api/cold-chain/  POST /api/simulation                 │   │
│  │  POST /api/bob/query    GET  /api/recommendations            │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │                                           │
│  ┌──────────────────────▼───────────────────────────────────────┐   │
│  │                    Service Layer                             │   │
│  │                                                              │   │
│  │  ┌────────────────┐  ┌────────────────┐  ┌───────────────┐  │   │
│  │  │ DisruptionSvc  │  │  ShipmentSvc   │  │  RouteSvc     │  │   │
│  │  │ · getActive()  │  │ · getAffected()│  │ · recommend() │  │   │
│  │  │ · setStatus()  │  │ · scoreImpact()│  │ · getAll()    │  │   │
│  │  └───────┬────────┘  └───────┬────────┘  └───────┬───────┘  │   │
│  │          │                   │                   │           │   │
│  │  ┌───────▼────────┐  ┌───────▼────────┐  ┌───────▼───────┐  │   │
│  │  │  CarrierSvc    │  │   FleetSvc     │  │ ColdChainSvc  │  │   │
│  │  │ · recommend()  │  │ · getIdle()    │  │ · getAlert()  │  │   │
│  │  │ · scoreCarrier │  │ · getRedeploy()│  │ · classify()  │  │   │
│  │  └───────┬────────┘  └───────┬────────┘  └───────┬───────┘  │   │
│  │          │                   │                   │           │   │
│  │  ┌───────▼───────────────────▼───────────────────▼───────┐   │   │
│  │  │             RecommendationService                      │   │   │
│  │  │  · generateAll()   · generateBobSummary()              │   │   │
│  │  └───────────────────────────┬───────────────────────────┘   │   │
│  │                              │                               │   │
│  │  ┌───────────────────────────▼───────────────────────────┐   │   │
│  │  │              SimulationService                         │   │   │
│  │  │  · applyScenario()  · reset()                          │   │   │
│  │  └───────────────────────────────────────────────────────┘   │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
│                         │                                           │
│  ┌──────────────────────▼───────────────────────────────────────┐   │
│  │                  Database Layer (database.cpp)               │   │
│  │     · exec()  · query()  · seedData()  · resetToSeedState()  │   │
│  └──────────────────────┬───────────────────────────────────────┘   │
└─────────────────────────┼───────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     SQLite Database (in-memory)                     │
│                                                                     │
│  disruptions  │  routes  │  carriers  │  shipments                  │
│  fleet_assets │  sensor_readings      │  cold_chain_config          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Data Flow

### Disruption → Impact Analysis

```
User triggers disruption (or seed data loaded)
        ↓
GET /api/disruptions → DisruptionService.getActiveDisruptions()
        ↓
For each disruption: extract affected_routes[]
        ↓
GET /api/shipments/affected →
  ShipmentService.getAffectedShipments(disruptions)
  For each shipment:
    - Check if current_route is in disrupted routes
    - Check if carrier is_disrupted
    - Calculate impact score (0-100) with reasons
    - Assign risk level (LOW/MEDIUM/HIGH/CRITICAL)
        ↓
JSON response with sorted shipments (highest score first)
```

### Route Recommendation

```
POST /api/routes/recommend {shipment_id: "SH1001"}
        ↓
RouteService.recommend(shipment_id, current_route, origin, destination, cold_chain)
        ↓
Query: SELECT * FROM routes WHERE is_disrupted=0 AND id != current_route
       [AND refrigerated=1 if cold_chain]
       ORDER BY risk_score ASC, estimated_hours ASC
        ↓
Filter: match origin/destination
        ↓
Rank: min(risk_score × 100 + estimated_hours)
        ↓
Return: RouteRecommendation with reason, cost, additional_hours
```

### Cold-Chain Excursion Detection

```
GET /api/cold-chain/{shipment_id}
        ↓
ColdChainService.getAlert(shipment_id)
        ↓
Load: sensor_readings ORDER BY timestamp
Load: cold_chain_config (min_temp, max_temp, margins, allowed_minutes)
        ↓
Calculate: peak_temp, min_temp, max_temp, excursion_duration, excursion_count
        ↓
Classify:
  excursion_magnitude = peak_temp - max_temp (or min_temp - temp)
  if magnitude >= critical_margin → CRITICAL
  if duration >= allowed_minutes × 3 → CRITICAL
  if magnitude >= warning_margin × 2 → HIGH
  if duration >= allowed_minutes → HIGH
  if magnitude >= warning_margin → WARNING
  else → NORMAL
        ↓
Return: ColdChainAlert with severity, reason, recommended_action
```

---

## Database Schema

| Table | Key Columns |
|---|---|
| `disruptions` | id, type, name, location, severity, affected_routes, status |
| `routes` | id, origin, destination, estimated_hours, cost_usd, is_disrupted, risk_score |
| `carriers` | id, name, available_capacity, reliability, avg_delay_hours, cold_chain, is_disrupted |
| `shipments` | id, origin, destination, current_route, carrier, priority, cold_chain, expected_delay_hours |
| `fleet_assets` | id, type, location, status, capacity_kg, utilisation_pct, refrigerated |
| `sensor_readings` | shipment_id, timestamp, temperature, humidity, sensor_status |
| `cold_chain_config` | shipment_id, min_temp, max_temp, warning_margin, critical_margin, allowed_excursion_minutes |

---

## Key Design Decisions

| Decision | Rationale |
|---|---|
| C++ with embedded SQLite | Zero external dependencies, single binary, portable |
| In-memory SQLite with seed data | Fast startup, deterministic demo, easily resetable |
| Custom HTTP server | No Boost/Crow required, works with base MinGW install |
| Additive impact score | Each component is traceable to a specific cause |
| Configurable cold-chain thresholds | Different products have different temperature requirements |
| Keyword-based Bob dispatch | Simple, explainable, no ML required |

---

## Bob AI Integration

```
User types: "What should we do right now?"
        ↓
POST /api/bob/query {"question": "..."}
        ↓
api_handler.cpp handleBobQuery()
  keyword detection: "what should we do" → full summary
        ↓
RecommendationService.generateBobSummary()
  - getActiveDisruptions()
  - getAffectedShipments()
  - getIdleAssets()
  - getAllAlerts()
  - generateAll() → top 5 priority recommendations
        ↓
Returns structured text with:
  - Active disruptions count + names
  - Affected shipments + highest risk shipment details
  - Fleet utilisation + idle assets
  - Cold-chain alerts
  - Top 5 recommended actions
```
