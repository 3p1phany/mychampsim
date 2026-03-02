#!/usr/bin/env bash
# One-click build & run script for RL_PAGE experiment
# Usage: ./scripts/run_rlpage.sh [--build-only] [--run-only] [--quick-test]
#
# Options:
#   --build-only   Only build, don't run benchmarks
#   --run-only     Skip build, only run benchmarks (assumes already built)
#   --quick-test   Run a single trace with warmup=1M, sim=5M for quick verification
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHAMPSIM_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
DRAMSIM3_ROOT="$(cd "$CHAMPSIM_ROOT/../dramsim3" && pwd)"

BUILD=1
RUN=1
QUICK_TEST=0

for arg in "$@"; do
    case "$arg" in
        --build-only) RUN=0 ;;
        --run-only)   BUILD=0 ;;
        --quick-test) QUICK_TEST=1 ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

cd "$CHAMPSIM_ROOT"

# =========================================================
# Step 1: Build DRAMSim3 + ChampSim with RL_PAGE config
# =========================================================
if [[ "$BUILD" -eq 1 ]]; then
    echo "============================================"
    echo " Building DRAMSim3 with RL_PAGE support"
    echo "============================================"

    # Build DRAMSim3 (plain Makefile for simplicity)
    echo "[1/3] Compiling DRAMSim3..."
    pushd "$DRAMSIM3_ROOT" >/dev/null
    make clean 2>/dev/null || true
    make -j$(nproc)
    popd >/dev/null

    # Clean ChampSim first (before config.sh, since make clean removes generated headers)
    echo "[2/4] Cleaning ChampSim..."
    make clean 2>/dev/null || true

    # Generate ChampSim Makefile/headers from RL_PAGE config
    echo "[3/4] Generating ChampSim build files from champsim_config_RLPAGE.json..."
    python3 config.sh champsim_config_RLPAGE.json

    # Build ChampSim
    echo "[4/4] Compiling ChampSim..."
    make -j$(nproc)

    echo ""
    echo "Build successful! Binary: $CHAMPSIM_ROOT/bin/champsim"
    echo ""
fi

# Set LD_LIBRARY_PATH for DRAMSim3 dynamic linking
export LD_LIBRARY_PATH="${DRAMSIM3_ROOT}:${LD_LIBRARY_PATH:-}"

# =========================================================
# Step 2: Quick verification test (optional)
# =========================================================
if [[ "$QUICK_TEST" -eq 1 ]]; then
    echo "============================================"
    echo " Quick Verification Test (warmup=1M, sim=5M)"
    echo "============================================"

    # Find the first available trace from selected_slices.tsv
    TRACE_ROOT="${TRACE_ROOT:-/root/data/Trace/LA}"
    FIRST_TRACE=""

    if [[ -f "$CHAMPSIM_ROOT/scripts/selected_slices.tsv" ]]; then
        FIRST_TRACE=$(awk '
        BEGIN { FS="[ \t]+" }
        {
            sub(/\r$/, "", $0)
            if ($0 ~ /^[[:space:]]*$/) next
            if ($1 ~ /^#/) next
            if (tolower($1)=="benchmark" && tolower($2)=="slice") next
            trace=$4
            if (trace != "") {
                if (trace !~ "^/") trace = "'"$TRACE_ROOT"'/" trace
                print trace
                exit
            }
        }
        ' "$CHAMPSIM_ROOT/scripts/selected_slices.tsv")
    fi

    if [[ -z "$FIRST_TRACE" || ! -f "$FIRST_TRACE" ]]; then
        echo "[WARN] Cannot find a trace file for quick test."
        echo "       Set TRACE_ROOT or check scripts/selected_slices.tsv"
        if [[ "$RUN" -eq 0 ]]; then exit 0; fi
    else
        QUICK_DIR="$CHAMPSIM_ROOT/results/RLPAGE_quicktest"
        mkdir -p "$QUICK_DIR"
        echo "Trace: $FIRST_TRACE"
        echo "Output: $QUICK_DIR/run.log"
        echo ""

        "$CHAMPSIM_ROOT/bin/champsim" \
            --warmup_instructions 1000000 \
            --simulation_instructions 5000000 \
            -loongarch \
            "$FIRST_TRACE" \
            > "$QUICK_DIR/run.log" 2>&1 || true

        # Verify RL_PAGE counters in output
        echo "--- RL_PAGE Verification ---"
        if grep -q "rlpage_decisions" "$QUICK_DIR/run.log" 2>/dev/null; then
            grep "rlpage_" "$QUICK_DIR/run.log" | head -20
            echo ""

            # Check key invariants
            decisions=$(grep "rlpage_decisions" "$QUICK_DIR/run.log" | tail -1 | awk '{print $3}')
            close_cnt=$(grep "rlpage_close_count" "$QUICK_DIR/run.log" | tail -1 | awk '{print $3}')
            open_cnt=$(grep "rlpage_keepopen_count" "$QUICK_DIR/run.log" | tail -1 | awk '{print $3}')
            explorations=$(grep "rlpage_explorations" "$QUICK_DIR/run.log" | tail -1 | awk '{print $3}')

            if [[ -n "$decisions" && "$decisions" -gt 0 ]]; then
                echo "PASS: rlpage_decisions = $decisions (> 0)"
                sum=$((close_cnt + open_cnt))
                if [[ "$sum" -eq "$decisions" ]]; then
                    echo "PASS: close($close_cnt) + keepopen($open_cnt) = decisions($decisions)"
                else
                    echo "WARN: close($close_cnt) + keepopen($open_cnt) = $sum != decisions($decisions)"
                fi
                if [[ -n "$explorations" && "$explorations" -gt 0 ]]; then
                    echo "PASS: explorations = $explorations (> 0, epsilon-greedy working)"
                fi
            else
                echo "WARN: rlpage_decisions = 0 or not found"
            fi
        else
            echo "WARN: rlpage counters not found in output"
            echo "Last 30 lines of run.log:"
            tail -30 "$QUICK_DIR/run.log"
        fi
        echo "----------------------------"
        echo ""
    fi

    if [[ "$RUN" -eq 0 ]]; then
        echo "Quick test done. Exiting (--build-only or no --run flag)."
        exit 0
    fi
fi

# =========================================================
# Step 3: Full benchmark run
# =========================================================
if [[ "$RUN" -eq 1 && "$QUICK_TEST" -eq 0 ]]; then
    echo "============================================"
    echo " Running Full RL_PAGE Benchmark Suite"
    echo "============================================"
    echo ""
    echo "Using full benchmark manifest: benchmarks_selected.tsv"
    echo "Label will be set to: RLPAGE_1c"
    echo ""

    export TRACE_ROOT="${TRACE_ROOT:-/root/data/Trace/LA}"
    export BINARY="$CHAMPSIM_ROOT/bin/champsim"
    export MANIFEST="${MANIFEST:-$CHAMPSIM_ROOT/benchmarks_selected.tsv}"
    export JOBS="${JOBS:-128}"
    export WARMUP="${WARMUP:-20000000}"
    export SIM="${SIM:-80000000}"

    # run_benchmarks.sh has two read prompts: (1) label, (2) confirm y/N
    printf "RLPAGE_1c\ny\n" | MANIFEST="$MANIFEST" \
        BINARY="$BINARY" \
        JOBS="$JOBS" \
        WARMUP="$WARMUP" \
        SIM="$SIM" \
        "$CHAMPSIM_ROOT/scripts/run_benchmarks.sh"

    echo ""
    echo "============================================"
    echo " RL_PAGE Benchmark Run Complete!"
    echo "============================================"
    echo "Results: $CHAMPSIM_ROOT/results/RLPAGE_1c/"
    echo ""
    echo "To compare with GS baseline:"
    echo "  python3 scripts/compare_ipc.py --a results/GS_1c --b results/RLPAGE_1c --a-label GS --b-label RLPAGE"
    echo ""
    echo "To compare with DYMPL:"
    echo "  python3 scripts/compare_ipc.py --a results/DYMPL_1c --b results/RLPAGE_1c --a-label DYMPL --b-label RLPAGE"
fi
