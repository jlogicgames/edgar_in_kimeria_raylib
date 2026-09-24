# Edgard in Kimeria — raylib port

This is the C99/raylib/Flecs port of *Edgard in Kimeria*. The work is tracked in
[`docs/migration-plan.md`](docs/migration-plan.md).

## macOS

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

### Release a notarized app

Release builds are real `.app` bundles: assets are packaged in
`Edgard in Kimeria.app/Contents/Resources/assets/`, so launching the app from Finder does
not depend on the working directory. On a release Mac with a Developer ID Application
certificate and a `notarytool` keychain profile, run:

```sh
make release-macos \
  MACOS_SIGN_IDENTITY='Developer ID Application: Your Name (TEAMID)' \
  NOTARY_PROFILE=your-notarytool-profile
```

This builds, signs with the hardened runtime, submits the zip for notarization, staples the
ticket to the app, and verifies Gatekeeper acceptance. The resulting app is at
`build-release-macos/edgard_in_kimeria.app`. Open that app from Finder as the final local
release check.

## Controls

| Action | Keyboard | Gamepad | iOS |
|---|---|---|---|
| Move | A/D or Left/Right | D-pad or left stick | Joystick |
| Jump | J or Z | South | Jump button |
| Attack | K or X | West or East | Attack button |
| Interact | L or C | North | Interact button |
| Pause / confirm menu | Esc / Enter or Space | Start / South or Start | Pause / tap |
| Back from menu | Esc | East or Select | Back button |

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
before play begins. `EIK_CAPTURE_INTERVAL` and `EIK_CAPTURE_SHOTS` set the capture cadence
and count. `EIK_DEBUG_DRAW`, `EIK_INVULNERABLE`, `EIK_CHROMA_GLITCH`,
`EIK_DEBUG_TILEMAP`, and `EIK_SHOW_FPS` enable their matching diagnostics at startup.
F1–F6 toggle debug draw, invulnerability, effects, immediate level advance, checkpoint,
and the FPS overlay respectively. `EIK_ASSET_ROOT` overrides the packaged asset directory.

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

## iOS

The iOS build uses raylib's SDL backend and OpenGL ES 3, bundles `assets/`, supports safe-area
aware touch controls, and uses an ambient audio session that respects the silent switch. The
Simulator deliberately leaves audio disabled because its CoreAudio service can deadlock during
raylib initialization; validate sound on a physical iPhone.

Build it for an Apple-silicon iOS Simulator with:

```sh
cmake -S . -B build-ios-simulator -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ios-simulator --config Debug
```

For a TestFlight upload, create a local App Store export-options plist and configure an App Store
Connect API key in Xcode's standard key location. Then run:

```sh
make ios-testflight \
  IOS_DEVELOPMENT_TEAM=TEAMID \
  IOS_EXPORT_OPTIONS_PLIST=/path/to/ExportOptions.plist \
  APP_STORE_CONNECT_KEY_ID=KEYID \
  APP_STORE_CONNECT_ISSUER_ID=ISSUERID
```

The target creates a signed device archive, exports its IPA, and uploads it to TestFlight.

## Web deployment

`make web` builds and serves the local web version. On a push to `main`, GitHub Actions builds
the Emscripten release and deploys `index.html`, the JavaScript loader, WebAssembly module, and
asset data file to GitHub Pages. Enable Pages with **GitHub Actions** as its source in the
repository settings before the first deployment.

## Deviations from Rust

The port intentionally corrects these Rust behaviours and adds the following features:

- **L3:** honours object-layer offsets; the ported `forest.tmx` removes its stray spawn offset.
- **P9, P10, P13:** game over follows the third death, lives persist between levels, and gravity
  is measured per second rather than per fixed tick.
- **E6, E7, E9:** Red mobs walk back to their spawn after attacking, use Yellow-mob-style body
  contact and stomps, and circles use circle-versus-rectangle collision.
- **O2, O8, O10:** hearts restore one life up to three; vertical escalators carry riders on both
  axes; falling platforms are solid when touched from below or the side.
- **R8, S4:** iOS keeps HUD and controls within the safe area and uses an ambient, lifecycle-aware
  audio session.
- **U12–U15:** Esc/gamepad B backs out of About and Options; main-menu Play resets a run; iOS
  control text is touch-specific; the HUD displays remaining lives.
- **I6–I8:** an FPS overlay, optional Flecs Explorer in macOS development builds, and iOS
  multi-touch controls are new.
