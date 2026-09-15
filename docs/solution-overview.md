# Solution Overview

## SupplyGuard — Supply Chain Disruption Assistant & Fleet Optimizer

### Core Idea

SupplyGuard monitors the supply chain state and, when disruptions occur, automatically
analyses their downstream impact, generates recommendations, and gives operations teams
a single place to understand the situation and take action — with Bob AI as the intelligent
interface.

---

## The Complete Workflow

```
ACTIVE DISRUPTION (weather / port strike / road closure / carrier)
        ↓
IDENTIFY AFFECTED SHIPMENTS
  → Query all shipments on disrupted routes and with disrupted carriers
        ↓
ANALYSE IMPACT / RISK (0-100 score per shipment)
  → Route affected (+30), carrier disrupted (+20), delay severity (+6/+12/+20),
    priority modifier (+10/+15), cold-chain sensitivity (+10)
        ↓
RECOMMEND ALTERNATIVE ROUTE
  → Find undisrupted routes for same corridor
  → Rank by risk score + estimated time + cost
  → Return best with explanation
        ↓
RECOMMEND ALTERNATIVE CARRIER
  → Filter by: not disrupted, sufficient capacity, cold-chain capability
  → Rank by reliability × 50 – delay × 5 – cost × 0.5
  → Return best with explanation
        ↓
IDENTIFY IDLE FLEET
  → Query assets with status IDLE or AVAILABLE
  → Match to demand hotspots (locations with highest disrupted shipment count)
  → Filter by compatibility (refrigeration, type, capacity)
        ↓
RECOMMEND FLEET REDEPLOYMENT
  → One recommendation per idle asset to nearest demand hotspot
        ↓
MONITOR COLD-CHAIN DATA (IoT sensor readings)
  → Per-shipment configurable temperature range
  → Read all sensor readings sorted by timestamp
        ↓
DETECT TEMPERATURE EXCURSIONS
  → Mark readings outside [min_temp, max_temp] as excursion
  → Calculate excursion duration in minutes
  → Count separate excursion events
        ↓
CLASSIFY SEVERITY (NORMAL / WARNING / HIGH / CRITICAL)
  → Based on: excursion magnitude vs. warning/critical margins,
    excursion duration vs. allowed limit, excursion count, time to delivery
        ↓
GENERATE ACTIONABLE RECOMMENDATIONS
  → REROUTE_SHIPMENT, CHANGE_CARRIER, REDEPLOY_FLEET,
    PRIORITISE_SHIPMENT, COLD_CHAIN_REVIEW, MONITOR_DISRUPTION
  → Sorted by priority (CRITICAL > HIGH > MEDIUM > LOW)
        ↓
BOB EXPLAINS THE SITUATION AND RECOMMENDED ACTION
  → Natural language Q&A backed by live backend data
  → Full operational summary with top 5 priority actions
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

Computes an explainable 0–100 risk score for each affected shipment. The score has additive
components, each traceable to a specific cause.

### 5. Route Recommendation Engine

Finds undisrupted routes for the same corridor, ranks by (risk_score × 100 + estimated_hours),
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

A single-page application with Chart.js visualisations, tabbed navigation covering all major
sections: overview, disruptions, shipments, fleet, cold chain, recommendations, simulation,
and Bob AI.

---

## Demo Scenario

1. **Normal operation** — Dashboard shows 24 shipments, 5 disruptions, 8 idle assets
2. **Disruption active** — Cyclone Biparjoy affects R01, R02, R08; 12 shipments impacted
3. **Risk scored** — SH1016 (vaccines, CRITICAL priority, cold-chain) scores CRITICAL
4. **Route recommended** — Switch SH1001 from R01 to R11 (Mumbai–Delhi Rail Freight)
5. **Fleet idle** — T204, T211, T215 idle at Mumbai/Delhi, recommended for redeployment
6. **Temperature excursion** — SH1016 shows peak 15.1°C vs. configured max 8°C → CRITICAL severity
7. **Bob summary** — Ask "What should we do right now?" → receives complete operational action plan
