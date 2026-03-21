#!/usr/bin/env bash
# Local native app checks — mirrors CI where possible without emulators.
# XCUITest requires macOS + Xcode + XcodeGen (brew install xcodegen).
#
# Usage:
#   scripts/run-native-fleet-local.sh quick      # no Simulator UI tests
#   scripts/run-native-fleet-local.sh full       # + ios-uitest on macOS
#   scripts/run-native-fleet-local.sh ios-uitest
#   IOS_DEST='platform=iOS Simulator,name=iPhone 16 Pro,OS=latest' scripts/run-native-fleet-local.sh ios-uitest
#
# Android instrumented: start an emulator then:
#   cd apps/android && ./gradlew connectedDebugAndroidTest
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

cmd="${1:-help}"

run_human_kit() {
  echo "==> HumanKit swift test"
  (cd apps/shared/HumanKit && swift test)
}

run_ios_spm() {
  echo "==> iOS Swift package build"
  (cd apps/ios && swift build)
}

run_ios_uitest() {
  echo "==> iOS XCUITest (XcodeGen + xcodebuild)"
  if ! command -v xcodebuild &>/dev/null; then
    echo "SKIP: xcodebuild not found (install Xcode)" >&2
    return 0
  fi
  if ! command -v xcodegen &>/dev/null; then
    echo "ERROR: xcodegen not found — brew install xcodegen" >&2
    return 1
  fi
  local dest="${IOS_DEST:-platform=iOS Simulator,name=iPhone 16,OS=latest}"
  (cd apps/ios && xcodegen generate)
  xcodebuild test \
    -project apps/ios/HumaniOS.xcodeproj \
    -scheme HumaniOS \
    -destination "$dest" \
    -only-testing:HumaniOSUITests \
    -parallel-testing-enabled NO \
    -retry-tests-on-failure \
    CODE_SIGNING_ALLOWED=NO \
    COMPILER_INDEX_STORE_ENABLE=NO
}

run_macos() {
  echo "==> macOS app swift build (debug + release)"
  (cd apps/macos && swift build)
  (cd apps/macos && swift build -c release)
}

run_android_unit() {
  echo "==> Android assembleDebug + lint + unit tests"
  if ! command -v java &>/dev/null; then
    echo "SKIP: java not found" >&2
    return 0
  fi
  (cd apps/android && chmod +x gradlew && ./gradlew assembleDebug lint test)
}

case "$cmd" in
  human-kit) run_human_kit ;;
  ios-spm) run_ios_spm ;;
  ios-uitest) run_ios_uitest ;;
  macos) run_macos ;;
  android-unit) run_android_unit ;;
  quick)
    run_human_kit
    run_ios_spm
    run_macos
    run_android_unit
    ;;
  full)
    run_human_kit
    run_ios_spm
    if [ "$(uname -s)" = Darwin ]; then
      run_ios_uitest
    else
      echo "SKIP: ios-uitest (requires macOS)"
    fi
    run_macos
    run_android_unit
    ;;
  help | *)
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
    ;;
esac
