# Solution Overview

## SupplyGuard â€” Supply Chain Disruption Assistant & Fleet Optimizer

### Core Idea

SupplyGuard monitors the supply chain state and, when disruptions occur, automatically
analyses their downstream impact, generates recommendations, and gives operations teams
a single place to understand the situation and take action â€” with Bob AI as the intelligent
interface.

---

## The Complete Workflow

```
ACTIVE DISRUPTION (weather / port strike / road closure / carrier)
        â†“
IDENTIFY AFFECTED SHIPMENTS
  â†’ Query all shipments on disrupted routes and with disrupted carriers
        â†“
ANALYSE IMPACT / RISK (0-100 score per shipment)
  â†’ Route affected (+30), carrier disrupted (+20), delay severity (+6/+12/+20),
    priority modifier (+10/+15), cold-chain sensitivity (+10)
        â†“
RECOMMEND ALTERNATIVE ROUTE
  â†’ Find undisrupted routes for same corridor
  â†’ Rank by risk score + estimated time + cost
  â†’ Return best with explanation
        â†“
RECOMMEND ALTERNATIVE CARRIER
  â†’ Filter by: not disrupted, sufficient capacity, cold-chain capability
  â†’ Rank by reliability Ã— 50 â€“ delay Ã— 5 â€“ cost Ã— 0.5
  â†’ Return best with explanation
        â†“
IDENTIFY IDLE FLEET
  â†’ Query assets with status IDLE or AVAILABLE
  â†’ Match to demand hotspots (locations with highest disrupted shipment count)
  â†’ Filter by compatibility (refrigeration, type, capacity)
        â†“
RECOMMEND FLEET REDEPLOYMENT
  â†’ One recommendation per idle asset to nearest demand hotspot
        â†“
MONITOR COLD-CHAIN DATA (IoT sensor readings)
  â†’ Per-shipment configurable temperature range
  â†’ Read all sensor readings sorted by timestamp
        â†“
DETECT TEMPERATURE EXCURSIONS
  â†’ Mark readings outside [min_temp, max_temp] as excursion
  â†’ Calculate excursion duration in minutes
  â†’ Count separate excursion events
        â†“
CLASSIFY SEVERITY (NORMAL / WARNING / HIGH / CRITICAL)
  â†’ Based on: excursion magnitude vs. warning/critical margins,
    excursion duration vs. allowed limit, excursion count, time to delivery
        â†“
GENERATE ACTIONABLE RECOMMENDATIONS
  â†’ REROUTE_SHIPMENT, CHANGE_CARRIER, REDEPLOY_FLEET,
    PRIORITISE_SHIPMENT, COLD_CHAIN_REVIEW, MONITOR_DISRUPTION
  â†’ Sorted by priority (CRITICAL > HIGH > MEDIUM > LOW)
        â†“
BOB EXPLAINS THE SITUATION AND RECOMMENDED ACTION
  â†’ Natural language Q&A backed by live backend data
  â†’ Full operational summary with top 5 priority actions
```

---

## Components

### 1. C++ Backend Server

A self-contained HTTP server written in C++17 with no external framework dependencies.
Handles GET and POST requests, parses JSON using nlohmann/json, and serves the frontend.

### 2. SQLite Database

Stores all supply chain state: disruptions, routes, carriers, shipments, fleet assets,
sensor readings, and cold-chain configuration. Uses the bundled SQLite amalgamation for
zero-dependency portability.

### 3. Disruption Service

Loads active disruptions, determines which routes they affect, and identifies all shipments
on those routes.

### 4. Shipment Impact Analyser

Computes an explainable 0â€“100 risk score for each affected shipment. The score has additive
components, each traceable to a specific cause.

### 5. Route Recommendation Engine

Finds undisrupted routes for the same corridor, ranks by (risk_score Ã— 100 + estimated_hours),
and returns the best available option with a natural-language explanation.

### 6. Carrier Recommendation Engine

Filters available carriers by disruption status, capacity, and cold-chain capability, then
ranks by a weighted reliability/delay/cost score.

### 7. Fleet Optimisation Service

Detects idle and available assets, identifies demand hotspots from disrupted shipment counts,
and generates feasible redeployment recommendations.

### 8. Cold-Chain Monitoring Engine

Reads per-shipment IoT sensor logs, calculates excursion statistics, and classifies severity
using configurable thresholds per product type.

### 9. Recommendation Engine

Aggregates outputs from all services into a unified prioritised recommendation list with
actionable text for each item.

### 10. Simulation Engine

Supports what-if scenarios (weather, road closure, port strike, carrier recall, demand surge,
temperature excursion injection) that update database state and allow before/after comparison.

### 11. Bob AI Assistant

The `/api/bob/query` endpoint accepts natural language questions, dispatches to the appropriate
services based on keyword matching, and returns structured operational answers using live data.

### 12. Frontend Dashboard

A single-page application with Chart.js visualisations, tabbed navigation with tabbed navigation and a polished UI covering: Overview, Disruptions (fullscreen modal), Shipments (74 ships, sortable, approve/reject), Fleet (sortable), Cold Chain, Simulation, and Bob AI (redesigned two-column layout).

---

## Demo Scenario

1. **Normal operation** â€” Dashboard Shows 74 shipments
2. **Disruption active** â€” Cyclone Biparjoy affects R01, R02, R08; click card to see all affected shipments in fullscreen modal
3. **Risk scored** â€” SH1016 (vaccines, CRITICAL priority, cold-chain) scores CRITICAL
4. **Route recommended** â€” Switch SH1001 from R01 to R11 (Mumbaiâ€“Delhi Rail Freight)
5. **Fleet idle** â€” T204, T211, T215 idle at Mumbai/Delhi, recommended for redeployment
6. **Temperature excursion** â€” SH1016 shows peak 15.1Â°C vs. configured max 8Â°C â†’ CRITICAL severity
7. **Simulation** — Run "Port Strike" to see R09/R14 shipments gain +72h delay, Chennai fleet idled
8. **Bob summary** â€” Ask "What should we do right now?" â†’ receives complete operational action plan

