# Edgard in Kimeria — raylib port

This is the C99/raylib/Flecs port of *Edgard in Kimeria*. The work is tracked in
[`docs/migration-plan.md`](docs/migration-plan.md).

## Run on macOS

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/edgard_in_kimeria
```

The build copies `assets/` beside the executable. To load a different asset directory,
set `EIK_ASSET_ROOT` to that directory (for example, `EIK_ASSET_ROOT=/path/to/assets`).

## Build for the web

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web
```

Serve `build-web/` with a local HTTP server. The web build preloads `assets/` and uses a
viewport-filling canvas.
