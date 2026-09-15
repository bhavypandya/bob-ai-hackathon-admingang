# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Project
C++17 backend (MinGW/g++) + plain HTML/CSS/JS frontend. No npm, no CMake used in practice — only `src/build.bat`.

## Build & Run

```bat
# Build everything (must be run from repo root or any directory — script self-cds)
src\build.bat

# Run server (MUST cd first — frontend_dir defaults to ./frontend relative to CWD)
cd src\build\bin
supply_chain_backend.exe --frontend .\frontend

# Or use the root launcher (handles cd automatically)
start_server.bat

# Run all tests
src\build\bin\run_tests.exe
```

**Critical gotcha:** `build.bat` prints "=== Build Complete ===" even when linking fails (e.g. due to a running `.exe` locking the file). Check stderr for `Permission denied` to confirm a real failure.

**Critical gotcha:** The `.exe` resolves `--frontend` relative to its **working directory at launch**, not its own location. Always `cd src\build\bin` before running, or pass an absolute path.

## Architecture

```
src/backend/           ← C++17, never edit unless explicitly asked
  main.cpp             ← raw TCP HTTP server (no framework), registers all routes
  api/api_handler.cpp  ← all REST endpoints, uses nlohmann/json
  database/database.cpp← SQLite :memory: only; schema + seed run on every startup (no persistence)
  services/            ← business logic (DisruptionService, ShipmentService, etc.)
  models/              ← plain structs + enum↔string helpers in header only
  third_party/sqlite/  ← bundled sqlite3.c (compiled with gcc, not g++)

src/frontend/          ← SOURCE — edit these
src/build/bin/frontend/← DEPLOYED copy — backend serves from here, not src/frontend/
```

**Deploy after every frontend edit:**
```powershell
Copy-Item "src\frontend\index.html"        "src\build\bin\frontend\index.html" -Force
Copy-Item "src\frontend\css\dashboard.css" "src\build\bin\frontend\css\dashboard.css" -Force
Copy-Item "src\frontend\js\dashboard.js"   "src\build\bin\frontend\js\dashboard.js" -Force
```

## Backend Patterns

- **Database is always `:memory:`** — all data resets on restart. No migration system.
- **`initDB()` must be called before `getDB()`** — `getDB()` throws if called first.
- **sqlite3.c compiled with `gcc` (not `g++`)** — do not change this in build.bat.
- All services take `db::Database&` by reference in constructor; no DI container.
- Enum↔string conversion lives in each model's `.h` file as `static` methods (e.g. `Shipment::statusToString()`).
- All JSON serialisation is in `api_handler.cpp` via `nlohmann/json`; models have no `toJson()`.
- HTTP router does exact pattern matching with `{param}` segments; no regex, no middleware.
- Tests use a custom `check(bool, string)` TAP-style helper defined in `tests/test_main.cpp` — no gtest/catch2.
- To run a single test suite, isolate it: each `test_*.cpp` defines one function (e.g. `testDisruption()`); comment out others in `test_main.cpp` and rebuild.
- Tests must compile with `-DTESTING_MODE` flag (already in build.bat).

## Frontend Patterns

- **All approve/reject decisions live only in `state.decisions`** (JS session memory) — backend never knows about them; page refresh loses all decisions.
- `state.allShipments` is patched locally on route/carrier approval — backend still returns original data after refresh.
- `escapeAttr()` (not `escapeHtml()`) must be used for values injected into inline `onclick="..."` attributes.
- The detail panel uses a flex-column layout: `.detail-header` is `flex-shrink:0` (always visible), `#detailContent` scrolls independently.
- `*.exe` is gitignored — binary never in repo, must rebuild on each new machine.
