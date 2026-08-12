#!/bin/bash

set -eu

script_dir="$(cd "$(dirname "$0")" && pwd)"
cd "$script_dir"

cadical_lib="${CADICAL_LIB:-external/cadical/libcadical.a}"
cadical_include_dir="${CADICAL_INCLUDE_DIR:-external/cadical/include}"
build_dir="${BCP_BUILD_DIR:-build}"

case "$cadical_lib" in
    /*) ;;
    *) cadical_lib="$script_dir/$cadical_lib" ;;
esac
case "$cadical_include_dir" in
    /*) ;;
    *) cadical_include_dir="$script_dir/$cadical_include_dir" ;;
esac

if [ ! -f "$cadical_lib" ]; then
    echo "========================================================"
    echo " [ERROR] CaDiCaL library not found!"
    echo " Expected path: ./$cadical_lib"
    echo "========================================================"
    echo " Please compile CaDiCaL manually before running this script."
    echo "========================================================"
    exit 1
fi

echo "Found CaDiCaL library. Proceeding..."

echo "Configuring project..."
cmake -S "$script_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCADICAL_LIB="$cadical_lib" \
    -DCADICAL_INCLUDE_DIR="$cadical_include_dir"

echo "Building project..."
cmake --build "$build_dir" --parallel

if [ ! -f "$build_dir/bcp" ]; then
    echo "Error: Could not locate the built executable '$build_dir/bcp'."
    exit 1
fi
cp "$build_dir/bcp" "$script_dir/bcp"

echo "========================================================"
echo " Build Finished Successfully!"
echo " The executable '$script_dir/bcp' is now in the repository root."
echo "========================================================"
