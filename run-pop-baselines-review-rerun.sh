#!/usr/bin/env bash

# Run POP-S-B and POPH-S-B from the pinned original source with the same
# encoding-plus-solving timing definition used by the manuscript.

set -Eeuo pipefail
export PYTHONDONTWRITEBYTECODE=1

expected_tag="${BCP_RELEASE_TAG:-review-rerun-rc3}"
expected_upstream_sha="8f19dbff4135e6cff9e4b147ebe8462603d5fe03"
expected_dataset_count=53
repetitions=3
time_limit=3600
solver_seed=0

select_python() {
    local candidate
    if [ -n "${BCP_PYTHON:-}" ]; then
        candidate="$BCP_PYTHON"
    else
        candidate="$(dirname "$upstream_dir")/.venv/bin/python"
    fi
    if [ ! -x "$candidate" ]; then
        echo "ERROR: Python environment not found: $candidate" >&2
        echo "Run prepare-pop-baseline-source.sh first, or set BCP_PYTHON." >&2
        exit 1
    fi
    "$candidate" - <<'PY'
import sys
if sys.version_info < (3, 10):
    raise SystemExit('POP baseline source requires Python >= 3.10')
import networkx
import numpy
if networkx.__version__ != '2.8.5' or numpy.__version__ != '2.0.2':
    raise SystemExit(
        f'Expected networkx 2.8.5 and numpy 2.0.2; found '
        f'{networkx.__version__} and {numpy.__version__}'
    )
PY
    printf '%s' "$candidate"
}

usage() {
    echo "Usage: $0 REPO_AT_RELEASE_TAG POPSAT_SOURCE CADICAL_BINARY SESSION_DIR [--dry-run]" >&2
}

if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    usage
    exit 2
fi

repo_dir="$(cd "$1" && pwd)"
upstream_dir="$(cd "$2" && pwd)"
solver_path="$(cd "$(dirname "$3")" && pwd)/$(basename "$3")"
mkdir -p "$4"
session_dir="$(cd "$4" && pwd)"
dry_run=false
if [ "${5:-}" = "--dry-run" ]; then
    dry_run=true
elif [ "$#" -eq 5 ]; then
    usage
    exit 2
fi

if [ "$(git -C "$repo_dir" cat-file -t "refs/tags/$expected_tag" 2>/dev/null || true)" != "tag" ]; then
    echo "ERROR: $expected_tag must be an annotated tag in $repo_dir." >&2
    exit 1
fi
expected_sha="$(git -C "$repo_dir" rev-parse "refs/tags/$expected_tag^{}")"
if [ "$(git -C "$repo_dir" rev-parse HEAD)" != "$expected_sha" ]; then
    echo "ERROR: use a detached clean worktree at refs/tags/$expected_tag." >&2
    exit 1
fi
if [ -n "$(git -C "$repo_dir" status --porcelain --untracked-files=all -- . \
    ':(exclude)result/**' ':(exclude)external/popsatgcpbcp/source/**')" ]; then
    echo "ERROR: release worktree is dirty." >&2
    exit 1
fi

if [ "$(git -C "$upstream_dir" rev-parse HEAD)" != "$expected_upstream_sha" ]; then
    echo "ERROR: expected POP baseline source $expected_upstream_sha." >&2
    exit 1
fi
if [ -n "$(git -C "$upstream_dir" status --porcelain --untracked-files=all)" ]; then
    echo "ERROR: POP baseline source is dirty." >&2
    exit 1
fi
python_bin="$(select_python)"
if [ ! -x "$solver_path" ] || [ "$($solver_path --version)" != "1.9.5" ]; then
    echo "ERROR: CADICAL_BINARY must be an executable CaDiCaL 1.9.5 binary." >&2
    exit 1
fi

dataset_count="$(find "$repo_dir/dataset" -maxdepth 1 -type f -name '*.col' | wc -l | tr -d ' ')"
if [ "$dataset_count" -ne "$expected_dataset_count" ]; then
    echo "ERROR: expected $expected_dataset_count .col files, found $dataset_count." >&2
    exit 1
fi

mkdir -p "$session_dir/results" "$session_dir/logs"

validate_result() {
    local csv_path="$1"
    local method="$2"
    "$python_bin" - "$csv_path" "$method" "$expected_sha" "$expected_upstream_sha" \
        "$expected_dataset_count" "$repetitions" "$time_limit" "$solver_seed" \
        "$solver_path" "$repo_dir/dataset" <<'PY'
import collections
import csv
import hashlib
import math
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
method, source_sha, upstream_sha = sys.argv[2:5]
instances, repetitions = map(int, sys.argv[5:7])
time_limit, seed = sys.argv[7:9]
solver_path = pathlib.Path(sys.argv[9])
dataset_dir = pathlib.Path(sys.argv[10])
with path.open(newline='', encoding='utf-8') as handle:
    reader = csv.DictReader(handle)
    rows = list(reader)
    columns = set(reader.fieldnames or [])
required_columns = {
    'name', 'cnf_calls', 'solver_calls', 'status', 'span', 'encoding_time',
    'total_solving_time', 'time_used', 'timed_out', 'optimality_proven',
    'run_id', 'source_sha', 'source_dirty', 'upstream_source_sha',
    'upstream_source_dirty', 'input_sha256', 'binary_sha256', 'solver',
    'solver_seed', 'method', 'time_limit', 'search_strategy', 'concurrency',
}
if missing := required_columns - columns:
    raise SystemExit(f'{path}: missing columns {sorted(missing)}')
if len(rows) != instances * repetitions:
    raise SystemExit(f'{path}: expected {instances * repetitions} rows, found {len(rows)}')
expected = {
    'method': {method}, 'source_sha': {source_sha}, 'source_dirty': {'False'},
    'upstream_source_sha': {upstream_sha}, 'upstream_source_dirty': {'False'},
    'solver': {'CaDiCaL 1.9.5'}, 'solver_seed': {seed},
    'time_limit': {str(float(time_limit))}, 'search_strategy': {'descending-linear'},
    'concurrency': {'1'}, 'run_id': {str(i) for i in range(1, repetitions + 1)},
}
for column, wanted in expected.items():
    found = {row[column].strip() for row in rows}
    if found != wanted:
        raise SystemExit(f'{path}: {column}: expected {wanted}, found {found}')
def sha256(file_path):
    digest = hashlib.sha256()
    with file_path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()

solver_hash = sha256(solver_path)
for number, row in enumerate(rows, start=2):
    encoding = float(row['encoding_time'])
    solving = float(row['total_solving_time'])
    total = float(row['time_used'])
    if not all(math.isfinite(value) and value >= 0 for value in (encoding, solving, total)):
        raise SystemExit(f'{path}:{number}: invalid timing value')
    if not math.isclose(total, encoding + solving, rel_tol=1e-9, abs_tol=1e-9):
        raise SystemExit(f'{path}:{number}: time_used != encoding_time + total_solving_time')
    cnf_calls, solver_calls = int(row['cnf_calls']), int(row['solver_calls'])
    if solver_calls < 0 or cnf_calls < solver_calls or cnf_calls - solver_calls > 1:
        raise SystemExit(f'{path}:{number}: inconsistent CNF/SAT call counts')
    if row['binary_sha256'] != solver_hash:
        raise SystemExit(f'{path}:{number}: CaDiCaL binary hash mismatch')
    input_path = dataset_dir / row['name']
    if not input_path.is_file() or row['input_sha256'] != sha256(input_path):
        raise SystemExit(f'{path}:{number}: input hash mismatch')
counts = collections.Counter(row['name'] for row in rows)
if len(counts) != instances or set(counts.values()) != {repetitions}:
    raise SystemExit(f'{path}: every input must occur exactly {repetitions} times')
PY
}

run_method() {
    local method="$1"
    local output="$session_dir/results/${method}.csv"
    local log="$session_dir/logs/${method}.log"
    local -a command=(
        "$python_bin" -u "$repo_dir/benchmark_pop_baselines.py" "$method"
        --source_dir "$upstream_dir"
        --dataset_dir "$repo_dir/dataset"
        --solver "$solver_path"
        --output "$output"
        --time_limit "$time_limit"
        --repetitions "$repetitions"
        --solver_seed "$solver_seed"
    )
    printf '%q ' "${command[@]}"
    printf '\n'
    if $dry_run; then
        return
    fi
    "${command[@]}" 2>&1 | tee -a "$log"
    validate_result "$output" "$method"
}

echo "Release: $expected_tag ($expected_sha)"
echo "Original POP source: $expected_upstream_sha"
echo "Python: $($python_bin --version 2>&1) ($python_bin)"
echo "Matrix: 2 baselines x $expected_dataset_count inputs x $repetitions repetitions = 318 runs"
echo "Search: descending linear; timing: encoding + solving; seed: $solver_seed"

run_method POP-S-B
run_method POPH-S-B

echo "BOTH POP BASELINES COMPLETED AND VALIDATED."
