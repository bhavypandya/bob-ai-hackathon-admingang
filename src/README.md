# Supply Chain Disruption Assistant — Source Code

## Quick Build & Run

### Windows (MinGW)

```batch
build.bat
build\bin\supply_chain_backend.exe --frontend frontend
```

### Linux / macOS

```bash
chmod +x build.sh
./build.sh
./build/bin/supply_chain_backend --frontend frontend
```

Open `http://localhost:8080`

## Run Tests

```batch
build\bin\run_tests.exe
```

## Directory Structure

```
src/
├── backend/
│   ├── main.cpp                     ← HTTP server (self-contained, no Crow/Boost)
│   ├── api/api_handler.cpp          ← All REST API + Bob query handler
│   ├── database/database.cpp        ← SQLite wrapper + seed data
│   ├── models/                      ← Data model structs (header-only)
│   └── services/                    ← Business logic
│       ├── disruption_service.cpp
│       ├── shipment_service.cpp     ← Impact scoring (0-100)
│       ├── route_service.cpp        ← Route recommendation engine
│       ├── carrier_service.cpp      ← Carrier recommendation engine
│       ├── fleet_service.cpp        ← Idle detection + redeployment
│       ├── cold_chain_service.cpp   ← Temperature excursion detection
│       ├── recommendation_service.cpp ← Unified recommendations + Bob summary
│       └── simulation_service.cpp  ← What-if scenario engine
├── frontend/
│   ├── index.html                   ← Single-page dashboard
│   ├── css/dashboard.css
│   └── js/dashboard.js
├── bob/
│   ├── bob_config.yaml              ← Bob integration configuration
│   └── README.md
├── tests/                           ← 67 unit tests
├── third_party/
│   ├── nlohmann/json.hpp            ← JSON library (header-only)
│   └── sqlite/sqlite3.c             ← SQLite amalgamation
├── CMakeLists.txt
├── build.bat                        ← Windows build script
└── build.sh                         ← Linux/macOS build script
```

## API Endpoints

| Method | Path | Description |
|---|---|---|
| GET | `/api/health` | Health check |
| GET | `/api/dashboard` | Full KPI dashboard |
| GET | `/api/shipments` | All shipments |
| GET | `/api/shipments/affected` | Affected shipments with risk scores |
| GET | `/api/disruptions` | All disruptions |
| GET | `/api/routes` | All routes |
| POST | `/api/routes/recommend` | Route recommendation |
| GET | `/api/carriers` | All carriers |
| POST | `/api/carriers/recommend` | Carrier recommendation |
| GET | `/api/fleet` | All fleet assets |
| GET | `/api/fleet/idle` | Idle assets |
| GET | `/api/fleet/utilisation` | Fleet metrics |
| POST | `/api/fleet/redeploy` | Update asset status |
| GET | `/api/cold-chain/alerts` | All cold-chain alerts |
| GET | `/api/cold-chain/{id}` | Shipment cold-chain detail |
| GET | `/api/cold-chain/{id}/temperature` | Temperature readings |
| POST | `/api/simulation` | Run simulation scenario |
| GET | `/api/recommendations` | All recommendations |
| POST | `/api/bob/query` | Bob AI natural language query |
