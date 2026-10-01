#!/bin/bash
# BUILD_MAC.sh — Compila Acid Synth per macOS
# Richiede: Xcode Command Line Tools (gcc/clang)
#
# Uso:
#   ./BUILD_MAC.sh              # Compila standalone
#   ./BUILD_MAC.sh vst3         # Compila VST3
#   ./BUILD_MAC.sh all          # Compila entrambi
#   ./BUILD_MAC.sh test         # Esegue i test
#
# Output:
#   acid_synth_mac        — standalone macOS (CoreAudio)
#   AcidSynth.vst3/       — bundle VST3

set -e
SRC="src/delay.c src/filter.c src/lfo.c src/osc.c src/reverb.c src/sequencer.c src/synth.c src/wavefolder.c src/scale.c"
cd "$(dirname "$0")"

build_standalone() {
    echo "=== Building macOS standalone (CoreAudio) ==="
    gcc -O2 -Isrc src/standalone_mac.c $SRC \
        -framework CoreAudio -framework AudioToolbox \
        -framework CoreFoundation -framework Carbon \
        -lm -o acid_synth_mac
    echo "  → acid_synth_mac"
    ls -la acid_synth_mac
}

build_vst3() {
    echo "=== Building VST3 plugin ==="
    # Crea struttura bundle VST3
    BUNDLE="AcidSynth.vst3"
    mkdir -p "$BUNDLE/Contents/MacOS" "$BUNDLE/Contents/Resources"

    # Info.plist per il bundle
    cat > "$BUNDLE/Contents/Info.plist" << 'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key><string>English</string>
    <key>CFBundleExecutable</key><string>acid_synth</string>
    <key>CFBundleIdentifier</key><string>com.pietro.acidsynth.vst3</string>
    <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
    <key>CFBundleName</key><string>Acid Synth v4</string>
    <key>CFBundlePackageType</key><string>BNDL</string>
    <key>CFBundleShortVersionString</key><string>1.0.0</string>
    <key>CFBundleVersion</key><string>1.0.0</string>
    <key>CSResourcesFileMapped</key><true/>
    <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

    # Compila il binario VST3
    g++ -O2 -std=c++11 -Isrc src/vst3_acid.cpp $SRC \
        -dynamiclib \
        -lm -framework CoreFoundation \
        -o "$BUNDLE/Contents/MacOS/acid_synth"

    echo "  → $BUNDLE"
    ls -la "$BUNDLE/Contents/MacOS/acid_synth"
}

run_tests() {
    echo "=== Running all tests ==="
    local ok=0 fail=0
    for t in tests/test_*.c; do
        local n=$(basename "$t" .c)
        [ "$n" = "test_util" ] && continue
        gcc -Isrc -o "/tmp/${n}" $SRC "$t" -lm 2>/dev/null && \
        output=$(/tmp/${n} 2>&1) && \
        echo "$output" | grep -E "^(  TEST|===)" && \
        ok=$((ok+1)) || fail=$((fail+1))
    done
    echo "=== SUITES: $ok passed, $fail failed ==="
}

case "${1:-standalone}" in
    standalone) build_standalone ;;
    vst3)       build_vst3 ;;
    all)        build_standalone; build_vst3 ;;
    test)       run_tests ;;
    *)
        echo "Uso: $0 [standalone|vst3|all|test]"
        exit 1
        ;;
esac
