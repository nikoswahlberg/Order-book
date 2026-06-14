#!/usr/bin/env bash
# build_wasm.sh — compile the C++ matching engine to WebAssembly
#
# Prerequisites (one-time setup on Mac):
#   brew install emscripten
#
# Usage:
#   cd cpp-order-book
#   ./web/build_wasm.sh
#
# Output: web/order_book.js + web/order_book.wasm
# Then open web/index.html via a local server:
#   cd web && python3 -m http.server 8080
#   open http://localhost:8080

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR"

echo "==> Building order book WASM module..."
echo "    Project root : $PROJECT_ROOT"
echo "    Output dir   : $OUT_DIR"

emcc -std=c++20 -O2 \
    -I"$PROJECT_ROOT/include" \
    -I"$PROJECT_ROOT/src" \
    "$SCRIPT_DIR/bindings.cpp" \
    "$PROJECT_ROOT/src/order_book.cpp" \
    "$PROJECT_ROOT/src/data_gen.cpp" \
    -lembind \
    -s MODULARIZE=1 \
    -s EXPORT_NAME=OrderBookModule \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ENVIRONMENT=web \
    -o "$OUT_DIR/order_book.js"

echo ""
echo "==> Done. Files written:"
echo "    $OUT_DIR/order_book.js"
echo "    $OUT_DIR/order_book.wasm"
echo ""
echo "==> To serve locally:"
echo "    cd web && python3 -m http.server 8080"
echo "    open http://localhost:8080"
