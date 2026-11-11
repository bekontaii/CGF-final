$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $ProjectRoot
try {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        throw 'CMake 3.21+ is required. Install CMake and Visual Studio 2022 Build Tools with Desktop development with C++ and Windows 10 SDK. Then reopen PowerShell.'
    }
    cmake --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    cmake --build --preset windows --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
    ctest --preset windows
    if ($LASTEXITCODE -ne 0) { throw 'Logic tests failed.' }
    Write-Host 'Ready: build\bin\Release\WeatherWorld.exe'
} finally { Pop-Location }
