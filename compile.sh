#!/bin/bash

ACTION=${1:-compile}
BUILD_DIR=build

if [[ "$ACTION" == "clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    exit 0
fi

mkdir -p "$BUILD_DIR"

cmake -S . -B "$BUILD_DIR" -G "Unix Makefiles"
cmake --build "$BUILD_DIR" -j$(nproc)
