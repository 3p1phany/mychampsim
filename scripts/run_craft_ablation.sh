#!/usr/bin/env bash
# =============================================================================
# CRAFT Ablation Experiment Runner
#
# Sequentially runs all ablation experiments defined in
# docs/experiments/craft_ablation_experiment_plan.md
#
# For each experiment:
#   1. Modify feature flags in dramsim3/src/command_queue.h
#   2. Rebuild DRAMSim3 + ChampSim
#   3. Run all benchmarks with the experiment label
#
# Usage:
#   TRACE_ROOT=/root/data/Trace/LA ./scripts/run_craft_ablation.sh [PHASE]
#
# PHASE (optional):
#   1  = Core ablation only         (A0-A6, 6 new experiments)
#   2  = Interaction analysis        (B1-B5, 5 experiments)
#   3  = Parameter sensitivity       (S1-S6, 6 experiments)
#   all = All phases (default)
#
# Environment variables:
#   TRACE_ROOT       - path to trace directory (REQUIRED)
#   MANIFEST_SRC     - path to selected_slices.tsv (default: ./scripts/selected_slices.tsv)
#   JOBS             - parallelism for benchmark runs (default: 128)
#   SKIP_EXISTING    - if 1, skip experiments whose results/<label>/.done exists (default: 1)
#   DRY_RUN          - if 1, only show what would be done (default: 0)
# =============================================================================
set -euo pipefail

# ── Paths ──
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CHAMPSIM_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_ROOT="$(cd "$CHAMPSIM_DIR/.." && pwd)"
DRAMSIM3_DIR="$PROJECT_ROOT/dramsim3"
HEADER="$DRAMSIM3_DIR/src/command_queue.h"
RESULTS_DIR="$CHAMPSIM_DIR/results"

# ── Config ──
TRACE_ROOT="${TRACE_ROOT:-}"
MANIFEST_SRC="${MANIFEST_SRC:-$CHAMPSIM_DIR/scripts/selected_slices.tsv}"
JOBS="${JOBS:-128}"
SKIP_EXISTING="${SKIP_EXISTING:-1}"
DRY_RUN="${DRY_RUN:-0}"
PHASE="${1:-all}"

# ── Validate ──
if [[ -z "$TRACE_ROOT" ]]; then
    echo "ERROR: TRACE_ROOT is required. Example:" >&2
    echo "  TRACE_ROOT=/root/data/Trace/LA $0" >&2
    exit 1
fi
[[ -d "$TRACE_ROOT" ]] || { echo "ERROR: TRACE_ROOT not found: $TRACE_ROOT" >&2; exit 1; }
[[ -f "$HEADER" ]] || { echo "ERROR: Header not found: $HEADER" >&2; exit 1; }
[[ -f "$MANIFEST_SRC" ]] || { echo "ERROR: Manifest not found: $MANIFEST_SRC" >&2; exit 1; }

# ── Log ──
LOG_DIR="$CHAMPSIM_DIR/logs"
mkdir -p "$LOG_DIR"
MASTER_LOG="$LOG_DIR/ablation_$(date +%Y%m%d_%H%M%S).log"
log() { echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" | tee -a "$MASTER_LOG"; }

# =============================================================================
# Feature flag manipulation
# =============================================================================

set_flag() {
    # Usage: set_flag FLAG_NAME true|false
    local name="$1" value="$2"
    sed -i "s/\(static constexpr bool ${name} = \)\(true\|false\)/\1${value}/" "$HEADER"
}

set_param() {
    # Usage: set_param PARAM_NAME value
    local name="$1" value="$2"
    sed -i "s/\(static constexpr int  ${name} = \)[0-9]*/\1${value}/" "$HEADER"
}

reset_all_flags() {
    # Reset all feature flags to false and parameters to defaults
    set_flag CRAFT_PHASE_RESET_ENABLED   false
    set_flag CRAFT_QDSD_ENABLED          false
    set_flag CRAFT_RIGHT_STREAK_ENABLED  false
    set_flag CRAFT_RW_ENABLED            false
    set_flag CRAFT_STREAK_DECAY_ENABLED  false
    # Reset parameters to defaults
    set_param CRAFT_PHASE_THRESHOLD 4
    set_param CRAFT_QDSD_SCALE_CAP  4
    set_param CRAFT_RIGHT_THRESHOLD 4
}

enable_all_flags() {
    set_flag CRAFT_PHASE_RESET_ENABLED   true
    set_flag CRAFT_QDSD_ENABLED          true
    set_flag CRAFT_RIGHT_STREAK_ENABLED  true
    set_flag CRAFT_RW_ENABLED            true
    set_flag CRAFT_STREAK_DECAY_ENABLED  true
}

show_flags() {
    grep -E 'CRAFT_(PHASE_RESET_ENABLED|QDSD_ENABLED|RIGHT_STREAK_ENABLED|RW_ENABLED|STREAK_DECAY_ENABLED|PHASE_THRESHOLD|QDSD_SCALE_CAP|RIGHT_THRESHOLD) =' "$HEADER" \
        | sed 's/static constexpr [a-z]* */  /'
}

# =============================================================================
# Build
# =============================================================================

build_dramsim3() {
    log "  Building DRAMSim3..."
    (cd "$DRAMSIM3_DIR/build" && make -j8) >> "$MASTER_LOG" 2>&1
}

build_champsim() {
    log "  Building ChampSim..."
    (cd "$CHAMPSIM_DIR" && make -j8) >> "$MASTER_LOG" 2>&1
}

rebuild_all() {
    build_dramsim3
    build_champsim
    log "  Build OK"
}

# =============================================================================
# Run benchmarks (non-interactive wrapper)
# =============================================================================

run_experiment() {
    local label="$1"

    # Check if already done
    if [[ "$SKIP_EXISTING" == "1" ]]; then
        local result_dir="$RESULTS_DIR/$label"
        if [[ -d "$result_dir" ]]; then
            # Check if all benchmarks completed
            local remaining_file="$result_dir/REMAINING.tsv"
            if [[ -f "$remaining_file" ]]; then
                local remaining
                remaining=$(awk '!/^[[:space:]]*($|#)/{c++} END{print c+0}' "$remaining_file")
                if [[ "$remaining" -eq 0 ]]; then
                    log "  SKIP: $label (all benchmarks already completed)"
                    return 0
                else
                    log "  RESUME: $label ($remaining benchmarks remaining)"
                fi
            else
                # No remaining file: check if TOTAL_TIME.txt exists with 0 remaining
                local total_file="$result_dir/TOTAL_TIME.txt"
                if [[ -f "$total_file" ]] && grep -q 'remaining_tasks=0' "$total_file" 2>/dev/null; then
                    log "  SKIP: $label (completed per TOTAL_TIME.txt)"
                    return 0
                fi
            fi
        fi
    fi

    if [[ "$DRY_RUN" == "1" ]]; then
        log "  DRY_RUN: would run $label"
        return 0
    fi

    log "  Running benchmarks: $label"

    # Prepare manifest (same logic as run_selected_slices.sh)
    local manifest_tmp
    manifest_tmp="$(mktemp)"
    awk -v root="$TRACE_ROOT" '
    BEGIN { FS = "[ \t]+"; OFS = "\t" }
    {
      sub(/\r$/, "", $0)
      if ($0 ~ /^[[:space:]]*$/) { next }
      if ($1 ~ /^#/) { print; next }
      if (tolower($1)=="benchmark" && tolower($2)=="slice") { print; next }

      bench=$1; slice=$2; weight=$3; trace=$4
      if (bench=="" || slice=="" || weight=="" || trace=="") { next }
      if (trace !~ "^/") trace = root "/" trace
      print bench, slice, weight, trace
    }
    ' "$MANIFEST_SRC" > "$manifest_tmp"

    # Run benchmarks non-interactively
    # Pipe label + "y" confirmation to stdin
    local run_log="$LOG_DIR/${label}.log"
    (
        cd "$CHAMPSIM_DIR"
        export LD_LIBRARY_PATH="$DRAMSIM3_DIR:${LD_LIBRARY_PATH:-}"
        export MANIFEST="$manifest_tmp"
        export JOBS="$JOBS"
        printf '%s\ny\n' "$label" | scripts/run_benchmarks.sh >> "$run_log" 2>&1
    )
    local rc=$?
    rm -f "$manifest_tmp"

    if [[ $rc -ne 0 ]]; then
        log "  WARNING: $label finished with exit code $rc (some benchmarks may have failed)"
        log "  See log: $run_log"
    else
        log "  DONE: $label"
    fi
    return 0  # Don't abort the whole ablation on individual experiment failure
}

# =============================================================================
# Experiment definitions
#
# Each function configures flags, rebuilds, and runs.
# =============================================================================

run_one() {
    # Usage: run_one LABEL setup_function
    local label="$1"
    shift
    log "============================================"
    log "Experiment: $label"

    # Configure flags
    "$@"
    log "  Flags:"
    show_flags | while IFS= read -r line; do log "  $line"; done

    # Build & Run
    rebuild_all
    run_experiment "$label"
    log ""
}

# ── A0: Baseline (all off) ──
setup_A0() { reset_all_flags; }

# ── A1: Phase Reset only ──
setup_A1() { reset_all_flags; set_flag CRAFT_PHASE_RESET_ENABLED true; }

# ── A2: QDSD only ──
setup_A2() { reset_all_flags; set_flag CRAFT_QDSD_ENABLED true; }

# ── A3: Right Streak only ──
setup_A3() { reset_all_flags; set_flag CRAFT_RIGHT_STREAK_ENABLED true; }

# ── A4: Read/Write Cost only ──
setup_A4() { reset_all_flags; set_flag CRAFT_RW_ENABLED true; }

# ── A5: Streak Decay only ──
setup_A5() { reset_all_flags; set_flag CRAFT_STREAK_DECAY_ENABLED true; }

# ── A6: All enhancements ──
setup_A6() { reset_all_flags; enable_all_flags; }

# ── B1: PR + SD ──
setup_B1() {
    reset_all_flags
    set_flag CRAFT_PHASE_RESET_ENABLED  true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
}

# ── B2: RW + QDSD ──
setup_B2() {
    reset_all_flags
    set_flag CRAFT_RW_ENABLED   true
    set_flag CRAFT_QDSD_ENABLED true
}

# ── B3: RS + SD ──
setup_B3() {
    reset_all_flags
    set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
}

# ── B4: All conflict-path (PR + QDSD + RW) ──
setup_B4() {
    reset_all_flags
    set_flag CRAFT_PHASE_RESET_ENABLED true
    set_flag CRAFT_QDSD_ENABLED        true
    set_flag CRAFT_RW_ENABLED          true
}

# ── B5: All precharge-path (RS + RW + SD) ──
setup_B5() {
    reset_all_flags
    set_flag CRAFT_RIGHT_STREAK_ENABLED true
    set_flag CRAFT_RW_ENABLED           true
    set_flag CRAFT_STREAK_DECAY_ENABLED true
}

# ── S1: Phase Threshold = 3 ──
setup_S1() {
    reset_all_flags
    set_flag  CRAFT_PHASE_RESET_ENABLED true
    set_param CRAFT_PHASE_THRESHOLD 3
}

# ── S2: Phase Threshold = 6 ──
setup_S2() {
    reset_all_flags
    set_flag  CRAFT_PHASE_RESET_ENABLED true
    set_param CRAFT_PHASE_THRESHOLD 6
}

# ── S3: QDSD Scale Cap = 2 ──
setup_S3() {
    reset_all_flags
    set_flag  CRAFT_QDSD_ENABLED true
    set_param CRAFT_QDSD_SCALE_CAP 2
}

# ── S4: QDSD Scale Cap = 8 ──
setup_S4() {
    reset_all_flags
    set_flag  CRAFT_QDSD_ENABLED true
    set_param CRAFT_QDSD_SCALE_CAP 8
}

# ── S5: Right Threshold = 3 ──
setup_S5() {
    reset_all_flags
    set_flag  CRAFT_RIGHT_STREAK_ENABLED true
    set_param CRAFT_RIGHT_THRESHOLD 3
}

# ── S6: Right Threshold = 6 ──
setup_S6() {
    reset_all_flags
    set_flag  CRAFT_RIGHT_STREAK_ENABLED true
    set_param CRAFT_RIGHT_THRESHOLD 6
}

# =============================================================================
# Restore header on exit (always leave code in "all enabled" state)
# =============================================================================

cleanup() {
    log "Restoring header to all-enabled state..."
    reset_all_flags
    enable_all_flags
    set_param CRAFT_PHASE_THRESHOLD 4
    set_param CRAFT_QDSD_SCALE_CAP  4
    set_param CRAFT_RIGHT_THRESHOLD 4
    log "Done. Header restored."
}
trap cleanup EXIT

# =============================================================================
# Main
# =============================================================================

log "============================================"
log "CRAFT Ablation Experiment Suite"
log "============================================"
log "Phase:        $PHASE"
log "TRACE_ROOT:   $TRACE_ROOT"
log "MANIFEST:     $MANIFEST_SRC"
log "JOBS:         $JOBS"
log "SKIP_EXISTING:$SKIP_EXISTING"
log "DRY_RUN:      $DRY_RUN"
log "Header:       $HEADER"
log "Master log:   $MASTER_LOG"
log ""

T0=$(date +%s)

# ── Phase 1: Core Ablation ──
if [[ "$PHASE" == "1" || "$PHASE" == "all" ]]; then
    log "========== Phase 1: Core Ablation =========="
    run_one "CRAFT_BASE_1c"     setup_A0   # A0: all flags off (pure CRAFT baseline)
    run_one "CRAFT_PR_1c"       setup_A1   # A1: Phase Reset only
    run_one "CRAFT_QDSD_1c"     setup_A2   # A2: QDSD only
    run_one "CRAFT_RS_1c"       setup_A3   # A3: Right Streak only
    run_one "CRAFT_RW_1c"       setup_A4   # A4: Read/Write Cost only
    run_one "CRAFT_SD_1c"       setup_A5   # A5: Streak Decay only
    run_one "CRAFT_ALL_1c"      setup_A6   # A6: All enhancements
fi

# ── Phase 2: Interaction Analysis ──
if [[ "$PHASE" == "2" || "$PHASE" == "all" ]]; then
    log "========== Phase 2: Interaction Analysis =========="
    run_one "CRAFT_PR_SD_1c"       setup_B1   # B1: PR + SD
    run_one "CRAFT_RW_QDSD_1c"     setup_B2   # B2: RW + QDSD
    run_one "CRAFT_RS_SD_1c"       setup_B3   # B3: RS + SD
    run_one "CRAFT_CONFLICT_1c"    setup_B4   # B4: PR + QDSD + RW
    run_one "CRAFT_PRECHARGE_1c"   setup_B5   # B5: RS + RW + SD
fi

# ── Phase 3: Parameter Sensitivity ──
if [[ "$PHASE" == "3" || "$PHASE" == "all" ]]; then
    log "========== Phase 3: Parameter Sensitivity =========="
    run_one "CRAFT_PR3_1c"       setup_S1   # S1: PHASE_THRESHOLD=3
    run_one "CRAFT_PR6_1c"       setup_S2   # S2: PHASE_THRESHOLD=6
    run_one "CRAFT_QDSD_C2_1c"   setup_S3   # S3: QDSD_SCALE_CAP=2
    run_one "CRAFT_QDSD_C8_1c"   setup_S4   # S4: QDSD_SCALE_CAP=8
    run_one "CRAFT_RS3_1c"       setup_S5   # S5: RIGHT_THRESHOLD=3
    run_one "CRAFT_RS6_1c"       setup_S6   # S6: RIGHT_THRESHOLD=6
fi

T1=$(date +%s)
ELAPSED=$(( T1 - T0 ))
HOURS=$(( ELAPSED / 3600 ))
MINS=$(( (ELAPSED % 3600) / 60 ))
SECS=$(( ELAPSED % 60 ))

log "============================================"
log "All ablation experiments complete."
log "Total time: ${HOURS}h ${MINS}m ${SECS}s"
log "Results in: $RESULTS_DIR"
log "Master log: $MASTER_LOG"
log "============================================"

# ── Generate comparison summary ──
log ""
log "To compare results, run:"
log "  cd $CHAMPSIM_DIR"
LABELS=("CRAFT_BASE_1c" "CRAFT_PR_1c" "CRAFT_QDSD_1c" "CRAFT_RS_1c" "CRAFT_RW_1c" "CRAFT_SD_1c" "CRAFT_ALL_1c")
for lbl in "${LABELS[@]}"; do
    if [[ "$lbl" != "CRAFT_BASE_1c" ]]; then
        log "  python3 scripts/compare_ipc.py --a results/CRAFT_BASE_1c --b results/$lbl --a-label CRAFT --b-label $lbl"
    fi
done
