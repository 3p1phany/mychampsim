#!/usr/bin/env bash
# =============================================================================
# CRAFT Ablation Experiment Runner (one-click, method 3: dedicated config JSON)
#
# Usage:
#   ./scripts/run_craft_ablation.sh                # Phase 1+2+3, full benchmark set
#   PHASE=1   ./scripts/run_craft_ablation.sh      # Phase 1 only (core ablation, 7 experiments)
#   PHASE=12  ./scripts/run_craft_ablation.sh      # Phase 1+2
#   USE_SELECTED=1 ./scripts/run_craft_ablation.sh # 3-benchmark quick test
#   JOBS=64   ./scripts/run_craft_ablation.sh      # custom parallelism
#   DRY_RUN=1 ./scripts/run_craft_ablation.sh      # print only, no execution
# =============================================================================
set -euo pipefail

# ======================== Paths ========================
PROJ_ROOT="/root/data/smartPRE"
CHAMPSIM_DIR="$PROJ_ROOT/champsim-la"
DRAMSIM3_DIR="$PROJ_ROOT/dramsim3"
HEADER="$DRAMSIM3_DIR/src/command_queue.h"
CRAFT_CONFIG_JSON="$CHAMPSIM_DIR/champsim_config_CRAFT.json"

# ======================== Options ========================
PHASE="${PHASE:-123}"
USE_SELECTED="${USE_SELECTED:-0}"
JOBS="${JOBS:-128}"
DRY_RUN="${DRY_RUN:-0}"
SKIP_EXISTING="${SKIP_EXISTING:-1}"

export LD_LIBRARY_PATH="$DRAMSIM3_DIR:${LD_LIBRARY_PATH:-}"
export JOBS
export DRY_RUN

# ======================== Helpers ========================
log() { printf "[%s] %s\n" "$(date '+%H:%M:%S')" "$*"; }

set_flag() {
    sed -i "s/\(static constexpr bool ${1} = \)\(true\|false\)/\1${2}/" "$HEADER"
}

set_param() {
    sed -i "s/\(static constexpr int *${1} = \)[0-9]*/\1${2}/" "$HEADER"
}

all_flags_off() {
    set_flag CRAFT_PHASE_RESET_ENABLED   false
    set_flag CRAFT_QDSD_ENABLED          false
    set_flag CRAFT_RIGHT_STREAK_ENABLED  false
    set_flag CRAFT_RW_ENABLED            false
    set_flag CRAFT_STREAK_DECAY_ENABLED  false
}

default_params() {
    set_param CRAFT_PHASE_THRESHOLD 4
    set_param CRAFT_QDSD_SCALE_CAP  4
    set_param CRAFT_RIGHT_THRESHOLD 4
}

show_config() {
    grep -E "CRAFT_(PHASE_RESET|QDSD|RIGHT_STREAK|RW|STREAK_DECAY)_ENABLED" "$HEADER" | sed 's/.*bool /    /'
    grep -E "CRAFT_(PHASE_THRESHOLD|QDSD_SCALE_CAP|RIGHT_THRESHOLD) " "$HEADER" | sed 's/.*int */    /'
}

# ======================== Build ========================
build_all() {
    # 1. Rebuild DRAMSim3 (feature flags changed)
    log "  build DRAMSim3..."
    (cd "$DRAMSIM3_DIR/build" && make -j8) > /dev/null 2>&1

    # 2. Regenerate ChampSim with CRAFT config JSON & rebuild
    log "  build ChampSim (champsim_config_CRAFT.json)..."
    cd "$CHAMPSIM_DIR"
    python3 config.sh champsim_config_CRAFT.json > /dev/null 2>&1
    make -j8 > /dev/null 2>&1

    log "  build OK"
}

# ======================== Run ========================
run_one() {
    local label="$1"
    log "========================================"
    log "Experiment: $label"
    show_config

    # Skip if already complete
    if [[ "$SKIP_EXISTING" == "1" ]]; then
        local rdir="$CHAMPSIM_DIR/results/$label"
        if [[ -f "$rdir/REMAINING.tsv" ]]; then
            local rem
            rem=$(awk '!/^[[:space:]]*($|#)/{c++} END{print c+0}' "$rdir/REMAINING.tsv")
            if [[ "$rem" -eq 0 ]]; then
                log "  SKIP (already complete)"
                return 0
            fi
            log "  RESUME ($rem remaining)"
        fi
    fi

    if [[ "$DRY_RUN" == "1" ]]; then
        log "  DRY_RUN: would run $label"
        return 0
    fi

    build_all

    # Run benchmarks non-interactively
    log "  running benchmarks..."
    cd "$CHAMPSIM_DIR"

    if [[ "$USE_SELECTED" == "1" ]]; then
        printf '%s\ny\n' "$label" | \
            MANIFEST_SRC=./scripts/selected_slices.tsv \
            TRACE_ROOT=/root/data/Trace/LA \
            scripts/run_selected_slices.sh > /dev/null 2>&1 || true
    else
        printf '%s\ny\n' "$label" | \
            MANIFEST=./benchmarks_selected.tsv \
            scripts/run_benchmarks.sh > /dev/null 2>&1 || true
    fi

    # Verify CRAFT policy in first run.log
    local first_log
    first_log=$(find "$CHAMPSIM_DIR/results/$label" -name run.log -print -quit 2>/dev/null || true)
    if [[ -n "$first_log" ]] && grep -q "row buffer policy CRAFT" "$first_log"; then
        log "  VERIFIED: CRAFT policy active"
    elif [[ -n "$first_log" ]]; then
        log "  *** ERROR: CRAFT policy NOT detected! ***"
        head -15 "$first_log" | grep "row buffer policy" || true
    fi

    # Generate summary.tsv
    local rdir="$CHAMPSIM_DIR/results/$label"
    if [[ -d "$rdir" ]]; then
        local mf="$rdir/MANIFEST.tsv"
        [[ -f "$mf" ]] || mf="$CHAMPSIM_DIR/benchmarks_selected.tsv"
        python3 scripts/summarize_ipc.py --results "$rdir" --manifest "$mf" 2>/dev/null || true
    fi

    log "  DONE: $label"
}

# ======================== Summary ========================
final_summary() {
    log "========================================"
    log "Comparisons vs CRAFT_1c baseline"
    cd "$CHAMPSIM_DIR"

    local baseline="results/CRAFT_1c"
    if [[ ! -f "$baseline/summary.tsv" ]]; then
        log "WARN: $baseline/summary.tsv not found, skip"
        return
    fi

    printf "\n  %-25s %s\n" "Experiment" "GEOMEAN speedup"
    printf "  %-25s %s\n"   "----------" "---------------"

    for dir in results/CRAFT_*_1c; do
        local label
        label=$(basename "$dir")
        [[ "$label" == "CRAFT_1c" || "$label" == "CRAFT_AC10" ]] && continue
        [[ ! -f "$dir/summary.tsv" ]] && continue

        python3 scripts/compare_ipc.py \
            --a "$baseline" --b "$dir" \
            --a-label CRAFT --b-label "$label" \
            --out "results/compare_CRAFT_vs_${label}.tsv" 2>/dev/null || continue

        local geomean
        geomean=$(tail -1 "results/compare_CRAFT_vs_${label}.tsv" | cut -f6)
        printf "  %-25s %s\n" "$label" "$geomean"
    done
    echo ""
}

# ======================== Cleanup on exit ========================
cleanup() {
    log "Restoring: flags=ALL ON, params=default, config=default JSON"
    all_flags_off
    set_flag CRAFT_PHASE_RESET_ENABLED   true
    set_flag CRAFT_QDSD_ENABLED          true
    set_flag CRAFT_RIGHT_STREAK_ENABLED  true
    set_flag CRAFT_RW_ENABLED            true
    set_flag CRAFT_STREAK_DECAY_ENABLED  true
    default_params
    (cd "$DRAMSIM3_DIR/build" && make -j8) > /dev/null 2>&1 || true
    # Restore ChampSim to default config (GS)
    cd "$CHAMPSIM_DIR"
    python3 config.sh champsim_config.json > /dev/null 2>&1 || true
    make -j8 > /dev/null 2>&1 || true
    log "Restored."
}
trap cleanup EXIT

# ======================== Pre-flight ========================
[[ -f "$HEADER" ]]           || { echo "ERR: $HEADER not found"; exit 1; }
[[ -f "$CRAFT_CONFIG_JSON" ]] || { echo "ERR: $CRAFT_CONFIG_JSON not found"; exit 1; }
[[ -d "$DRAMSIM3_DIR/build" ]] || { echo "ERR: DRAMSim3 build dir missing"; exit 1; }

log "CRAFT Ablation Runner"
log "  PHASE=$PHASE  USE_SELECTED=$USE_SELECTED  JOBS=$JOBS  DRY_RUN=$DRY_RUN"
echo ""

T0=$(date +%s)

# ======================== Phase 1: Core Ablation ========================
if [[ "$PHASE" == *1* ]]; then
    log "===== Phase 1: Core Ablation (7 experiments) ====="

    # A0: all flags off (pure CRAFT baseline with no enhancements)
    all_flags_off; default_params
    run_one "CRAFT_BASE_1c"

    # A1: Phase Reset only
    all_flags_off; default_params; set_flag CRAFT_PHASE_RESET_ENABLED true
    run_one "CRAFT_PR_1c"

    # A2: QDSD only
    all_flags_off; default_params; set_flag CRAFT_QDSD_ENABLED true
    run_one "CRAFT_QDSD_1c"

    # A3: Right Streak only
    all_flags_off; default_params; set_flag CRAFT_RIGHT_STREAK_ENABLED true
    run_one "CRAFT_RS_1c"

    # A4: Read/Write Cost only
    all_flags_off; default_params; set_flag CRAFT_RW_ENABLED true
    run_one "CRAFT_RW_1c"

    # A5: Streak Decay only
    all_flags_off; default_params; set_flag CRAFT_STREAK_DECAY_ENABLED true
    run_one "CRAFT_SD_1c"

    # A6: All enhancements
    all_flags_off; default_params
    set_flag CRAFT_PHASE_RESET_ENABLED  true
    set_flag CRAFT_QDSD_ENABLED         true
    set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_flag CRAFT_RW_ENABLED           true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
    run_one "CRAFT_ALL_1c"
fi

# ======================== Phase 2: Interaction Analysis ========================
if [[ "$PHASE" == *2* ]]; then
    log "===== Phase 2: Interaction Analysis (5 experiments) ====="

    # B1: PR + SD
    all_flags_off; default_params
    set_flag CRAFT_PHASE_RESET_ENABLED  true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
    run_one "CRAFT_PR_SD_1c"

    # B2: RW + QDSD
    all_flags_off; default_params
    set_flag CRAFT_RW_ENABLED   true
    set_flag CRAFT_QDSD_ENABLED true
    run_one "CRAFT_RW_QDSD_1c"

    # B3: RS + SD
    all_flags_off; default_params
    set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
    run_one "CRAFT_RS_SD_1c"

    # B4: All conflict-path (PR + QDSD + RW)
    all_flags_off; default_params
    set_flag CRAFT_PHASE_RESET_ENABLED true
    set_flag CRAFT_QDSD_ENABLED        true
    set_flag CRAFT_RW_ENABLED          true
    run_one "CRAFT_CONFLICT_1c"

    # B5: All precharge-path (RS + RW + SD)
    all_flags_off; default_params
    set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_flag CRAFT_RW_ENABLED           true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
    run_one "CRAFT_PRECHARGE_1c"
fi

# ======================== Phase 3: Parameter Sensitivity ========================
if [[ "$PHASE" == *3* ]]; then
    log "===== Phase 3: Parameter Sensitivity (6 experiments) ====="

    # S1: PHASE_THRESHOLD=3
    all_flags_off; default_params; set_flag CRAFT_PHASE_RESET_ENABLED true
    set_param CRAFT_PHASE_THRESHOLD 3
    run_one "CRAFT_PR3_1c"

    # S2: PHASE_THRESHOLD=6
    all_flags_off; default_params; set_flag CRAFT_PHASE_RESET_ENABLED true
    set_param CRAFT_PHASE_THRESHOLD 6
    run_one "CRAFT_PR6_1c"

    # S3: QDSD_SCALE_CAP=2
    all_flags_off; default_params; set_flag CRAFT_QDSD_ENABLED true
    set_param CRAFT_QDSD_SCALE_CAP 2
    run_one "CRAFT_QDSD_C2_1c"

    # S4: QDSD_SCALE_CAP=8
    all_flags_off; default_params; set_flag CRAFT_QDSD_ENABLED true
    set_param CRAFT_QDSD_SCALE_CAP 8
    run_one "CRAFT_QDSD_C8_1c"

    # S5: RIGHT_THRESHOLD=3
    all_flags_off; default_params; set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_param CRAFT_RIGHT_THRESHOLD 3
    run_one "CRAFT_RS3_1c"

    # S6: RIGHT_THRESHOLD=6
    all_flags_off; default_params; set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_param CRAFT_RIGHT_THRESHOLD 6
    run_one "CRAFT_RS6_1c"
fi

# ======================== Final ========================
T1=$(date +%s)
ELAPSED=$(( T1 - T0 ))
log "Total time: $((ELAPSED/3600))h $(((ELAPSED%3600)/60))m $((ELAPSED%60))s"

final_summary
