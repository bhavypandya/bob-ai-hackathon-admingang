# SupplyGuard — Supply Chain Disruption Assistant & Fleet Optimizer

> **IBM Bob AI Hackathon Submission** — AdminGang

---

## 👥 Team

| Field | Value |
|---|---|
| **Team Name** | AdminGang |
| **Track** | AI |
| **Team Lead** | Bhavy Pandya |
| **Members** | Johan Bhalsod, Dhyan Chovatiya, Naman Dhameliya |

---

## 🎯 Problem Statement

Supply chain disruptions — weather events, port strikes, road closures, carrier outages —
cascade across hundreds of active shipments simultaneously. Operations managers cannot
manually track which shipments are affected, identify available fleet assets for redeployment,
or monitor cold-chain temperature sensor logs across a large, dynamic supply network in real time.

---

## 💡 Solution

SupplyGuard is a complete C++ backend + web dashboard that automatically identifies disrupted
shipments, scores their risk (0–100) with explainable reasons, recommends alternative routes
and carriers, detects idle fleet assets for redeployment, monitors cold-chain IoT sensor data
for temperature excursions, and presents everything through an integrated Bob AI assistant
that answers natural language operational questions using live backend data.

---

## ✨ Key Features

- **Disruption Impact Analysis:** Every affected shipment receives an explainable risk score (0–100) with specific reasons (route affected, carrier disrupted, priority, cold-chain sensitivity)
- **Disruption Detail Modal:** Click any disruption card to open a fullscreen detail view showing description, affected routes, and all matching shipments
- **Route & Carrier Recommendations:** Algorithm-driven recommendations comparing risk, cost, capacity, and availability — approve or reject directly from the shipment detail panel
- **Fleet Utilisation Optimisation:** Automatic detection of idle assets with data-driven redeployment recommendations
- **Cold-Chain IoT Monitoring:** Configurable temperature range monitoring with excursion severity classification (NORMAL → WARNING → HIGH → CRITICAL)
- **Sortable Tables:** Click any column header in Overview, Shipments, or Fleet tables to sort ascending/descending
- **What-If Simulations:** 6 scenario types with meaningful before/after impact on shipments and fleet
- **74 Shipments:** Realistic sample dataset spanning all major Indian supply chain corridors
- **Bob AI Assistant:** Redesigned chat interface with quick-question sidebar; natural language Q&A backed by live C++ backend data

---

## 🛠️ Tech Stack

| Category | Technologies |
|---|---|
| **Languages** | C++17, HTML5, CSS3, JavaScript |
| **Backend** | Custom C++ HTTP server (no frameworks required) |
| **IBM Technologies** | IBM Bob AI |
| **Database** | SQLite3 (bundled amalgamation, in-memory) |
| **Libraries** | nlohmann/json (header-only) |
| **Frontend** | Chart.js, Vanilla JS |
| **Build** | MinGW/GCC (Windows), GCC (Linux/macOS) |

---

## 📁 Repository Structure

```
├── src/
│   ├── backend/
│   │   ├── main.cpp              ← HTTP server + route registration
│   │   ├── api/                  ← REST API handlers + Bob query handler
│   │   ├── database/             ← SQLite wrapper + seed data (74 shipments)
│   │   ├── models/               ← Data model structs
│   │   └── services/             ← Business logic services
│   │       ├── disruption_service.cpp
│   │       ├── shipment_service.cpp   ← Impact scoring algorithm
│   │       ├── route_service.cpp      ← Route recommendation engine
│   │       ├── carrier_service.cpp    ← Carrier recommendation engine
│   │       ├── fleet_service.cpp      ← Fleet optimisation
│   │       ├── cold_chain_service.cpp ← Temperature excursion detection
│   │       ├── recommendation_service.cpp ← Unified recommendation engine
│   │       └── simulation_service.cpp ← What-if scenario engine
│   ├── frontend/
│   │   ├── index.html            ← Dashboard SPA
│   │   ├── css/dashboard.css
│   │   └── js/dashboard.js
│   ├── tests/                    ← Unit tests (67 tests, all passing)
│   ├── third_party/              ← nlohmann/json + SQLite amalgamation
│   ├── build.bat                 ← Windows build script
│   └── build.sh                  ← Linux/macOS build script
├── docs/                         ← Architecture, setup, problem statement
├── demo/                         ← Screenshots, video link
├── presentation/                 ← Slide deck
└── submission.yaml
```

---

## ⚡ How to Run

### Windows (MinGW/GCC)

```batch
# Build
src\build.bat

# Run (from repo root — launcher handles cd automatically)
start_server.bat
```

Or manually:
```batch
cd src\build\bin
supply_chain_backend.exe --frontend .\frontend
```

### Linux / macOS

```bash
cd src
chmod +x build.sh
./build.sh
./build/bin/supply_chain_backend --frontend frontend
```

Open **http://localhost:8080** in your browser.

### Run Tests

```batch
src\build\bin\run_tests.exe
```

Expected: **67 passed, 0 failed**

---

## 🖥️ Demo Flow

1. Open `http://localhost:8080` → **Overview** tab shows KPI cards + priority recommendations (click any to jump to that shipment)
2. **Disruptions** tab → 5 active/monitoring disruptions — click any card for a fullscreen detail view with affected shipments
3. **Shipments** tab → 74 shipments; filter to Delayed/Disrupted; click column headers to sort; click any row to open the detail panel
4. **Shipment detail panel** → shows risk reasons, ON_TIME banner or route recommendation with Approve/Reject buttons; red Close button top-right
5. **Fleet** tab → utilisation chart, sortable asset table, redeployment recommendations
6. **Cold Chain** tab → temperature cards + interactive Chart.js temperature graph
7. **Simulation** tab → click any scenario to see before/after impact on shipments and fleet
8. **Ask Bob** (top-right button) → redesigned chat UI with sidebar quick questions; ask *"What should we do right now?"*

| Artifact | Link |
|---|---|
| 📹 Demo Video | [See demo/demo-video-link.txt](demo/demo-video-link.txt) |
| 🖼️ Screenshots | [See demo/screenshots/](demo/screenshots/) |
| 📊 Presentation | [See presentation/](presentation/) |

---

## ⚠️ Known Limitations

- HTTP server is single-threaded (one request at a time) — suitable for demo, not production
- Approve/Reject decisions are session-only (browser memory) — page refresh resets them
- Cold-chain severity is based on configured thresholds — not a regulatory determination
- Route recommendations use heuristic scoring, not graph-based pathfinding
- Demo data covers Indian supply chain geography

---

## 🏅 What We're Most Proud Of

The **complete end-to-end workflow** — from a disruption event through risk scoring,
route/carrier recommendation, fleet redeployment, cold-chain monitoring, to Bob AI
summarising the situation and recommending actions — all implemented in clean C++17
with zero external HTTP framework dependencies and 67 passing unit tests.
The dashboard UI delivers a polished, fully interactive experience with sortable tables,
fullscreen disruption modals, an improved Bob AI page, and 74 realistic shipments across
all major corridors.
