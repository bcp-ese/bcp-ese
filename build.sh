#!/bin/bash

set -e

cd "$(dirname "$0")"

# --- Configuration ---
CADICAL_LIB="external/cadical/libcadical.a"
CADICAL_HEADER="external/cadical/include/cadical.hpp"
KISSAT_EXEC="bin/kissat"
BUILD_DIR="build"
EXEC_NAME="bcp"

# --- Dependency Check ---
MISSING_DEPS=0

echo "Checking dependencies..."

if [ ! -f "$CADICAL_LIB" ]; then
    echo " [ERROR] Missing: $CADICAL_LIB"
    MISSING_DEPS=1
fi

if [ ! -f "$CADICAL_HEADER" ]; then
    echo " [ERROR] Missing: $CADICAL_HEADER"
    MISSING_DEPS=1
fi

if [ ! -f "$KISSAT_EXEC" ]; then
    echo " [ERROR] Missing: $KISSAT_EXEC"
    MISSING_DEPS=1
fi

if [ $MISSING_DEPS -eq 1 ]; then
    echo "========================================================"
    echo " CRITICAL: Missing required dependencies!"
    echo "========================================================"
    echo " Please manually add the missing files listed above"
    echo " to the correct directories before running this script."
    echo "========================================================"
    exit 1
fi

echo "All dependencies found. Proceeding..."

# --- Build Process ---
if [ -d "$BUILD_DIR" ]; then
    echo "Cleaning previous build..."
    rm -rf "$BUILD_DIR"
fi

mkdir "$BUILD_DIR"
cd "$BUILD_DIR"

echo "Configuring project..."
cmake -DCMAKE_BUILD_TYPE=Release ..

echo "Building project..."
cmake --build . --parallel

# --- Move Executable ---
if [ -f "$EXEC_NAME" ]; then
    mv "$EXEC_NAME" ../
elif [ -f "Release/$EXEC_NAME" ]; then
    mv "Release/$EXEC_NAME" ../
else
    echo "Error: Could not locate the built executable '$EXEC_NAME'."
    exit 1
fi

echo "========================================================"
echo " Build Finished Successfully!"
echo " The executable '$EXEC_NAME' is now in the current directory."
echo "========================================================"