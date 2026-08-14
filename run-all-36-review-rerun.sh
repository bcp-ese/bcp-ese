#!/usr/bin/env bash

# Run the complete 36-configuration matrix reported in the manuscript.
# This intentionally excludes pairwise, incremental "both", and Xa(no-cache).

set -Eeuo pipefail

expected_tag="${BCP_RELEASE_TAG:-review-rerun-rc6}"
expected_dataset_count=53
repetitions=3
time_limit=3600
concurrency=1
save_interval_seconds="${BCP_SAVE_INTERVAL_SECONDS:-900}"

usage() {
    echo "Usage: $0 REPO_AT_RELEASE_TAG SESSION_DIR [--dry-run]" >&2
    echo "Example: $0 ../bcp-rerun ../rerun-output/review-rerun-rc6" >&2
}

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    usage
    exit 2
fi

repo_dir="$1"
session_dir="$2"
dry_run=false
if [ "${3:-}" = "--dry-run" ]; then
    dry_run=true
elif [ "$#" -eq 3 ]; then
    usage
    exit 2
fi

repo_dir="$(cd "$repo_dir" && pwd)"
mkdir -p "$session_dir"
session_dir="$(cd "$session_dir" && pwd)"
binary_path="${BCP_BINARY:-$repo_dir/bcp}"
result_dir="$repo_dir/result"
state_dir="$session_dir/state"
log_dir="$session_dir/logs"
start_marker="$session_dir/.session-start"

mkdir -p "$result_dir" "$state_dir" "$log_dir"
if [ ! -e "$start_marker" ]; then
    touch "$start_marker"
fi

if [ "$(git -C "$repo_dir" cat-file -t "refs/tags/$expected_tag" 2>/dev/null || true)" != "tag" ]; then
    echo "ERROR: $expected_tag must exist as an annotated tag in $repo_dir." >&2
    echo "Commit and tag the reviewed logging changes before starting official runs." >&2
    exit 1
fi

if ! expected_sha="$(git -C "$repo_dir" rev-parse "refs/tags/$expected_tag^{}" 2>/dev/null)"; then
    echo "ERROR: annotated release tag $expected_tag does not exist in $repo_dir." >&2
    echo "Commit and tag the reviewed logging changes before starting official runs." >&2
    exit 1
fi

actual_sha="$(git -C "$repo_dir" rev-parse HEAD)"
if [ "$actual_sha" != "$expected_sha" ]; then
    echo "ERROR: expected release SHA $expected_sha, found $actual_sha" >&2
    echo "Use a detached worktree at refs/tags/$expected_tag." >&2
    exit 1
fi

actual_tag="$(git -C "$repo_dir" describe --tags --exact-match HEAD 2>/dev/null || true)"
if [ "$actual_tag" != "$expected_tag" ]; then
    echo "ERROR: HEAD is not the exact annotated tag $expected_tag." >&2
    exit 1
fi

dirty_paths="$(git -C "$repo_dir" status --porcelain --untracked-files=all -- . ':(exclude)result/**')"
if [ -n "$dirty_paths" ]; then
    echo "ERROR: release worktree is dirty outside result/:" >&2
    echo "$dirty_paths" >&2
    exit 1
fi

if [ ! -x "$binary_path" ]; then
    echo "ERROR: executable not found: $binary_path" >&2
    exit 1
fi

dataset_count="$(find "$repo_dir/dataset" -maxdepth 1 -type f -name '*.col' | wc -l | tr -d ' ')"
if [ "$dataset_count" -ne "$expected_dataset_count" ]; then
    echo "ERROR: expected $expected_dataset_count .col files, found $dataset_count." >&2
    exit 1
fi

python3 -c 'import pandas, psutil' >/dev/null
(cd "$repo_dir" && python3 -m unittest test/test_benchmark.py)

find_latest_resume() {
    local config_id="$1"
    local latest=""
    local candidate
    while IFS= read -r -d '' candidate; do
        if [ -z "$latest" ] || [ "$candidate" -nt "$latest" ]; then
            latest="$candidate"
        fi
    done < <(
        find "$result_dir" -maxdepth 1 -type f -newer "$start_marker" \
            \( -name "${config_id}_CaDiCaL_partial_*.csv" \
            -o -name "${config_id}_CaDiCaL_crash_*.csv" \
            -o -name "${config_id}_CaDiCaL_interrupted_*.csv" \) -print0
    )
    printf '%s' "$latest"
}

find_latest_final() {
    local config_id="$1"
    local latest=""
    local candidate
    while IFS= read -r -d '' candidate; do
        if [ -z "$latest" ] || [ "$candidate" -nt "$latest" ]; then
            latest="$candidate"
        fi
    done < <(
        find "$result_dir" -maxdepth 1 -type f -newer "$start_marker" \
            -name "${config_id}_CaDiCaL_????-??-??-??-??-??.csv" -print0
    )
    printf '%s' "$latest"
}

validate_final_csv() {
    local csv_path="$1"
    python3 - "$csv_path" "$expected_sha" "$expected_dataset_count" "$repetitions" \
        "$time_limit" "$concurrency" <<'PY'
import collections
import csv
import math
import pathlib
import sys

csv_path = pathlib.Path(sys.argv[1])
expected_sha = sys.argv[2]
expected_instances = int(sys.argv[3])
expected_repetitions = int(sys.argv[4])
expected_time_limit = sys.argv[5]
expected_concurrency = sys.argv[6]

with csv_path.open(newline='', encoding='utf-8') as handle:
    reader = csv.DictReader(handle)
    rows = list(reader)
    fieldnames = set(reader.fieldnames or [])

analysis_counters = {
    'conflicts', 'decisions', 'propagations', 'learned', 'learned_lits',
    'restarts', 'reduced'
}
timing_columns = {'encoding_time', 'total_solving_time', 'time_used'}
missing_counters = analysis_counters - fieldnames
if missing_counters:
    raise SystemExit(
        f'{csv_path}: missing required CaDiCaL counters {sorted(missing_counters)}'
    )
missing_timing = timing_columns - fieldnames
if missing_timing:
    raise SystemExit(f'{csv_path}: missing timing columns {sorted(missing_timing)}')

expected_rows = expected_instances * expected_repetitions
if len(rows) != expected_rows:
    raise SystemExit(f'{csv_path}: expected {expected_rows} rows, found {len(rows)}')

def values(column):
    return {row[column].strip() for row in rows}

checks = {
    'source_sha': {expected_sha},
    'source_dirty': {'False'},
    'solver': {'CaDiCaL'},
    'solver_seed': {'0'},
    'time_limit': {expected_time_limit},
    'concurrency': {expected_concurrency},
    'run_id': {str(i) for i in range(1, expected_repetitions + 1)},
}
for column, expected in checks.items():
    actual = values(column)
    if actual != expected:
        raise SystemExit(
            f'{csv_path}: {column} expected {sorted(expected)}, found {sorted(actual)}'
        )

for row_number, row in enumerate(rows, start=2):
    for column in analysis_counters:
        raw_value = row[column].strip()
        try:
            value = float(raw_value)
        except ValueError as error:
            raise SystemExit(
                f'{csv_path}:{row_number}: {column} is not numeric: {raw_value!r}'
            ) from error
        if not math.isfinite(value) or value < 0:
            raise SystemExit(
                f'{csv_path}:{row_number}: {column} must be finite and nonnegative, found {raw_value!r}'
            )
    timing = {}
    for column in timing_columns:
        raw_value = row[column].strip()
        try:
            value = float(raw_value)
        except ValueError as error:
            raise SystemExit(
                f'{csv_path}:{row_number}: {column} is not numeric: {raw_value!r}'
            ) from error
        if not math.isfinite(value) or value < 0:
            raise SystemExit(
                f'{csv_path}:{row_number}: {column} must be finite and nonnegative, found {raw_value!r}'
            )
        timing[column] = value
    expected_total = timing['encoding_time'] + timing['total_solving_time']
    if not math.isclose(timing['time_used'], expected_total, rel_tol=1e-9, abs_tol=1e-9):
        raise SystemExit(
            f'{csv_path}:{row_number}: time_used does not equal encoding plus solving time'
        )

counts = collections.Counter(row['name'] for row in rows)
if len(counts) != expected_instances or set(counts.values()) != {expected_repetitions}:
    raise SystemExit(
        f'{csv_path}: each of {expected_instances} instances must occur '
        f'exactly {expected_repetitions} times'
    )
keys = [(row['name'], int(row['run_id'])) for row in rows]
if len(set(keys)) != len(keys):
    raise SystemExit(f'{csv_path}: duplicate (name, run_id) rows')
run_ids_by_instance = collections.defaultdict(set)
for name, run_id in keys:
    run_ids_by_instance[name].add(run_id)
expected_run_ids = set(range(1, expected_repetitions + 1))
if any(run_ids != expected_run_ids for run_ids in run_ids_by_instance.values()):
    raise SystemExit(f'{csv_path}: each instance must contain every expected run_id exactly once')
PY
}

print_command() {
    local arg
    for arg in "$@"; do
        printf '%q ' "$arg"
    done
    printf '\n'
}

run_one() {
    local config_id="$1"
    local method="$2"
    shift 2

    local done_file="$state_dir/${config_id}.done"
    local log_file="$log_dir/${config_id}.log"
    local final_csv=""
    local resume_csv=""

    if [ -f "$done_file" ]; then
        final_csv="$(sed -n '1p' "$done_file")"
        if [ -f "$final_csv" ]; then
            validate_final_csv "$final_csv"
            echo "SKIP complete: $config_id"
            return
        fi
        echo "ERROR: completion marker exists but CSV is missing: $final_csv" >&2
        exit 1
    fi

    final_csv="$(find_latest_final "$config_id")"
    if [ -n "$final_csv" ]; then
        validate_final_csv "$final_csv"
        printf '%s\n' "$final_csv" > "$done_file"
        echo "RECOVERED complete result: $config_id -> $final_csv"
        return
    fi

    resume_csv="$(find_latest_resume "$config_id")"
    local -a command=(
        python3 -u "$repo_dir/benchmark.py" "$method"
        --time_limit "$time_limit"
        --repetitions "$repetitions"
        --num_concurrent_processes "$concurrency"
        --save_interval_seconds "$save_interval_seconds"
        --binary "$binary_path"
    )
    command+=("$@")
    if [ -n "$resume_csv" ]; then
        command+=(--continue_from "$resume_csv")
        echo "RESUME: $config_id from $resume_csv"
    else
        echo "START: $config_id"
    fi
    print_command "${command[@]}"

    if $dry_run; then
        return
    fi

    set +e
    (
        cd "$repo_dir"
        "${command[@]}"
    ) 2>&1 | tee -a "$log_file"
    local benchmark_status="${PIPESTATUS[0]}"
    set -e

    if [ "$benchmark_status" -ne 0 ]; then
        resume_csv="$(find_latest_resume "$config_id")"
        echo "STOPPED: $config_id (exit $benchmark_status)" >&2
        if [ -n "$resume_csv" ]; then
            echo "Restart this same script to resume from: $resume_csv" >&2
        fi
        exit "$benchmark_status"
    fi

    final_csv="$(find_latest_final "$config_id")"
    if [ -z "$final_csv" ]; then
        echo "ERROR: benchmark succeeded but no final CSV was found for $config_id." >&2
        exit 1
    fi
    validate_final_csv "$final_csv"
    printf '%s\n' "$final_csv" > "$done_file"
    echo "DONE: $config_id -> $final_csv"
}

echo "Release: $expected_tag ($expected_sha)"
echo "Matrix: 36 configurations x $expected_dataset_count instances x $repetitions repetitions = 5724 instance runs"
echo "Timing: concurrency=$concurrency, time_limit=${time_limit}s, CaDiCaL seed=0"
echo "Session: $session_dir"

# 1G and 1L: (none, y) x (without, with symmetry).
run_one '1G-N-0-0' '1G'
run_one '1G-N-S-0' '1G' --use_symmetry_breaking
run_one '1G-I-y-0-0' '1G' --use_incremental_solving --variable_for_incremental y
run_one '1G-I-y-S-0' '1G' --use_incremental_solving --variable_for_incremental y --use_symmetry_breaking
run_one '1L-N-0-0' '1L'
run_one '1L-N-S-0' '1L' --use_symmetry_breaking
run_one '1L-I-y-0-0' '1L' --use_incremental_solving --variable_for_incremental y
run_one '1L-I-y-S-0' '1L' --use_incremental_solving --variable_for_incremental y --use_symmetry_breaking

# 2G and 2L: (none, x, y) x (without, with symmetry).
run_one '2G-N-0-0' '2G'
run_one '2G-N-S-0' '2G' --use_symmetry_breaking
run_one '2G-I-x-0-0' '2G' --use_incremental_solving --variable_for_incremental x
run_one '2G-I-x-S-0' '2G' --use_incremental_solving --variable_for_incremental x --use_symmetry_breaking
run_one '2G-I-y-0-0' '2G' --use_incremental_solving --variable_for_incremental y
run_one '2G-I-y-S-0' '2G' --use_incremental_solving --variable_for_incremental y --use_symmetry_breaking
run_one '2L-N-0-0' '2L'
run_one '2L-N-S-0' '2L' --use_symmetry_breaking
run_one '2L-I-x-0-0' '2L' --use_incremental_solving --variable_for_incremental x
run_one '2L-I-x-S-0' '2L' --use_incremental_solving --variable_for_incremental x --use_symmetry_breaking
run_one '2L-I-y-0-0' '2L' --use_incremental_solving --variable_for_incremental y
run_one '2L-I-y-S-0' '2L' --use_incremental_solving --variable_for_incremental y --use_symmetry_breaking

# X: (fixed, vary) x (none, x) x (without, with symmetry).
run_one 'X-fixed-width-N-0-0' 'X' --width fixed
run_one 'X-fixed-width-N-S-0' 'X' --width fixed --use_symmetry_breaking
run_one 'X-fixed-width-I-x-0-0' 'X' --width fixed --use_incremental_solving --variable_for_incremental x
run_one 'X-fixed-width-I-x-S-0' 'X' --width fixed --use_incremental_solving --variable_for_incremental x --use_symmetry_breaking
run_one 'X-vary-width-N-0-0' 'X' --width vary
run_one 'X-vary-width-N-S-0' 'X' --width vary --use_symmetry_breaking
run_one 'X-vary-width-I-x-0-0' 'X' --width vary --use_incremental_solving --variable_for_incremental x
run_one 'X-vary-width-I-x-S-0' 'X' --width vary --use_incremental_solving --variable_for_incremental x --use_symmetry_breaking

# Xa in the manuscript is the cached auxiliary-variable implementation.
run_one 'Xa(cache)-fixed-width-N-0-0' 'Xa(cache)' --width fixed
run_one 'Xa(cache)-fixed-width-N-S-0' 'Xa(cache)' --width fixed --use_symmetry_breaking
run_one 'Xa(cache)-fixed-width-I-x-0-0' 'Xa(cache)' --width fixed --use_incremental_solving --variable_for_incremental x
run_one 'Xa(cache)-fixed-width-I-x-S-0' 'Xa(cache)' --width fixed --use_incremental_solving --variable_for_incremental x --use_symmetry_breaking
run_one 'Xa(cache)-vary-width-N-0-0' 'Xa(cache)' --width vary
run_one 'Xa(cache)-vary-width-N-S-0' 'Xa(cache)' --width vary --use_symmetry_breaking
run_one 'Xa(cache)-vary-width-I-x-0-0' 'Xa(cache)' --width vary --use_incremental_solving --variable_for_incremental x
run_one 'Xa(cache)-vary-width-I-x-S-0' 'Xa(cache)' --width vary --use_incremental_solving --variable_for_incremental x --use_symmetry_breaking

echo "ALL 36 CONFIGURATIONS COMPLETED AND VALIDATED."
