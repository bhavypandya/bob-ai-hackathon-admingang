#!/bin/bash
# Build script for Supply Chain Disruption Assistant (Linux/macOS)
set -e

echo "=== Supply Chain Assistant — Build Script ==="
echo ""

# Determine number of CPU cores
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Create build directory
mkdir -p build
cd build

# Configure
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo "Building with $CORES cores..."
make -j${CORES}

echo ""
echo "=== Build Complete ==="
echo "Run: ./bin/supply_chain_backend --frontend ../frontend"
echo "Open: http://localhost:8080"
