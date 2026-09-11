#!/bin/bash
set -euo pipefail

version="$1"
art="build/KaradagBeat_artefacts/Release"
work="build/pkg"
out="build/installer"

rm -rf "$work"
mkdir -p "$work/vst3" "$work/au" "$work/app" "$out"

cp -R "$art/VST3/Karadag Beat.vst3"      "$work/vst3/"
cp -R "$art/AU/Karadag Beat.component"   "$work/au/"
cp -R "$art/Standalone/Karadag Beat.app" "$work/app/"

for bundle in "$work/vst3/Karadag Beat.vst3" "$work/au/Karadag Beat.component" "$work/app/Karadag Beat.app"; do
    codesign --force --deep --sign - "$bundle"
done

component () {
    local name="$1" location="$2"

    pkgbuild --analyze --root "$work/$name" "$work/$name.plist"

    for i in $(seq 0 20); do
        /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$work/$name.plist" 2>/dev/null || break
    done

    pkgbuild --root "$work/$name" --component-plist "$work/$name.plist" \
             --identifier "com.karadag.karadagbeat.$name" --version "$version" \
             --install-location "$location" "$work/$name.pkg"
}

component vst3 "/Library/Audio/Plug-Ins/VST3"
component au   "/Library/Audio/Plug-Ins/Components"
component app  "/Applications"

sed "s/@VERSION@/$version/g" installer/macos/distribution.xml > "$work/distribution.xml"

productbuild --distribution "$work/distribution.xml" --package-path "$work" \
             --resources installer/macos "$out/KaradagBeat-$version-macOS.pkg"

echo "$out/KaradagBeat-$version-macOS.pkg"
