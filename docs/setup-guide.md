# Setup Guide

## Prerequisites

| Tool | Version | Purpose |
|---|---|---|
| C++ Compiler | GCC 6.3+ / MSVC 2019+ / Clang 7+ | Backend compilation |
| MinGW (Windows) | Any recent | g++, gcc on Windows |
| CMake (optional) | 3.16+ | Alternative build system |
| Web browser | Chrome/Firefox/Edge | Frontend dashboard |

**No Python, Node.js, or external package managers required.**

All dependencies are bundled in the repository:
- `src/third_party/sqlite/` â€” SQLite 3.46 amalgamation
- `src/third_party/nlohmann/` â€” nlohmann/json v3.11.3 (header-only)

---

## Quick Start (Windows â€” MinGW)

### Step 1: Clone the repository

```batch
git clone <repository-url>
cd bob-ai-hackathon-admingang
```

### Step 2: Build

```batch
src\build.bat
```

The script will:
- Compile the bundled SQLite3
- Compile all 8 C++ services
- Compile the API handler and main server
- Link the `supply_chain_backend.exe` binary
- Copy the frontend to `src\build\bin\frontend\`

### Step 3: Run the backend

Use the root launcher (handles cd automatically):
``batch
start_server.bat
``r

Or manually:
``batch
cd src\build\bin
supply_chain_backend.exe --frontend .\frontend
``

### Step 4: Open the dashboard

Open your browser and navigate to:
```
http://localhost:8080
```

You should see the SupplyGuard dashboard with the Overview tab active.

---

## Quick Start (Linux / macOS)

### Step 1: Install prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libsqlite3-dev

# macOS
xcode-select --install
brew install cmake
```

### Step 2: Build

```bash
cd src
chmod +x build.sh
./build.sh
```

### Step 3: Run

```bash
./build/bin/supply_chain_backend --frontend frontend
```

### Step 4: Open the dashboard

```
http://localhost:8080
```

---

## Build Options

| Option | Default | Description |
|---|---|---|
| `--port <n>` | 8080 | HTTP port to listen on |
| `--db <path>` | `:memory:` | SQLite database path (`:memory:` = in-memory) |
| `--frontend <dir>` | `./frontend` | Path to frontend HTML/CSS/JS files |

Example with custom port:
```batch
supply_chain_backend.exe --port 9090 --frontend src\frontend
```

---

## Running Tests

```batch
src\build\bin\run_tests.exe
```

Expected output:
```
=== Supply Chain Assistant â€” Unit Tests ===

â”€â”€ Disruption Tests â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
  âœ“  Active disruptions loaded from seed data
  ...

Results: 67 passed, 0 failed
```

---

## Build with CMake (Alternative)

If CMake is available:

```bash
mkdir -p src/build_cmake
cmake -S src -B src/build_cmake -DCMAKE_BUILD_TYPE=Release
cmake --build src/build_cmake --parallel
```

Then run:
```bash
./src/build_cmake/bin/supply_chain_backend --frontend src/frontend
```

---

## Environment Variables (Optional)

Copy `.env.example` to `.env`:

```batch
copy src\.env.example src\.env
```

| Variable | Default | Description |
|---|---|---|
| `APP_PORT` | 8080 | HTTP server port |
| `DB_PATH` | `:memory:` | SQLite database path |

**Note:** The application works with defaults and does not require `.env` to be configured
for the demo.

---

## Verifying the Installation

After starting the backend, verify:

```
GET http://localhost:8080/api/health
```

Expected response:
```json
{
  "status": "ok",
  "service": "Supply Chain Disruption Assistant",
  "version": "2.1.0"
}
```

```
GET http://localhost:8080/api/dashboard
```

Should return the full KPI dashboard data including disruptions, affected shipments,
fleet stats, and cold-chain alerts.

---

## Troubleshooting

### "Port already in use"

Change the port: `supply_chain_backend.exe --port 9090`
And update `src/frontend/js/dashboard.js` line 1: `const API = 'http://localhost:9090';`

### "g++ not found" on Windows

Install MinGW from https://www.mingw-w64.org/ or via MSYS2:
```
pacman -S mingw-w64-x86_64-gcc
```

### Build fails with nlohmann errors

Ensure `src/third_party/nlohmann/json.hpp` exists. If not, download it:
```
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp" -OutFile "src\third_party\nlohmann\json.hpp"
```

### CORS errors in browser

The backend sets `Access-Control-Allow-Origin: *` on all responses.
If you see CORS errors, ensure you're accessing the dashboard through the backend
at `http://localhost:8080` rather than opening `index.html` directly as a file.

### Dashboard shows "Backend Not Running"

The frontend cannot connect to the backend. Check:
1. The backend executable is running
2. It is listening on port 8080 (check console output)
3. No firewall is blocking localhost connections

