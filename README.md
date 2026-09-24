# TH095 iOS port

Source for the Japanese TH095 (Shoot the Bullet) iOS 14+ port.

See [build and control instructions](ios/README.md) and the
[0.1.1 validation record](ios/VALIDATION.md). The native game runtime, SDL input,
OpenGL ES renderer, archive reader and audio integration are included.
Original game resources and signing credentials are not included.

Version 0.1.1 addresses Release-only black screens, font-pointer corruption,
missing touch-button labels, replay-directory handling and short menu taps.
Simulator validation does not establish complete scene coverage or certify
performance/installation on every iOS device. iPad mini 5 / iOS 14.1 must be
retested with this build before the device-specific report can be closed.

This is a separate portable build product based on
[N0zoM1z0/th095](https://github.com/N0zoM1z0/th095). It makes no additional claim
of byte-exact matching to the original Windows executable.
