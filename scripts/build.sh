#!/usr/bin/env sh
set -eu
PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$PROJECT_ROOT"
if ! command -v cmake >/dev/null 2>&1; then
    echo 'Install CMake 3.21+ and a C++17 compiler. On macOS: xcode-select --install, then brew install cmake.' >&2
    exit 1
fi
cmake --preset unix
cmake --build --preset unix --parallel
ctest --preset unix
printf 'Ready: %s/build/bin/WeatherWorld\n' "$PROJECT_ROOT"
