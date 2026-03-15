#!/usr/bin/env bash
set -euo pipefail

# Defaults
BOARD="xiao_nrf54l15/nrf54l15/cpuapp"
NODE=""
BUILD_TYPE="pristine"   # or "incremental"
EXTRA_OVERLAYS=""       # semicolon-separated, e.g. "overlays/overlay-54l15.conf;overlays/ot-mtd.conf"
APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
  cat <<EOF
Usage:
  $(basename "$0") --node NDxx [--board <zephyr_board>] [--overlays <semicolon_list>] [--no-pristine]

Examples:
  $(basename "$0") --node ND12
  $(basename "$0") --node ND30 --overlays "overlays/overlay-54l15.conf;overlays/ot-mtd.conf"
  $(basename "$0") --node ND12 --no-pristine

Notes:
  --node NDxx    → gebruikt nodes/NDxx/NDxx.conf als Kconfig overlay
  --no-pristine  → skip 'west build -p always' (sneller itereren)
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --node)      NODE="$2"; shift 2;;
    --board)     BOARD="$2"; shift 2;;
    --overlays)  EXTRA_OVERLAYS="$2"; shift 2;;
    --no-pristine) BUILD_TYPE="incremental"; shift;;
    -h|--help)   usage; exit 0;;
    *) echo "Unknown arg: $1"; usage; exit 1;;
  esac
done

if [[ -z "${NODE}" ]]; then
  echo "ERROR: --node NDxx is verplicht"; usage; exit 1
fi

NODE_DIR="${APP_DIR}/nodes/${NODE}"
NODE_CONF="${NODE_DIR}/${NODE}.conf"
if [[ ! -f "${NODE_CONF}" ]]; then
  echo "ERROR: Node overlay ontbreekt: ${NODE_CONF}"
  exit 1
fi

# Compose OVERLAY_CONFIG: extra overlays (optioneel) + node overlay als laatste (wint)
OVERLAY_CONFIG="${EXTRA_OVERLAYS}"
if [[ -n "${OVERLAY_CONFIG}" ]]; then
  OVERLAY_CONFIG="${OVERLAY_CONFIG};${NODE_CONF}"
else
  OVERLAY_CONFIG="${NODE_CONF}"
fi

# Build dir per node
BUILD_DIR="${APP_DIR}/build/${NODE}"
mkdir -p "${BUILD_DIR}"

echo "-----------------------------------"
echo "Building for node  : ${NODE}"
echo "Zephyr BOARD       : ${BOARD}"
echo "Kconfig overlays   : ${OVERLAY_CONFIG}"
echo "Build dir          : ${BUILD_DIR}"
echo "-----------------------------------"

pushd "${APP_DIR}" >/dev/null

if [[ "${BUILD_TYPE}" == "pristine" ]]; then
  west build -p always -b "${BOARD}" -d "${BUILD_DIR}" -- -DOVERLAY_CONFIG="${OVERLAY_CONFIG}"
else
  west build             -b "${BOARD}" -d "${BUILD_DIR}" -- -DOVERLAY_CONFIG="${OVERLAY_CONFIG}"
fi

# Flashen (optioneel):
# west flash -d "${BUILD_DIR}"

popd >/dev/null
