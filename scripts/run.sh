#!/usr/bin/env sh
set -eu
PROJECT_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ ! -x "$PROJECT_ROOT/build/bin/WeatherWorld" ]; then
    echo 'Build first with ./scripts/build.sh' >&2
    exit 1
fi
exec "$PROJECT_ROOT/build/bin/WeatherWorld" "$@"
