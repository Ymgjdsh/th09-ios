#!/bin/sh
set -eu
cd "$(dirname "$0")"
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
test_name=${1:-testRotateLandscapePortrait}
case "$test_name" in
  testRotateLandscapePortrait|testBattleRotateLandscapePortrait|testReplayReturnThenBattle|testPortraitBattleAndSettings) ;;
  *) echo "Unknown test: $test_name" >&2; exit 2 ;;
esac
mkdir -p results
stamp=$(date +%Y%m%d-%H%M%S)
xcodebuild test-without-building \
  -project TH095UiRegression.xcodeproj \
  -scheme TH095UiRegression \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=8C2244DC-5551-4106-BE9C-85CA02D906C2' \
  -derivedDataPath build \
  -parallel-testing-enabled NO \
  -maximum-concurrent-test-simulator-destinations 1 \
  -only-testing:"TH095UiRegression/TH095UiRegressionTests/$test_name" \
  -resultBundlePath "results/$test_name-$stamp.xcresult" \
  CODE_SIGNING_ALLOWED=NO
