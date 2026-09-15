# SupplyGuard — Supply Chain Disruption Assistant & Fleet Optimizer

> **IBM Bob AI Hackathon Submission** — AdminGang

---

## 👥 Team

| Field | Value |
|---|---|
| **Team Name** | AdminGang |
| **Track** | AI |
| **Team Lead** | Bhavy Pandya |
| **Members** | 1.Johan Bhalsod 2.Dhyan Chovatiya 3.Naman Dhameliya|

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
- **Route & Carrier Recommendations:** Algorithm-driven recommendations comparing risk, cost, capacity, and availability
- **Fleet Utilisation Optimisation:** Automatic detection of idle assets with data-driven redeployment recommendations
- **Cold-Chain IoT Monitoring:** Configurable temperature range monitoring with excursion severity classification (NORMAL → WARNING → HIGH → CRITICAL)
- **Bob AI Assistant:** Natural language operations Q&A backed by live C++ backend data

---

## 🛠️ Tech Stack

| Category | Technologies |
|---|---|
| **Languages** | C++17, HTML5, CSS3, JavaScript |
| **Backend** | Custom C++ HTTP server (no frameworks required) |
| **IBM Technologies** | IBM Bob AI |
| **Database** | SQLite3 (bundled amalgamation) |
| **Libraries** | nlohmann/json (header-only) |
| **Frontend** | Chart.js, Vanilla JS |
| **Build** | CMake + MinGW/GCC (Windows), GCC (Linux/macOS) |

---

## 📁 Repository Structure

```
├── src/
│   ├── backend/
│   │   ├── main.cpp              ← HTTP server + route registration
│   │   ├── api/                  ← REST API handlers + Bob query handler
│   │   ├── database/             ← SQLite wrapper + seed data
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
│   ├── bob/                      ← Bob integration configuration
│   ├── tests/                    ← Unit tests (67 tests, all passing)
│   ├── third_party/              ← nlohmann/json + SQLite amalgamation
│   ├── CMakeLists.txt
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
cd src
build.bat
build\bin\supply_chain_backend.exe --frontend frontend
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

1. Open `http://localhost:8080` → **Overview** tab shows KPI cards
2. **Disruptions** tab → 5 active/monitoring disruptions including Cyclone Biparjoy
3. **Shipments** tab → 24 shipments, filter to see CRITICAL/HIGH risk
4. Click any shipment → Detail panel with risk reasons + route recommendation
5. **Fleet** tab → Utilisation chart, idle assets, redeployment recommendations
6. **Cold Chain** tab → Temperature cards + interactive Chart.js temperature graph
7. **Simulation** tab → Click "Temperature Excursion" for SH1016 → watch severity change
8. **Bob AI** tab → Ask: *"What should we do right now?"*

| Artifact | Link |
|---|---|
| 📹 Demo Video | [See demo/demo-video-link.txt](demo/demo-video-link.txt) |
| 🌐 Live Demo | [See demo/live-demo-url.txt](demo/live-demo-url.txt) |
| 🖼️ Screenshots | [See demo/screenshots/](demo/screenshots/) |
| 📊 Presentation | [See presentation/](presentation/) |

---

## ⚠️ Known Limitations

- HTTP server is single-threaded (one request at a time) — suitable for demo, not production
- Cold-chain severity is based on configured thresholds — not a regulatory determination
- Route recommendations use heuristic scoring, not graph-based pathfinding
- Demo data is for Indian supply chain geography

---

## 🏅 What We're Most Proud Of

The **complete end-to-end workflow** — from a disruption event through risk scoring,
route/carrier recommendation, fleet redeployment, cold-chain monitoring, to Bob AI
summarising the situation and recommending actions — all implemented in clean C++17
with zero external HTTP framework dependencies and 67 passing unit tests.

---
