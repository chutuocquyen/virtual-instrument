#!/bin/sh
set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"

em++ -std=c++20 \
    "$ROOT/web/virtual_instrument.cpp" \
    -I "$ROOT/src" \
    -O2 \
    -Wconversion \
    -sAUDIO_WORKLET=1 \
    -sWASM_WORKERS=1 \
    -sALLOW_MEMORY_GROWTH=0 \
    -sEXIT_RUNTIME=0 \
    -o "$ROOT/web/virtual_instrument.js"
