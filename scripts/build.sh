#!/bin/bash
# ============================================================
# build.sh — Build the Kafka MCP gRPC Server
# ============================================================

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build"

echo "=============================================="
echo " Kafka MCP gRPC Server — Build Script"
echo " Build type: $BUILD_TYPE"
echo "=============================================="

# --- Check dependencies ---
echo "[1/4] Checking dependencies..."
command -v cmake    >/dev/null 2>&1 || { echo "ERROR: cmake not found"; exit 1; }
command -v protoc   >/dev/null 2>&1 || { echo "ERROR: protoc not found"; exit 1; }
command -v pkg-config >/dev/null 2>&1 || { echo "ERROR: pkg-config not found"; exit 1; }

echo "      cmake   : $(cmake --version | head -1)"
echo "      protoc  : $(protoc --version)"

# --- Create build directory ---
echo "[2/4] Creating build directory..."
mkdir -p $BUILD_DIR

# --- Configure ---
echo "[3/4] Configuring with CMake..."
cmake -B $BUILD_DIR \
      -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DBUILD_TESTS=ON \
      -S .

# --- Build ---
echo "[4/4] Building..."
cmake --build $BUILD_DIR --parallel $(nproc)

echo ""
echo "=============================================="
echo " Build complete!"
echo " Binary: $BUILD_DIR/kafka_mcp_server"
echo " Run:    ./$BUILD_DIR/kafka_mcp_server config/server.json"
echo "=============================================="
