#!/usr/bin/env bash
set -euo pipefail

# Portable launcher for OpenHazard Dedicated Server (Linux / macOS)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

find_binary() {
  local candidates=(
    "${SCRIPT_DIR}/openhazard-ded"
    "${SCRIPT_DIR}/../build/bin/openhazard-ded"
    "/usr/local/lib/openhazard/openhazard-ded"
    "/usr/lib/openhazard/openhazard-ded"
  )
  for bin in "${candidates[@]}"; do
    if [[ -x "${bin}" ]]; then
      printf '%s\n' "${bin}"
      return 0
    fi
  done
  return 1
}

BIN="$(find_binary)" || {
  echo "Error: could not locate openhazard-ded binary." >&2
  exit 1
}

exec "${BIN}" --no-import "$@"
