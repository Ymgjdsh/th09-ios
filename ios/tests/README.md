# Simulator UI regression runner

This independent XCTest target launches the separately installed game. Run it
against the ordinary Release game build with scripted input disabled.

Use `sh build.sh`, then `sh run.sh TEST_NAME`. Set the simulator destination in
both scripts to the desired device listed by `xcrun simctl list devices`.
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
