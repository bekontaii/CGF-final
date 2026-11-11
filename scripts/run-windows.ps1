$ErrorActionPreference = 'Stop'
$AppPath = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\bin\Release\WeatherWorld.exe'
if (-not (Test-Path $AppPath)) { throw 'Build first: powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1' }
& $AppPath @args
exit $LASTEXITCODE
