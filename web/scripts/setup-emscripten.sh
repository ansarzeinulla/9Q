#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EMSDK_DIR="${EMSDK_DIR:-$ROOT_DIR/.emsdk}"

if ! command -v git >/dev/null 2>&1; then
  echo "git was not found. Install Xcode Command Line Tools first:"
  echo "  xcode-select --install"
  exit 127
fi

if [[ ! -d "$EMSDK_DIR/.git" ]]; then
  git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

cd "$EMSDK_DIR"
git pull --ff-only
./emsdk install latest
./emsdk activate latest

cat <<EOF

Emscripten is installed in:
  $EMSDK_DIR

For this terminal, activate it with:
  source "$EMSDK_DIR/emsdk_env.sh"

This repo's npm run build:wasm also auto-loads that local SDK.
EOF
