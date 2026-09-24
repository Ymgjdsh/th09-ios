#!/bin/sh
set -eu
cd "$(dirname "$0")"
export DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer
xcodebuild build-for-testing \
  -project TH095UiRegression.xcodeproj \
  -scheme TH095UiRegression \
  -configuration Debug \
  -sdk iphonesimulator \
  -destination 'platform=iOS Simulator,id=8C2244DC-5551-4106-BE9C-85CA02D906C2' \
  -derivedDataPath build \
  CODE_SIGNING_ALLOWED=NO
