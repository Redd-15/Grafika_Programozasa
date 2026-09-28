# FS Telemetry

Real-time telemetry dashboard for a Formula Student car, built with OpenGL 3.3 and Dear ImGui.

- **Center:** a 3D car you can orbit horizontally by dragging with the left mouse button. The front wheels steer and all four wheels spin from the steering-angle and speed telemetry, and the cockpit steering wheel turns with the input. A grid floor scrolls with vehicle speed.
- **Left:** car state (OFF → STARTUP → TSON → R2D), LV battery (10–15 V), driver inputs (APPS, BPS, steering), temperatures (with warning/critical markers and a 30 s trend), and the accumulator (TS state, Bender, pack V, unbalance).
- **Right:** the dash settings group (power limit, RPM limit, cooling modes, wheel size, regen max) with the knob-selected row highlighted, plus speed / RPM / pedal graphs.

## Build

Requires CMake ≥ 3.20 and a C++17 compiler. GLFW, GLM and Dear ImGui are downloaded automatically.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/fs_telemetry.exe
```

## Code layout

| File | Purpose |
|---|---|
| `src/telemetry.*` | `TelemetryFrame` data model, 30 s history traces, `TelemetrySource` interface and the simulator |
| `src/car_scene.*` | 3D scene: part transforms, shaders, reflective floor, orbit camera |
| `src/mesh.*` | GPU meshes, procedural geometry builder, OBJ loader |
| `src/ui.*` | Theme, fonts and all dashboard widgets (bars, gauges, charts, tiles) |
| `src/display_limits.h` | Alarm thresholds and bar ranges, named like the dash config |
| `src/gl_loader.*` | Minimal OpenGL function loader |

## Using real data

Implement `TelemetrySource` (for example over CAN, serial or UDP). Fill in `TelemetryFrame` inside `update()`, then pass your source instead of `CreateSimulatedSource()` in `main.cpp`. `update()` runs once per frame on the UI thread, so a blocking reader should run on its own thread and hand over the latest frame.

## Adding the real car model

Put OBJ files (with optional `.mtl` diffuse colours) in `assets/models/`. Any file that is missing falls back to the placeholder.

| File | Convention |
|---|---|
| `body.obj` | Whole car without wheels and steering wheel. Metres; +Y up, +Z forward, +X = car's left, ground at y = 0 |
| `wheel.obj` | One wheel centred at the origin, axle along X, rim face towards +X, modelled at 450 mm diameter. Right-side wheels are mirrored automatically |
| `steering_wheel.obj` | Centred at the origin in the XY plane, facing the driver (−Z), top at +Y |

Positions (wheelbase, track, steering-wheel mount, steering ratio) are set in `CarGeometry` in `src/car_scene.h`.
