# TH095 iOS port

This is the separate iOS 14+ port of the Japanese TH095 1.02a reconstruction.
The full runtime uses SDL2, an OpenGL ES 2 implementation of the required D3D8
operations, CP932 text conversion, and the game's original scripts and data.
Portable builds do not establish byte-exact reconstruction or complete
all-scene equivalence. See VALIDATION.md for the observed test coverage.

## Controls

- Left joystick: eight-direction movement. Dragging the playfield also moves
  the player relative to the finger, with configurable sensitivity.
- Z: photograph / confirm. S: focus / slow movement. X: cancel.
- The pause button opens the original pause menu. The `...` button opens
  persistent settings for Z toggle, S toggle, portrait battle and touch speed.
- Supported menu labels can be tapped directly. The joystick and Z/X remain
  available for the original menus.
- Portrait battle enlarges the central playfield while reserving space below
  for controls; landscape preserves the original 4:3 frame. iPad is supported.

## Build

Use macOS with Xcode 14 or newer and CMake. Dependencies are pinned in
CMakeLists.txt. Keep the iOS deployment target at 14.0. Place your own Japanese
th095.dat and thbgm.dat under ios/resources/; they are required for the
private playable package and must never be committed to this repository.

```sh
cmake -S ios -B build-ios-device -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DTH095_FULL_PORT=ON -DTH095_IOS_REGRESSION_DRIVER=OFF \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build-ios-device --config Release -j2
```

The resulting device .app is packaged inside Payload/ for TrollStore.
An iOS simulator requires a separately compiled simulator .app, not a device
IPA. For an Intel Mac, configure iphonesimulator with x86_64 architecture.
Simulator regression input can be enabled with
-DTH095_IOS_REGRESSION_DRIVER=ON; CMake rejects that option for device builds.
Always test Release, since optimization exposed a callback ABI defect that
Debug testing did not catch.

## Diagnostics and user data

Startup/render diagnostics and crash reports are written to the application's
Documents directory, exposed through Files/File Sharing. Scores, configuration,
replays and photographs are also private user data. Preserve them when updating.
Logs can contain device paths and should not be committed. Do not distribute
original resources, signing credentials, provisioning files, or build products
through this source repository.

Version 0.1.1 fixes the missing Release draw jobs, a 64-bit overwrite of a font
pointer, system-font discovery for touch labels, result VM array indexing,
32-bit HRESULT error handling and replay browsing that changed the resource
working directory. It restores the CP932-aware text renderer in place of the
incorrect whole-atlas RGBA replacement.
