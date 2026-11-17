#!/bin/zsh
cd -- "${0:A:h}"
exec "./Weather World.app/Contents/MacOS/WeatherWorld" "$@"
