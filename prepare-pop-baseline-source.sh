#!/usr/bin/env bash

set -Eeuo pipefail

source_url="https://github.com/s6dafabe/popsatgcpbcp.git"
source_sha="8f19dbff4135e6cff9e4b147ebe8462603d5fe03"
script_dir="$(cd "$(dirname "$0")" && pwd)"
target_dir="${1:-$script_dir/external/popsatgcpbcp/source}"
venv_dir="${2:-$(dirname "$target_dir")/.venv}"

select_bootstrap_python() {
    local candidate
    local -a candidates
    if [ -n "${BCP_BOOTSTRAP_PYTHON:-}" ]; then
        candidates=("$BCP_BOOTSTRAP_PYTHON")
    else
        candidates=(python3.12 python3.11 python3.10 python3)
    fi
    for candidate in "${candidates[@]}"; do
        if command -v "$candidate" >/dev/null 2>&1 && \
            "$candidate" -c 'import sys; raise SystemExit(sys.version_info < (3, 10))'; then
            command -v "$candidate"
            return
        fi
    done
    echo "ERROR: Python >= 3.10 is required for the original POP source." >&2
    exit 1
}

if [ -e "$target_dir" ] && [ ! -d "$target_dir/.git" ]; then
    echo "ERROR: target exists but is not a Git checkout: $target_dir" >&2
    exit 1
fi

if [ ! -d "$target_dir/.git" ]; then
    mkdir -p "$(dirname "$target_dir")"
    git clone "$source_url" "$target_dir"
fi

git -C "$target_dir" fetch --quiet origin "$source_sha"
git -C "$target_dir" checkout --quiet --detach "$source_sha"

actual_sha="$(git -C "$target_dir" rev-parse HEAD)"
if [ "$actual_sha" != "$source_sha" ]; then
    echo "ERROR: expected upstream SHA $source_sha, found $actual_sha" >&2
    exit 1
fi

dirty="$(git -C "$target_dir" status --porcelain --untracked-files=all)"
if [ -n "$dirty" ]; then
    echo "ERROR: upstream baseline checkout is dirty:" >&2
    echo "$dirty" >&2
    exit 1
fi

bootstrap_python="$(select_bootstrap_python)"
if [ ! -x "$venv_dir/bin/python" ]; then
    "$bootstrap_python" -m venv "$venv_dir"
fi
if ! "$venv_dir/bin/python" -c 'import sys; raise SystemExit(sys.version_info < (3, 10))'; then
    echo "ERROR: existing environment uses Python < 3.10: $venv_dir" >&2
    echo "Choose a new VENV_DIR as the second argument." >&2
    exit 1
fi
"$venv_dir/bin/python" -m pip install --requirement "$script_dir/requirements.txt"

echo "Prepared POP-S-B/POPH-S-B source at $target_dir"
echo "Source: $source_url"
echo "Commit: $source_sha"
echo "Python environment: $venv_dir"
