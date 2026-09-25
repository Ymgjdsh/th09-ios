<p align="center">
  <img src="ios/app/AppIcon1024.png" alt="TH095 iOS icon" width="180">
</p>

# TH095 iOS Port

An iOS port of the original Japanese **東方文花帖 ～ Shoot the Bullet 1.02a** by Team Shanghai Alice, based on the [TH095 reconstruction project](https://github.com/N0zoM1z0/th095).

The port runs the reconstructed game runtime with SDL2, OpenGL ES 2 rendering, touch input, and the original game archives. It targets iOS 14.0 or later on arm64 iPhone and iPad. The source repository does not include the original game data or signing material.

This is an active port. The current build has passed the [documented simulator checks](ios/VALIDATION.md), including title and menu navigation, first-scene photography and results, touch controls, and portrait/landscape rotation. Complete scene coverage, long sessions, and installation on every supported iOS version have not been established.

## Building

### Dependencies

- macOS with Xcode 14 or later and CMake
- An arm64 iOS device, or an iOS simulator for simulator builds
- The Japanese original `th095.dat` and `thbgm.dat` from your own game copy

SDL2, SDL2_image, SDL2_mixer, and SDL2_ttf are fetched at their pinned versions by CMake. The game uses an OpenGL ES 2 compatibility renderer for its D3D8 drawing calls.

### iOS

Place `th095.dat` and `thbgm.dat` in `ios/resources/`, then build the full game:

```sh
cmake -S ios -B build-ios-device -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 \
  -DTH095_FULL_PORT=ON -DTH095_IOS_REGRESSION_DRIVER=OFF \
  -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO
cmake --build build-ios-device --config Release -j2
```

The resulting `.app` can be packaged under `Payload/` as an IPA and signed for your installation method. A simulator requires a separate `iphonesimulator` build; a device IPA cannot be launched there. See [iOS build and data details](ios/README.md).

## Controls

On menus, tap a supported option directly, use the on-screen joystick to move the cursor, press **Z** to confirm, or **X** to go back.

During play, move with the left joystick or drag the playfield for relative movement. Hold **S** to focus and slow down, use **Z** for the camera, and **X** for the game's secondary action. The pause button opens the original pause menu.

The **...** button opens touch settings for Z and S toggle modes, touch speed, and the portrait battle view. Landscape shows the original full frame; portrait enlarges the central playfield and reserves room for controls. Settings are saved between launches.

## Current Limitations

- Only the original Japanese game is supported by this build. There is no language switch.
- The reported validation covers selected menus and gameplay paths; it does not prove that every scene, replay, or long play session behaves like the Windows original.
- Performance on physical devices cannot be inferred from simulator FPS.

Version 0.1.1 fixes optimized-build title black screens, a font-pointer overwrite, missing button labels, replay directory handling, and short menu taps. [Validation notes](ios/VALIDATION.md) describe what was observed and what remains unverified. Startup and crash diagnostics are available in the app's `Documents` directory through Files/File Sharing.

## Todo

- Verify the current build on physical iOS 14 devices and across additional aspect ratios.
- Exercise the remaining scenes, replay saving/playback, and longer gameplay sessions.
- Compare text, graphics, sound, and touch response against the original game in those paths.

## Credits

- [N0zoM1z0/th095](https://github.com/N0zoM1z0/th095) for the reconstruction used as the source base.
- [Team Shanghai Alice](https://www16.big.or.jp/~zun/) for the original game.
