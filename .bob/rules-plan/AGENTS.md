# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Non-obvious architectural constraints

- **`:memory:` SQLite only** — no file DB, no migrations, no persistence across restarts. Schema + seed run in `Database` constructor every time.
- **Frontend state is authoritative for decisions** — `state.decisions` and patched `state.allShipments` are the only record of approved/rejected recommendations; no backend endpoint exists for this.
- **Static file serving is hardcoded to 3 routes** (`/`, `/css/dashboard.css`, `/js/dashboard.js`) in `main.cpp` — adding new frontend files requires adding new routes in the backend.
- **HTTP server is single-threaded sequential** — `handleConnection()` is called inline in the `accept` loop; long requests block all other clients.
- **Services are stateless** — each `Service` constructor takes `db::Database&`; they hold no member state beyond the DB reference. Do not add caching to services.
- **No build system other than `build.bat`** — `CMakeLists.txt` exists but is not used; do not rely on it.
- **`third_party/sqlite/`** is the only vendored dependency; `nlohmann/json` is included via `third_party/` headers. No package manager.
- **Test binaries share all service `.o` files with the main binary** — adding a new service requires adding it to both link steps in `build.bat`.
