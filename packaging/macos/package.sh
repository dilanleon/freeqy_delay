#!/bin/bash
set -euo pipefail

payload=${1:?pass the CMake installer payload directory}
output=${2:?pass the output directory}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
version=$(cat "$payload/version.txt")
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

build_component() {
    local format=$1 bundle=$2 destination=$3
    local root="$work/$format"
    local source="$payload/$bundle"
    local executable
    executable=$(/usr/libexec/PlistBuddy -c 'Print CFBundleExecutable' "$source/Contents/Info.plist")
    /usr/bin/lipo "$source/Contents/MacOS/$executable" -verify_arch arm64 x86_64
    mkdir -p "$root"
    /bin/cp -RX "$source" "$root/$bundle"
    cat > "$work/component.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><array><dict>
    <key>RootRelativeBundlePath</key><string>$bundle</string>
    <key>BundleIsRelocatable</key><false/>
    <key>BundleIsVersionChecked</key><false/>
    <key>BundleHasStrictIdentifier</key><true/>
    <key>BundleOverwriteAction</key><string>upgrade</string>
</dict></array></plist>
EOF
    /usr/bin/codesign --verify --strict "$root/$bundle"
    /usr/bin/pkgbuild --root "$root" --component-plist "$work/component.plist" \
        --identifier "com.dleon.freeqydelay.pkg.$format" --version "$version" \
        --install-location "/Library/Audio/Plug-Ins/$destination" "$work/$format.pkg"
}

build_component vst3 'FREEQY DELAY.vst3' VST3
build_component clap 'FREEQY DELAY.clap' CLAP
build_component au 'FREEQY DELAY.component' Components
mkdir -p "$output"
/usr/bin/productbuild --distribution "$script_dir/Distribution.xml" --package-path "$work" \
    "$output/FREEQY-DELAY-macOS.pkg"
