# Weather World

Weather World is a **C++17 / OpenGL 3.3 Core** application for interactive weather visualisation. Its procedural 3D scene changes lighting, clouds, fog and precipitation using manual presets or current weather from Open-Meteo. The main graphics technique is directional shadow mapping with a **3×3 percentage-closer filter (PCF)**.

**Team:** Bekarys U, Bibarys K, Yeraly T.  
**Course:** Computer Graphics Fundamentals, Astana IT University, 2026–2027.  
**Repository:** [bekontaii/CGF-final](https://github.com/bekontaii/CGF-final).

## Features

- Orbit camera, procedural meshes, perspective projection and a movable light.
- Clear, cloudy, rain, snow, storm and fog presets, plus day/night controls.
- Dynamic PCF shadows, an on/off comparison and two quality levels.
- Instanced precipitation, moving clouds and lightning illumination.
- Background live-weather requests and an offline demonstration mode.
- A 90-second challenge with six rounds, scoring, a goal and reset.
- On-screen help, FPS, technique status and team identification.
- CMake builds, bundled library sources, tests and defence materials.

## Get the project

With Git installed, open Terminal on macOS/Linux or PowerShell on Windows:

```sh
git clone https://github.com/bekontaii/CGF-final.git
cd CGF-final
```

Alternatively, extract the whole ZIP. The revised submission archive contains `WeatherWorld`; GitHub's Download ZIP normally creates `CGF-final-main`. Open a terminal in that extracted folder.

**Run all commands below from the folder containing `CMakeLists.txt`, `scripts/`, `src/` and `vendor/`.** Do not run them from inside `src/` or from the ZIP file itself. Use the report and video supplied with the revision you intend to submit; a source clone may contain an older documentation package.

## Quick start on an Apple Silicon Mac

The included ARM64 application does not need a compiler or CMake:

1. Extract the complete project.
2. Open `Weather World.app`, or double-click `Start Weather World.command`.
3. Press **F1** for help.

For a reliable offline demonstration, run this from the project folder:

```sh
"./Weather World.app/Contents/MacOS/WeatherWorld" --no-network --weather clear
```

This starts the clear scene with shadows enabled. Keep the application bundle intact. The prebuilt app is for Apple Silicon; build from source on other supported systems. If macOS prevents the downloaded app from opening, use the source-build route below or the standard macOS approval controls after reviewing the source.

## Windows 10 or 11

Install **Visual Studio 2022 Build Tools** with **Desktop development with C++** and the Windows SDK, **CMake 3.21+** on `PATH`, and a graphics driver supporting **OpenGL 3.3 Core**. Reopen PowerShell after installation.

From the project folder:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\run-windows.ps1 --no-network --weather clear
```

The build script configures CMake, compiles Release binaries and runs tests. The run script forwards its arguments to the application. The executable is `build\bin\Release\WeatherWorld.exe`.

Equivalent direct commands:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\bin\Release\WeatherWorld.exe --no-network --weather clear
```

HTTPS uses Windows WinHTTP. No separate curl installation, API key or private server is needed. A laboratory Windows run is still required; the recorded validation results are from a Mac.

## macOS source build

Install Xcode Command Line Tools if missing:

```sh
xcode-select --install
```

Install CMake 3.21+ using its installer or, if Homebrew is already installed:

```sh
brew install cmake
```

From the project folder:

```sh
sh ./scripts/build.sh
sh ./scripts/run.sh --no-network --weather clear
```

The build script creates a Release build and runs tests. The executable is `build/bin/WeatherWorld`. Equivalent direct commands:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/bin/WeatherWorld --no-network --weather clear
```

macOS uses the system libcurl. The application requests a Core OpenGL context and loads GLSL shaders from files.

## Linux source build

On Ubuntu/Debian, install the compiler, CMake, OpenGL/X11 headers and libcurl:

```sh
sudo apt update
sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libcurl4-openssl-dev
```

Confirm that `cmake --version` reports at least 3.21, then run:

```sh
sh ./scripts/build.sh
sh ./scripts/run.sh --no-network --weather clear
```

The executable is `build/bin/WeatherWorld`. This build path uses an X11 desktop session with OpenGL 3.3 support. `--hidden` hides the window but still requires a working graphics context.

## First-run check and controls

The application opens a 1280×720 window with a cottage, trees, a left control panel and **PCF shadows ON**. Press **R**, rotate the light with the arrow keys and press **T** twice to compare shadows. Use **3**, **4** and **5** for rain, snow and storm.

| Input | Action |
|---|---|
| `1` / `2` / `3` / `4` / `5` / `6` | Clear / cloudy / rain / snow / storm / fog |
| `N` | Toggle day/night |
| `Space` | Pause animation and the challenge timer |
| `T` | Toggle PCF shadows |
| `Q` | Toggle high/low quality |
| Left / Right arrow | Rotate the light |
| Mouse drag outside the panel | Orbit the camera |
| Mouse wheel outside the panel | Zoom |
| `R` | Reset the camera |
| `F` | Refresh live weather |
| `F1` or `H` | Open/close help |
| `Esc` | Close help, then exit |

For current provider data, select **Live weather** and a city in the panel. Launching without `--no-network` also starts live mode. Manual weather keys select Simulation. Presets are illustrative and are not presented as live observations. Network failures are displayed, and the offline controls remain available.

## Weather Challenge

Click **Start 90-second challenge**. Each of six 15-second rounds asks for a weather condition and day/night state. Select them and click **Submit weather**. One answer is accepted per round; a correct match earns 100 points, while a wrong answer or timeout earns zero. The goal is **400 out of 600**. Use **Play again** or **Reset** without restarting the process. **Space** pauses the game timer as well as animation.

## Tests and repeatable captures

The build scripts run tests automatically. To rerun them:

```sh
# macOS/Linux
ctest --test-dir build --output-on-failure
```

```powershell
# Windows Release build
ctest --test-dir build -C Release --output-on-failure
```

Examples below use the macOS/Linux executable. On Windows, replace `./build/bin/WeatherWorld` with `.\build\bin\Release\WeatherWorld.exe`.

```sh
./build/bin/WeatherWorld --help
./build/bin/WeatherWorld --no-network --weather storm --night
./build/bin/WeatherWorld --no-network --tour
./build/bin/WeatherWorld --no-network --challenge
./build/bin/WeatherWorld --hidden --no-network --frames 180 --weather snow --screenshot snow.png
./build/bin/WeatherWorld --hidden --no-network --frames 5000 --weather clear
./build/bin/WeatherWorld --hidden --no-network --frames 5000 --weather clear --no-shadows
```

`--screenshot` saves the last rendered frame including the UI. `--low-quality` reduces the shadow-map resolution and particle count. Limited runs print a `BENCHMARK` record. Compare the same scene, camera, resolution and quality; hidden-window throughput is different from visible presentation FPS.

The revised package was checked on **Apple M5, macOS 27.2.1, OpenGL 4.1 Metal driver 91.7**. The clean Release build passed, CTest passed its target, and the test executable reported **145 passing checks**. Visible runs measured approximately **116–120 FPS at 1280×720**. The [validation folder](docs/validation/review-2026-11-17/) contains the method, output and peak resident-memory measurements. These results do not certify the laboratory Windows PC.

## Troubleshooting

| Problem | What to check |
|---|---|
| `cmake` is not found | Install CMake 3.21+ and reopen the terminal; check `cmake --version`. |
| Visual Studio generator is missing | Install the 2022 C++ Build Tools workload and Windows SDK. |
| A run script says “Build first” | Complete the build and confirm the executable exists at the OS-specific path above. |
| OpenGL window creation fails | Check the driver and use a graphical desktop with OpenGL 3.3 support. |
| Shaders or assets cannot be found | Keep `shaders/` and `assets/` beside a source-built executable; CMake copies them automatically. Keep the prebuilt `.app` intact. |
| Live weather fails | Read the error and use Simulation or `--no-network`; no secret API key is required. |
| Low frame rate | Press `Q` and retest the same scene and resolution. |

Launch from a terminal to read shader, framebuffer, graphics-context and resource diagnostics.

## Project structure and materials

```text
include/        Renderer, weather and challenge interfaces
src/            Application/UI, renderer, networking and game logic
shaders/        Vertex and fragment shader source files
assets/         Team configuration and interface font
vendor/         Bundled library sources and licence notices
tests/          Offline weather and challenge checks
scripts/        Build and run helpers
docs/           Report, slides, recording, evidence and submission notes
```

GLFW, GLM, GLAD, Dear ImGui, JSON and stb sources are bundled; CMake does not download them during the build. The system compiler, graphics libraries and networking prerequisites must still be installed.

The revised [report](docs/WeatherWorld_Report.docx) and [PDF](docs/WeatherWorld_Report.pdf) describe the pipeline, actual equations, evaluation and 15 journal studies. The [slides](docs/WeatherWorld_Defence.pptx) and 70-second [backup video](docs/WeatherWorld-demo.mp4) support the defence. Upload the video to YouTube or Google Drive, test link access and insert its URL in the report and `assets/team.json`.

## Repository-recorded contributions

The mapping below is based on the merged history at [`6177f81`](https://github.com/bekontaii/CGF-final/commit/6177f81b59461050cdf3410f6fbbdab99adc8ae3), inspected on 17 November 2026. It records submitted changes and integration, rather than asserting that every line was independently authored without assistance.

| Participant | Module and recorded contribution | Evidence |
|---|---|---|
| Bekarys U (`bekontaii`) | Weather/Open-Meteo integration, challenge logic, weather tests and validation results | [PR #1](https://github.com/bekontaii/CGF-final/pull/1), [PR #3](https://github.com/bekontaii/CGF-final/pull/3) |
| Bibarys K (`BibarysKadirbay`) | Application/UI, build configuration and dependencies, original documentation and demo package | [PR #2](https://github.com/bekontaii/CGF-final/pull/2), [PR #4](https://github.com/bekontaii/CGF-final/pull/4) |
| Yeraly T (`YeralYerte`) | Renderer, scene/shadow/sky/particle shaders and graphics screenshots | [PR #5](https://github.com/bekontaii/CGF-final/pull/5) |

Section **9.2 of the report** provides the file and commit evidence. The later report rewrite and package revisions are disclosed separately and are not attributed to the original repository commits.


## Sources, licences and assistance

Live conditions come from the [Open-Meteo Weather Forecast API](https://open-meteo.com/en/docs). Its data is licensed under CC BY 4.0; the free endpoint is intended for non-commercial use under the [provider's terms](https://open-meteo.com/en/terms). The displayed values are provider model-based current conditions, not readings from a sensor attached to the application.

Project geometry and surface patterns are procedural. No paid model or texture pack is included. Library, font and data notices are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the project is distributed under [MIT](LICENSE).

Substantial code, shader and documentation assistance from OpenAI Codex is disclosed in the report. Preserve that disclosure and all upstream licences. Every teammate must be able to explain and modify the light-space transform, depth comparison, bias and PCF sampling during the defence.
