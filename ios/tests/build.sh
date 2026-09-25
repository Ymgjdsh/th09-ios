#!/bin/sh
set -eu
cd "$(dirname "$0")"
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
simulator_destination=${TH095_SIMULATOR_DESTINATION:?Set TH095_SIMULATOR_DESTINATION to an Xcode simulator destination}
xcodebuild build-for-testing \
  -project TH095UiRegression.xcodeproj \
  -scheme TH095UiRegression \
  -configuration Debug \
  -sdk iphonesimulator \
  -destination "$simulator_destination" \
  -derivedDataPath build \
  CODE_SIGNING_ALLOWED=NO
