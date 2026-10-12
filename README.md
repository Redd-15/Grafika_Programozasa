# FS Telemetry

Real-time telemetry dashboard for a Formula Student car, built with OpenGL 3.3 and Dear ImGui.

- **Center:** a 3D car you can orbit horizontally by dragging with the left mouse button. The front wheels steer and all four wheels spin from the steering-angle and speed telemetry, and the cockpit steering wheel turns with the input. A grid floor scrolls with vehicle speed.
- **Left:** car state (OFF → STARTUP → TSON → R2D), LV battery (10–15 V), driver inputs (APPS, BPS, steering), temperatures (with warning/critical markers and a 30 s trend), and the accumulator (TS state, Bender, pack V, unbalance).
- **Right:** the dash settings group (power limit, RPM limit, cooling modes, wheel size, regen max) shown read-only, plus speed / RPM / pedal graphs.

## Build

Requires CMake ≥ 3.20 and a C++17 compiler. GLFW, GLM, Dear ImGui and nlohmann/json are downloaded automatically.

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
| `src/data_source.*` | Source choice, CAN-signal → dashboard-field mapping, `data_source.json` load/save |
| `src/udp_source.*` | UDP receiver thread and JSON flattening |
| `src/source_panel.*` | The data-source window |
| `src/display_limits.h` | Alarm thresholds and bar ranges, named like the dash config |
| `src/gl_loader.*` | Minimal OpenGL function loader |

## Data sources

Click the source pill in the top bar (for example `SIMULATION` or `UDP :5005`) to open the data-source window. The choice and the signal mapping are saved to `data_source.json` next to the executable.

- **Simulation:** the built-in simulated car.
- **UDP / JSON:** listens on a bind address and port for datagrams carrying a JSON object (or several, one per line) with the CAN messages and signals decoded by the car. The JSON is flattened into dotted paths:
  - Nested objects become path segments.
  - Array elements are named by their `"name"` (or hex `"id"`).
  - `{"name": ..., "value": ...}` pairs become one signal.

  So `{"messages": [{"name": "BMS_Status", "signals": {"SOC": 54}}]}` gives `messages.BMS_Status.signals.SOC`.

  To bind a value, select it on the left of the window, then click a signal in the live list on the right. You can also type a path, or just the end of one (`SOC`, `BMS_Status.signals.SOC`). Each binding has a scale and offset (`value = raw * scale + offset`). State fields (car state, TS state, Bender, cooling) accept either the state name as text (`"R2D"`) or its index as a number.

Other transports (serial, direct CAN) can be added by implementing `TelemetrySource` and constructing it in `CreateSource()`. `update()` runs on the UI thread, so a blocking reader should run on its own thread, like `UdpSource`.

## Adding the real car model

Put OBJ files (with optional `.mtl` diffuse colours) in `assets/models/`. Any file that is missing falls back to the placeholder.

| File | Convention |
|---|---|
| `body.obj` | Whole car without wheels and steering wheel. Metres; +Y up, +Z forward, +X = car's left, ground at y = 0 |
| `wheel.obj` | One wheel centred at the origin, axle along X, rim face towards +X, modelled at 450 mm diameter. Right-side wheels are mirrored automatically |
| `steering_wheel.obj` | Centred at the origin in the XY plane, facing the driver (−Z), top at +Y |

Positions (wheelbase, track, steering-wheel mount, steering ratio) are set in `CarGeometry` in `src/car_scene.h`.
