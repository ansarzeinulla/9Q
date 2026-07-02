#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v emcc >/dev/null 2>&1; then
  if [[ -n "${EMSDK_DIR:-}" && -f "$EMSDK_DIR/emsdk_env.sh" ]]; then
    source "$EMSDK_DIR/emsdk_env.sh" >/dev/null
  elif [[ -f "$ROOT_DIR/.emsdk/emsdk_env.sh" ]]; then
    source "$ROOT_DIR/.emsdk/emsdk_env.sh" >/dev/null
  fi
fi

if ! command -v emcc >/dev/null 2>&1; then
  echo "emcc was not found."
  echo ""
  echo "Run this once from the repo root:"
  echo "  npm run setup:emscripten"
  echo ""
  echo "Then build again:"
  echo "  npm run build:wasm"
  echo ""
  echo "If you already installed emsdk elsewhere, either source emsdk_env.sh first"
  echo "or set EMSDK_DIR=/path/to/emsdk before running this script."
  exit 127
fi

mkdir -p web/public/wasm

emcc \
  src/togyzkumalak_rules.cpp \
  src/position_hash.cpp \
  src/evaluation.cpp \
  src/minimax_engine.cpp \
  src/dag_search.cpp \
  web/wasm/togyz_wasm.cpp \
  -Iinclude \
  -std=c++17 \
  -O3 \
  -DNDEBUG \
  -DMINIMAX_TT_BITS=20 \
  -sMODULARIZE=1 \
  -sEXPORT_ES6=1 \
  -sENVIRONMENT=web,worker \
  -sALLOW_MEMORY_GROWTH=1 \
  -sINITIAL_MEMORY=134217728 \
  -sMAXIMUM_MEMORY=1073741824 \
  -sSTACK_SIZE=1048576 \
  -sMALLOC=emmalloc \
  -sFILESYSTEM=0 \
  -sNO_EXIT_RUNTIME=1 \
  -sEXPORTED_FUNCTIONS='["_tg_version","_tg_init_engine","_tg_reset","_tg_state_json","_tg_fen_string","_tg_last_error","_tg_last_bot_json","_tg_last_move_json","_tg_get_last_search_stats","_tg_set_fen","_tg_make_move","_tg_bot_move"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","UTF8ToString"]' \
  -o web/public/wasm/togyz_engine.js

echo "Built web/public/wasm/togyz_engine.js and web/public/wasm/togyz_engine.wasm"
