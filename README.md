# FS Telemetry

Real-time telemetry dashboard for a Formula Student car, built with OpenGL 3.3 and Dear ImGui.

- **Center:** a 3D car you can orbit horizontally by dragging with the left mouse button. The front wheels steer and all four wheels spin from the steering-angle and speed telemetry, and the cockpit steering wheel turns with the input. A grid floor scrolls with vehicle speed.
- **Left:** car state (STARTUP → STANDBY → TS ON → R2D, plus ERROR), LV battery (10–15 V), driver inputs (APPS, BPS, steering), temperatures (with warning/critical markers and a 30 s trend), and the accumulator (TS state, Bender, pack V, unbalance).
- **Right:** the dash settings group (power limit, RPM limit, motor / inverter pump overrides, wheel size, max recuperation %, frequency profile) shown read-only, plus speed / RPM / pedal graphs.

## Build

Requires CMake ≥ 3.20 and a C++17 compiler. GLFW, GLM, Dear ImGui, nlohmann/json, cgltf and stb_image are downloaded automatically.

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
| `src/car_model.*` | `car.glb` loader: moving parts, materials, textures |
| `src/mesh.*` | GPU meshes, procedural geometry builder, OBJ loader |
| `src/ui.*` | Theme, fonts and all dashboard widgets (bars, gauges, charts, tiles) |
| `src/data_source.*` | Source choice, CAN-signal → dashboard-field mapping, `data_source.json` load/save |
| `src/udp_source.*` | UDP receiver thread and JSON flattening |
| `src/source_panel.*` | The data-source window |
| `src/display_limits.h` | Alarm thresholds and bar ranges, named like the dash config |
| `src/gl_loader.*` | Minimal OpenGL function loader |

## Data sources

Click the source pill in the top bar (for example `SIMULATION` or `UDP :5005`) to open the data-source window. The choice and the signal mapping are saved to `data_source.json` next to the executable. On first start the mapping comes from `assets/data_source.default.json`, which binds every dashboard value to its signal in the car's DBC (`src/ERMe01_v2_4.dbc`). The car-state, TS-state and Bender enums use the DBC value numbers, so raw CAN values map 1:1.

- **Simulation:** the built-in simulated car.
- **UDP / JSON:** listens on a bind address and port for datagrams carrying a JSON object (or several, one per line) with the CAN messages and signals decoded by the car. The JSON is flattened into dotted paths:
  - Nested objects become path segments.
  - Array elements are named by their `"name"` (or hex `"id"`).
  - `{"name": ..., "value": ...}` pairs become one signal.

  So `{"messages": [{"name": "BMS_Status", "signals": {"SOC": 54}}]}` gives `messages.BMS_Status.signals.SOC`.

  To bind a value, select it on the left of the window, then click a signal in the live list on the right. You can also type a path, or just the end of one (`SOC`, `BMS_Status.signals.SOC`). Each binding has a scale and offset (`value = raw * scale + offset`). State fields (car state, TS state, Bender, cooling) accept either the state name as text (`"R2D"`) or its index as a number.

Other transports (serial, direct CAN) can be added by implementing `TelemetrySource` and constructing it in `CreateSource()`. `update()` runs on the UI thread, so a blocking reader should run on its own thread, like `UdpSource`.

## Adding the real car model

Export the car from Blender as a single glTF binary, `assets/models/car.glb`. It replaces the placeholder and keeps its texture maps. If the file can't be used, the app falls back to the placeholder and shows the reason in amber in the 3D view.

**Orientation:** nose towards −Y (facing you in Blender's Front view), +Z up, car's left at +X, in metres. The app stands the car on the ground and centres it, so its exact position doesn't matter.

**Moving parts** are found by object name. A part can be one object or an Empty with objects below it. Everything else is static body, including the suspension.

| Object name | Contents | Motion |
|---|---|---|
| `wheel_FL`, `wheel_FR`, `wheel_RL`, `wheel_RR` | Tyre, rim, brake disc. Axle along X, no camber or toe, or they wobble | Spin, and the front pair steers. Pivots on the centre of the wheel's bounds, so its origin doesn't matter |
| `upright_FL`, `upright_FR` (optional) | Parts that steer but don't spin | Steers around its wheel |
| `steering_wheel` | Steering wheel. **Put its origin on the column axis** | Turns around its origin. Set the column angle in `steeringWheelTiltDeg` in `src/car_scene.h` |

When applying transforms, use Ctrl+A > Rotation & Scale rather than All Transforms, which moves the origin. The wheel size setting only changes spin speed, not the size of the model.

**Materials:** use a Principled BSDF with Image Texture nodes wired into it. Base colour, metallic, roughness, normal, emissive and alpha maps are used, along with a second UV map and Mapping node offset, rotation and scale. For ambient occlusion, add a node group named `glTF Material Output` with an `Occlusion` input.

Maps built from nodes (Noise, ColorRamp, Mix) are **dropped by the exporter with no warning**, so bake them to images first. Clearcoat, transmission and other glTF material extensions are ignored.

**Export** with File > Export > glTF 2.0:

| Setting | Value |
|---|---|
| Format | glTF Binary (.glb) |
| Include > Limit to | Visible Objects |
| Transform > +Y Up | On |
| Mesh > Apply Modifiers | On |
| Mesh > UVs, Normals | On |
| Mesh > Tangents | On if you use normal maps |
| Material > Images | Automatic |
| Compression | Off |
| Animation | Off |

Blender can't compute tangents for meshes with n-gons, so it logs `Could not calculate tangents` and exports them without any. The app then estimates tangents per pixel, which is lower quality, so add a Triangulate modifier to those meshes if they use normal maps.

The car is drawn twice per frame because of the floor reflection, so decimate CAD-derived meshes. As a rule of thumb, start around 500k triangles and 2K textures. GitHub rejects files over 100 MB, so a bigger `car.glb` needs Git LFS or should stay out of the repository.

### Without textures

If there's no `car.glb`, OBJ files (with optional `.mtl` diffuse colours) in `assets/models/` are used instead. Any file that is missing falls back to the placeholder.

| File | Convention |
|---|---|
| `body.obj` | Whole car without wheels and steering wheel. Metres; +Y up, +Z forward, +X = car's left, ground at y = 0 |
| `wheel.obj` | One wheel centred at the origin, axle along X, rim face towards +X, modelled at 450 mm diameter. Right-side wheels are mirrored automatically |
| `steering_wheel.obj` | Centred at the origin in the XY plane, facing the driver (−Z), top at +Y |

Positions (wheelbase, track, steering-wheel mount, steering ratio) are set in `CarGeometry` in `src/car_scene.h`.
