#!/usr/bin/env bash
set -euo pipefail

# Portable launcher for OpenHazard Singleplayer (Linux / macOS)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

find_binary() {
  local candidates=(
    "${SCRIPT_DIR}/openhazard-sp"
    "${SCRIPT_DIR}/../build/bin/openhazard-sp"
    "/usr/local/lib/openhazard/openhazard-sp"
    "/usr/lib/openhazard/openhazard-sp"
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
  echo "Error: could not locate openhazard-sp binary." >&2
  exit 1
}

ARGS=()
for arg in "$@"; do
  case "${arg}" in
    --wayland) export SDL_VIDEODRIVER=wayland ;;
    --x11)     export SDL_VIDEODRIVER=x11 ;;
    *)         ARGS+=("${arg}") ;;
  esac
done

exec "${BIN}" "${ARGS[@]}"
