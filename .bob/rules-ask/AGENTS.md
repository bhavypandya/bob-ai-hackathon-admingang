# AGENTS.md

This file provides guidance to agents when working with code in this repository.

## Non-obvious context for answering questions

- **`src/frontend/` is the source; `src/build/bin/frontend/` is what the browser actually loads** — they can be out of sync.
- **Database resets on every server restart** — all data is seeded fresh from `database.cpp`; there is no persistent storage.
- **Approve/reject decisions exist only for the current browser session** — no backend endpoint stores them.
- **`build.bat` must be run from `src\` (it self-cds), not from repo root** — though `start_server.bat` at root handles the server launch correctly.
- **The HTTP server is hand-rolled in `main.cpp`** — there is no Express, Crow, or any HTTP framework; routing is a simple pattern-match loop.
- **All JSON serialisation is in `api/api_handler.cpp`**, not on the models — models are plain structs.
