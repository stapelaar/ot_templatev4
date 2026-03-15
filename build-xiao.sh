#!/usr/bin/env bash
set -euo pipefail

# Defaults
BOARD="xiao_nrf54l15/nrf54l15/cpuapp"
NODE=""
BUILD_TYPE="pristine"   # or "incremental"
EXTRA_OVERLAYS=""       # semicolon-separated, e.g. "overlays/ot-extra.conf"
APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Always-on overlays (v3-regel)
ALWAYS_OVERLAY_54L15="${APP_DIR}/overlays/overlay-54l15.conf"
ALWAYS_OVERLAY_OT_BASIS="${APP_DIR}/overlays/overlay-OT-network-basis.conf"

# Role overlays (worden op basis van node-config gekozen)
ROLE_OVERLAY_FTD="${APP_DIR}/overlays/overlay-OT-network-ftd.conf"
ROLE_OVERLAY_MTD="${APP_DIR}/overlays/overlay-OT-network-mtd.conf"

usage() {
  cat <<EOF
Usage:
  $(basename "$0") --node NDxx [--board <zephyr_board>] [--overlays <semicolon_list>] [--no-pristine]

Examples:
  $(basename "$0") --node ND12
  $(basename "$0") --node ND30 --overlays "overlays/ot-extra.conf"
  $(basename "$0") --node ND12 --no-pristine

Notes:
  * Altijd actief: overlays/overlay-54l15.conf + overlays/overlay-OT-network-basis.conf
  * Rol-overlay wordt gekozen o.b.v. node-config:
      - CONFIG_OPENTHREAD_MTD=y -> overlay-OT-network-mtd.conf
      - CONFIG_OPENTHREAD_FTD=y -> overlay-OT-network-ftd.conf
  * Node overlay (nodes/NDxx/NDxx.conf) gaat ALTJD als laatste (wint)
EOF
}

# Parse args
while [[ $# -gt 0 ]]; do
  case "$1" in
    --node)        NODE="$2"; shift 2;;
    --board)       BOARD="$2"; shift 2;;
    --overlays)    EXTRA_OVERLAYS="$2"; shift 2;;
    --no-pristine) BUILD_TYPE="incremental"; shift;;
    -h|--help)     usage; exit 0;;
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

# Verplichte overlays moeten bestaan
[[ -f "${ALWAYS_OVERLAY_54L15}" ]] || { echo "ERROR: Missing ${ALWAYS_OVERLAY_54L15}"; exit 1; }
[[ -f "${ALWAYS_OVERLAY_OT_BASIS}" ]] || { echo "ERROR: Missing ${ALWAYS_OVERLAY_OT_BASIS}"; exit 1; }

# ---- Rol bepalen (MTD vs FTD) ----
role="auto"
want_mtd=0
want_ftd=0

# a) expliciet via Kconfig flags (aanbevolen)
grep -Eq '^\s*CONFIG_OPENTHREAD_MTD\s*=\s*y\s*$' "${NODE_CONF}" && want_mtd=1
grep -Eq '^\s*CONFIG_OPENTHREAD_FTD\s*=\s*y\s*$' "${NODE_CONF}" && want_ftd=1

# b) (optioneel) fallback op 'mtd'/'ftd' substring in node-conf als geen flags aanwezig
if [[ $want_mtd -eq 0 && $want_ftd -eq 0 ]]; then
  if grep -qi '\bmtd\b' "${NODE_CONF}"; then want_mtd=1; fi
  if grep -qi '\bftd\b' "${NODE_CONF}"; then want_ftd=1; fi
fi

# c) sanity
if [[ $want_mtd -eq 1 && $want_ftd -eq 1 ]]; then
  echo "ERROR: Zowel MTD als FTD gevraagd in ${NODE_CONF}. Maak een keuze."
  exit 1
fi

if [[ $want_mtd -eq 1 ]]; then
  role="mtd"
  ROLE_OVERLAY="${ROLE_OVERLAY_MTD}"
elif [[ $want_ftd -eq 1 ]]; then
  role="ftd"
  ROLE_OVERLAY="${ROLE_OVERLAY_FTD}"
else
  # veilige default als niets is gezet: MTD
  role="mtd"
  ROLE_OVERLAY="${ROLE_OVERLAY_MTD}"
fi

[[ -f "${ROLE_OVERLAY}" ]] || { echo "ERROR: Missing ${ROLE_OVERLAY} (rol=${role})"; exit 1; }

# ---- OVERLAY_CONFIG opbouwen ----
# volgorde:
#   1) altijd 54l15 + OT-basis
#   2) evt. extra overlays via --overlays
#   3) rol-overlay (mtd/ftd)
#   4) node-overlay (als laatste, WINT)
OVERLAY_CONFIG="${ALWAYS_OVERLAY_54L15};${ALWAYS_OVERLAY_OT_BASIS}"
if [[ -n "${EXTRA_OVERLAYS}" ]]; then
  OVERLAY_CONFIG="${OVERLAY_CONFIG};${EXTRA_OVERLAYS}"
fi
OVERLAY_CONFIG="${OVERLAY_CONFIG};${ROLE_OVERLAY};${NODE_CONF}"

# Build dir per node
BUILD_DIR="${APP_DIR}/build/${NODE}"
mkdir -p "${BUILD_DIR}"

echo "-----------------------------------"
echo "Building for node  : ${NODE} (role=${role})"
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
