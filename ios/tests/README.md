# Simulator UI regression runner

This independent XCTest target launches the separately installed game. Run it
against the ordinary Release game build with scripted input disabled.

Set `TH095_SIMULATOR_DESTINATION` to the simulator you want to use, for example
`platform=iOS Simulator,id=<your-simulator-UDID>`. Then run `sh build.sh` and
`sh run.sh TEST_NAME`. Discover available simulators with
`xcrun simctl list devices`; no local simulator identifier is committed here.
Tests: testRotateLandscapePortrait, testBattleRotateLandscapePortrait,
testReplayReturnThenBattle, testPortraitBattleAndSettings.

Tests change orientation and tap actual screen coordinates. Assertions check
foreground state and the oriented screenshot dimensions; inspect the screenshots
and game logs to establish that menus and battle were actually reached. SDL's
accessibility app.frame can remain portrait even when the drawable has rotated.
The recorded screenshots and drawable log dimensions are the orientation evidence.

`python3 export-screenshots.py results/NAME.xcresult results/NAME-images` exports
unique screenshot payloads and a manifest. Identical attachments may share a
payload. Build products, results and private logs must remain untracked.
