#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/lbl_server"
CONF="${ROOT_DIR}/config/app.ini"

if [[ ! -x "$BIN" ]]; then
  echo "Binary not found: $BIN"
  echo "Run ./build.sh first"
  exit 1
fi

exec "$BIN" "$CONF"
