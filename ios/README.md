# TH095 iOS port lane

This directory is a separate iOS14+ bring-up product. It is not part of the
VC7.1 exact-build lane and it does not include the original executable or game
archives.

The current milestone is an arm64 device / Intel simulator UIKit bring-up with:

- a 60 Hz display loop and persistent `Documents/startup.log` diagnostics;
- touch movement with direct Z/S action regions;
- a settings overlay with persistent Z and S hold/toggle modes;
- a private user-data path for legally supplied `th095.dat` testing;
- a native THA1/PBG/LZSS reader that validates all 262 archive entries;
- real `title.anm` and `world01.anm` THTX texture decoding on the simulator.

The TH095 gameplay state machine, ANM/D3D8 renderer, audio, menu scripts,
replay, and scene/ECL execution are not yet connected to this bring-up. The
current app is a resource and input validation build and must not be described
or packaged as a complete game port.

The Intel Mac iOS 14 simulator has been checked with the archive in the app's
private `Documents/TH095Data` directory. The startup log records
`archive entries=262 probes=5 title=256x256`, and the screenshot shows the
decoded original title texture without a black-screen failure. An arm64
iphoneos build also completes with an iOS 14 deployment target. These checks
cover archive and texture bring-up only; they do not establish full gameplay.

Build on the Mac with CMake/Xcode 14, deployment target iOS 14.0. Use an
`arm64` device build and `x86_64` on an Intel Mac simulator. Original data and
credentials are deliberately excluded from source and IPA artifacts.
