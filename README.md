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
