# Edgard in Kimeria — raylib port

This is the C99/raylib/Flecs port of *Edgard in Kimeria*. The work is tracked in
[`docs/migration-plan.md`](docs/migration-plan.md).

## Run on macOS

Short command:

```sh
make macos
```

Equivalent commands without Make:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/edgard_in_kimeria
```

The build copies `assets/` beside the executable. To load a different asset directory,
set `EIK_ASSET_ROOT` to that directory (for example, `EIK_ASSET_ROOT=/path/to/assets`).

## Development captures

On macOS, a capture run writes numbered screenshots and exits by itself. The destination
directory must already exist:

```sh
mkdir -p /tmp/edgard-captures
EIK_CAPTURE=/tmp/edgard-captures EIK_CAPTURE_INPUT=run \
  EIK_CAPTURE_SHOTS=8 EIK_CAPTURE_INTERVAL=0.5 ./build/edgard_in_kimeria
```

`EIK_CAPTURE_INPUT` accepts `run`, `left`, `fx`, `cycle`, `checkpoint`, and `pause`.
`EIK_CAPTURE_LEVEL=1` starts at `forest`; `EIK_CAPTURE_DELAY` leaves the main menu visible
before play begins. `EIK_DEBUG_DRAW`, `EIK_INVULNERABLE`, `EIK_CHROMA_GLITCH`,
`EIK_DEBUG_TILEMAP`, and `EIK_SHOW_FPS` enable their matching diagnostics at startup.
F1–F6 toggle debug draw, invulnerability, effects, immediate level advance, checkpoint,
and the FPS overlay respectively.

For Flecs Explorer, configure a macOS development build with `-DEIK_DEV=ON`. It listens
only on `127.0.0.1:27750`; open [Flecs Explorer](https://flecs.dev/explorer/) and connect
to that address. `EIK_CAPTURE_ABOUT=1` is available only in that development build.

## Build for the web

Short command:

```sh
make web
```

Equivalent commands without Make:

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
python3 -m http.server --directory build-web 8000
```

The web command serves the build at [localhost:8000](http://localhost:8000). The web build
preloads `assets/` and uses a viewport-filling canvas. Use `make build-macos` or `make build-web`
when you only need the build artifact.

## iOS spike

The iOS spike uses raylib's SDL backend and OpenGL ES 3. It bundles `assets/` in the app and
renders the Phase 0 scene with a shader, a sound (tap or press Space), touch feedback, and the
first gamepad's left-stick value. The current iOS Simulator CoreAudio service deadlocks during
raylib audio initialization, so simulator builds leave audio disabled; the sound check is run on
the physical iPhone acceptance target.

Build it for an Apple-silicon iOS Simulator with:

```sh
cmake -S . -B build-ios-simulator -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ios-simulator --config Debug
```

For a physical device, generate the project with `-DCMAKE_OSX_SYSROOT=iphoneos`, open the Xcode
project, select a signed development team, and run it on an iPhone. The Phase 0b exit check is a
60 fps run on both the simulator and device, with a touch and paired gamepad visibly updating the
spike input readout.
