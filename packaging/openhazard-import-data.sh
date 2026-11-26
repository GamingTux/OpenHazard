#!/usr/bin/env bash
set -euo pipefail

# Import retail Star Trek: Voyager - Elite Force game assets for OpenHazard.
# Supports either:
#   1) GOG Windows installer (.exe) via `innoextract`
#   2) An existing Elite Force installation directory or BaseEF/ directory

usage() {
  cat <<'EOF'
Usage: openhazard-import-data <path-to-gog-setup.exe | path-to-install-dir>

Imports pak0.pk3..pak3.pk3 (and efgamex86.dll) read-only into:
  ${XDG_DATA_HOME:-$HOME/.local/share}/openhazard/baseEF/

Original files are never modified; saves and configs stay in the user data directory.
EOF
}

if [[ $# -lt 1 || "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi

SRC="$1"
if [[ "$(uname -s)" == "Darwin" ]]; then
  DEFAULT_ROOT="${XDG_DATA_HOME:+${XDG_DATA_HOME}/openhazard}"
  DEFAULT_ROOT="${DEFAULT_ROOT:-${HOME}/Library/Application Support/OpenHazard}"
  DEFAULT_CACHE="${HOME}/Library/Application Support/stvef-retail"
else
  DEFAULT_ROOT="${XDG_DATA_HOME:-$HOME/.local/share}/openhazard"
  DEFAULT_CACHE="${XDG_DATA_HOME:-$HOME/.local/share}/stvef-retail"
fi
DATA_ROOT="${OPENHAZARD_DATA_ROOT:-${DEFAULT_ROOT}}"
TARGET_BASEEF="${DATA_ROOT}/baseEF"
RETAIL_CACHE="${DEFAULT_CACHE}"

mkdir -p "${TARGET_BASEEF}"

SRC_LOWER="$(printf '%s' "${SRC}" | tr '[:upper:]' '[:lower:]')"
SOURCE_ROOT=""
if [[ -f "${SRC}" && "${SRC_LOWER}" == *.exe ]]; then
  if ! command -v innoextract >/dev/null 2>&1; then
    echo "Error: 'innoextract' is required to extract GOG installers (apt/brew install innoextract)." >&2
    exit 1
  fi
  echo "Extracting retail assets from GOG installer to ${RETAIL_CACHE} ..."
  mkdir -p "${RETAIL_CACHE}"
  innoextract -d "${RETAIL_CACHE}" --include "BaseEF" --include "efgamex86.dll" "${SRC}"
  SOURCE_ROOT="${RETAIL_CACHE}"
elif [[ -d "${SRC}" ]]; then
  SOURCE_ROOT="$(cd "${SRC}" && pwd)"
else
  echo "Error: '${SRC}' is neither a GOG .exe installer nor a directory." >&2
  exit 1
fi

find_ci_file() {
  local dir="$1"
  local name="$2"
  find "${dir}" -maxdepth 2 -iname "${name}" -type f 2>/dev/null | head -n 1 || true
}

REQUIRED=(pak0.pk3 pak1.pk3 pak2.pk3 pak3.pk3 efgamex86.dll)
MISSING=()

for f in "${REQUIRED[@]}"; do
  found="$(find_ci_file "${SOURCE_ROOT}" "${f}")"
  if [[ -z "${found}" ]]; then
    parent="$(dirname "${SOURCE_ROOT}")"
    found="$(find_ci_file "${parent}" "${f}")"
  fi
  if [[ -n "${found}" ]]; then
    ln -sf "${found}" "${TARGET_BASEEF}/${f}"
    echo "  [OK] ${f} -> ${found}"
  else
    MISSING+=("${f}")
    echo "  [MISSING] ${f}" >&2
  fi
done

opt="$(find_ci_file "${SOURCE_ROOT}" "playermaps.pk3")"
if [[ -n "${opt}" ]]; then
  ln -sf "${opt}" "${TARGET_BASEEF}/playermaps.pk3"
  echo "  [OK] playermaps.pk3 -> ${opt}"
fi

if [[ ${#MISSING[@]} -gt 0 ]]; then
  echo "Error: missing required retail file(s): ${MISSING[*]}" >&2
  exit 1
fi

echo "Done! Retail assets linked into ${TARGET_BASEEF}"
