#!/usr/bin/env bash
# FAPS-3D one-click build & run script
# Usage:
#   TRACE_ROOT=/path/to/traces ./scripts/run_faps3d.sh
#
# Environment variables:
#   TRACE_ROOT  - (required) path to trace root directory
#   SKIP_BUILD  - set to 1 to skip build steps (default: 0)
#   JOBS        - parallelism for simulation (default: 128)
#   WARMUP      - warmup instructions (default: 20000000)
#   SIM         - simulation instructions (default: 80000000)
#   LABEL       - result label (default: FAPS_1c)
#   COMPARE_WITH - result directory to compare against (e.g., results/GS_1c)
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CHAMPSIM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DRAMSIM3_DIR="$(cd "$CHAMPSIM_DIR/../dramsim3" && pwd)"

TRACE_ROOT="${TRACE_ROOT:-}"
SKIP_BUILD="${SKIP_BUILD:-0}"
LABEL="${LABEL:-FAPS_1c}"
COMPARE_WITH="${COMPARE_WITH:-}"

cd "$CHAMPSIM_DIR"

# ===== Step 1: Build DRAMSim3 =====
if [[ "$SKIP_BUILD" != "1" ]]; then
    echo "========================================"
    echo "[1/3] Building DRAMSim3..."
    echo "========================================"
    pushd "$DRAMSIM3_DIR" >/dev/null
    make clean || true
    make -j8
    popd >/dev/null

    echo "========================================"
    echo "[2/3] Generating Makefile & building ChampSim (FAPS config)..."
    echo "========================================"
    python3 config.sh champsim_config_FAPS.json
    make -j8

    echo "========================================"
    echo "[3/3] Build complete."
    echo "========================================"
else
    echo "[INFO] Skipping build (SKIP_BUILD=1)"
fi

# ===== Step 2: Run simulations =====
export LD_LIBRARY_PATH="${DRAMSIM3_DIR}:${LD_LIBRARY_PATH:-}"

if [[ -z "$TRACE_ROOT" ]]; then
    echo ""
    echo "Build succeeded. To run simulations, set TRACE_ROOT:"
    echo "  TRACE_ROOT=/path/to/traces SKIP_BUILD=1 $0"
    echo ""
    exit 0
fi

echo ""
echo "========================================"
echo "Running FAPS-3D simulations..."
echo "  TRACE_ROOT=$TRACE_ROOT"
echo "  LABEL=$LABEL"
echo "========================================"

# Use benchmarks_selected.tsv (full benchmark suite) via run_benchmarks.sh
# benchmarks_selected.tsv has absolute trace paths, so TRACE_ROOT is not needed by run_benchmarks.sh
printf '%s\ny\n' "$LABEL" | MANIFEST=./benchmarks_selected.tsv scripts/run_benchmarks.sh

# ===== Step 3: Summarize results =====
echo ""
echo "========================================"
echo "Summarizing IPC results..."
echo "========================================"
if [[ -f scripts/summarize_ipc.py ]]; then
    python3 scripts/summarize_ipc.py "results/${LABEL}"
fi

# ===== Step 4: Compare with baseline (if specified) =====
if [[ -n "$COMPARE_WITH" ]]; then
    echo ""
    echo "========================================"
    echo "Comparing with baseline: $COMPARE_WITH"
    echo "========================================"
    COMPARE_LABEL="$(basename "$COMPARE_WITH")"
    python3 scripts/compare_ipc.py \
        --a "$COMPARE_WITH" \
        --b "results/${LABEL}" \
        --a-label "$COMPARE_LABEL" \
        --b-label "$LABEL" \
        --out "results/compare_${COMPARE_LABEL}_vs_${LABEL}.tsv"
fi

echo ""
echo "Done! Results are in results/${LABEL}/"
