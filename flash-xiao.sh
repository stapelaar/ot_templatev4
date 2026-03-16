#!/usr/bin/env bash
set -euo pipefail

# flash-xiao.sh
# - Always flashes XIAO nRF54L15 (cpuapp)
# - Uses per-node build dir: build/54l15/<NDxx>

APP_DIR="$(cd "$(dirname "$0")" && pwd -P)"

usage() {
  cat <<'EOF2'
Usage:
  ./flash-xiao.sh --node NDxx [--runner <name>] [--build-dir <dir>]

Notes:
  - Target is fixed: XIAO nRF54L15 (cpuapp)
  - Default build dir: build/54l15/<NDxx>

Options:
  --node NDxx            Node profile (required)
  -r, --runner <name>    west runner (openocd|jlink|nrfjprog|nrfutil)
  -d, --build-dir <dir>  Override build directory
  -h, --help             Show this help

Examples:
  ./flash-xiao.sh --node ND30
  ./flash-xiao.sh --node ND30 -r openocd
  ./flash-xiao.sh --node ND30 -d build/54l15/ND30
EOF2
}

NODE="${NODE:-}"
RUNNER="${RUNNER:-}"
BUILD_DIR_OVERRIDE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --node)
      NODE="${2:-}"; [[ -n "$NODE" ]] || { echo "ERROR: --node requires NDxx (e.g. ND30)" >&2; exit 2; }
      shift 2;;
    -r|--runner)
      RUNNER="${2:-}"; shift 2;;
    -d|--build-dir)
      BUILD_DIR_OVERRIDE="${2:-}"; shift 2;;
    -h|--help|help)
      usage; exit 0;;
    *)
      echo "Unknown option: $1" >&2
      usage
      exit 2;;
  esac
done

if [[ -z "$NODE" ]]; then
  echo "ERROR: node is required. Use --node NDxx (e.g. --node ND30)" >&2
  usage
  exit 2
fi

BUILD_DIR="${BUILD_DIR_OVERRIDE:-$APP_DIR/build/$NODE}"

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "❌ Build dir not found: $BUILD_DIR" >&2
  echo "   Tip: build first: ./build-xiao.sh --node $NODE" >&2
  exit 1
fi

echo "Flashing node      : $NODE"
echo "Build dir          : $BUILD_DIR"
if [[ -n "$RUNNER" ]]; then
  echo "Runner             : $RUNNER"
else
  echo "Runner             : <default>"
fi
echo "-----------------------------------"

if [[ -n "$RUNNER" ]]; then
  west flash -d "$BUILD_DIR" -r "$RUNNER"
else
  west flash -d "$BUILD_DIR"
fi

echo "✅ Done."
