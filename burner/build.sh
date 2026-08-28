#!/bin/bash
# Offline APK build — no Gradle/AGP, just the SDK's own tools.
# Produces build/rc-burner.apk signed with the standard Android debug key.
set -euo pipefail
cd "$(dirname "$0")"

SDK="$HOME/Library/Android/sdk"
BT="$SDK/build-tools/35.0.0"
PLATFORM="$SDK/platforms/android-35/android.jar"
JAVA_HOME="${JAVA_HOME:-/opt/homebrew/opt/openjdk@21}"
export JAVA_HOME
PATH="$JAVA_HOME/bin:$PATH"

rm -rf build
mkdir -p build/classes

echo "== aapt2 link (manifest, no resources) =="
"$BT/aapt2" link \
    --manifest AndroidManifest.xml \
    -I "$PLATFORM" \
    --min-sdk-version 26 \
    --target-sdk-version 35 \
    -o build/base.apk

echo "== javac =="
javac --release 17 \
    -classpath "$PLATFORM" \
    -d build/classes \
    $(find src -name '*.java')

echo "== d8 =="
"$BT/d8" --release \
    --lib "$PLATFORM" \
    --min-api 26 \
    --output build \
    $(find build/classes -name '*.class')

echo "== package dex into apk =="
(cd build && zip -q -u base.apk classes.dex)

echo "== zipalign + sign =="
"$BT/zipalign" -f 4 build/base.apk build/rc-burner-unsigned.apk
"$BT/apksigner" sign \
    --ks "$HOME/.android/debug.keystore" \
    --ks-pass pass:android \
    --ks-key-alias androiddebugkey \
    --out build/rc-burner.apk \
    build/rc-burner-unsigned.apk

echo
echo "OK: $(pwd)/build/rc-burner.apk"
echo "Install with: adb install -r build/rc-burner.apk"
