# TH095 iOS 0.1.1 validation

Date: 2026-09-25. Product: version 0.1.1, build 2, arm64, minimum iOS 14.0.
The reported device is iPad mini 5 running iOS 14.1. No physical iOS device
was connected to the build Mac. Runtime observations below use Xcode 14.0,
an Intel Mac and an iPad (9th generation) iOS 16.0 simulator. They do not
establish device installation, real-device performance or all-scene completion.

## Reproduced defects and fixes

- **Observed, optimized simulator:** Debug rendered the title while Release
  showed only FPS/controls. Void ANM callbacks had been cast to callbacks with
  a return value. Optimization discarded the inner return value; the scheduler
  interpreted zero as removal and deleted draw layers before asset loading.
  Typed callback adapters restore the scheduler's continue result. Front-end
  callbacks now forward their actual results and preserve the loading barrier.
- **Device log plus arm64 disassembly/source evidence:** the supplied crash
  dereferenced a font object at `0x200000008`. The four-byte Supervisor clear
  color was declared as an eight-byte `unsigned long` reference in another
  translation unit, overwriting the adjacent font pointer. The shared reference
  is now `u32`, checked with its definition in the same translation unit.
- **Device log plus source evidence:** button labels searched fixed font paths
  absent on iOS 14. They now reuse the CoreText font resolver that successfully
  loaded the Japanese game font in the supplied device log.
- **Source review:** HRESULT is explicitly signed 32-bit so failed texture and
  audio operations cannot be mistaken for success on LP64. Result-screen
  accesses now use the actual auxiliary/photo members instead of indexing
  beyond the declared primary VM array.
- **Source review and runtime regression:** replay scanning no longer changes
  the process working directory inside the read-only app bundle. Relative
  replay patterns use the existing Documents fallback.
- **Observed UI regression:** a very short virtual menu-button tap could be
  released before a low-frame-rate input sample. Menu actions now retain a
  150 ms pulse; battle Z/S retain their hold/release semantics.
- The incorrect text replacement from the prior source commit was removed.
  The restored path respects CP932, the destination pixel format, texture
  coordinates and neighbouring labels in an atlas.

## Runtime coverage

All game builds in this section use Release (`-O2`).

| Check | Observed result |
| --- | --- |
| Cold title / Start / selected-scene text | Complete background, menu, character, preview photo and Japanese text; S/Z/X labels visible |
| Replay / Music / Options / Help | Each page opened; scripted traversal returned from all four and entered battle in the same process |
| First scene capture | Normal movement and Z/S input reached 3/3 photographs and the success result, without modifying health, score, charge or unlocks |
| Result transitions | Retry returned to battle and completed another 3/3; Next Scene resumed gameplay; Return to Select restored the scene screen and preview |
| Pause / resume | Captured frozen frame and restored normal redraw in the same session |
| Joystick | Direction, motion and release checks passed; relative movement and photo-button events were exercised |
| Actual UI taps, ordinary Release | With regression input disabled, XCTest tapped Start and Z and reached rendered battle; Replay, X, Start, Z also reached battle |
| Actual rotation | XCTest changed landscape/portrait/landscape-right; oriented screenshots and renderer logs showed 1080x810 / 810x1080 / 1080x810 |
| Portrait touch settings | Actual taps opened Start, Z, pause and settings; Z/S toggles were exercised twice and restored; Done returned to the paused game |

Screenshot attachments were inspected in addition to test assertions. The UI
runner checks foreground state and screenshot dimensions; it cannot infer
correct gameplay solely from a passing assertion. On SDL, accessibility
`app.frame` may retain portrait dimensions, so screenshots and drawable logs
are used to assess actual orientation. Waiting idle in battle can legitimately
reach the failure menu; that is not evidence of an app crash.

The simulator input driver is opt-in and rejected by CMake for device builds.
The arm64 Release executable was checked to contain no capture/menu regression
driver strings. Original game data and all private logs/screenshots remain
outside the public source repository.

## Packaging and limitations

Both device and simulator Release builds completed. The arm64 Mach-O reports
platform IOS, minimum 14.0, SDK 16.0. The app includes the selected icon and the
two original data archives; their SHA-256 values match the supplied originals:

```text
th095.dat e3ba9804faf7d3f09399461a7af702bb129e83ba525ec0bfccbe1a9d3e07280c
thbgm.dat 6f482af14a46444d3fbfffa4a06960e3e6c9e5879a7485bc8387d6ad6f2c5c2e
```

The resulting IPA is a device retest build, not a claim that all original
scenes, replay determinism, long sessions and iOS 14.1 device behavior have
been exhaustively verified. Simulator FPS is not a measurement of iPad mini 5
performance. Repeat startup, submenu navigation, portrait/landscape battle and
several captures on that device; `Documents/startup.log` now identifies build
0.1.1 (2). Files/File Sharing exposes those diagnostics for comparison.

The private `TH095-iOS14-v0.1.1.ipa` was ad-hoc signed and passed strict
codesign verification and ZIP integrity checks. Copies on the Mac and Windows
desktops are 141,049,877 bytes and share SHA-256
`3a37d35b81df06148ec3015a3bbd7521f1046ffd0b3e5ac8c49ef715efa99f01`.
The IPA was inspected for arm64/IOS platform, minimum version, bundle version,
icon assets, matching archive hashes and absence of regression-driver strings
or private diagnostic/credential files. These checks do not substitute for
TrollStore installation and launch on the reported iOS 14.1 device.
