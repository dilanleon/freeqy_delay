#!/bin/bash
set -euo pipefail

package=${1:?pass the unsigned test package path}
build_dir=${2:?pass the CMake build directory}
plug_ins=/Library/Audio/Plug-Ins
vst3="$plug_ins/VST3/FREEQY DELAY.vst3"
clap="$plug_ins/CLAP/FREEQY DELAY.clap"
au="$plug_ins/Components/FREEQY DELAY.component"
for path in "$vst3" "$clap" "$au"; do
    if [[ -e $path || -L $path ]]; then
        echo "Expected a clean test machine. Already installed: $path" >&2
        exit 1
    fi
done
work=$(mktemp -d)

cleanup() {
    rm -rf -- "$work"
    sudo rm -rf -- "$vst3" "$clap" "$au"
}
trap cleanup EXIT

install_formats() {
    local choice
    if (( $# == 0 )); then
        sudo /usr/sbin/installer -pkg "$package" -target /
        return
    fi
    {
        printf '<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><array>\n'
        for choice in "$@"; do
            printf '<dict><key>choiceIdentifier</key><string>%s</string>' "$choice"
            printf '<key>choiceAttribute</key><string>selected</string><key>attributeSetting</key><integer>0</integer></dict>\n'
        done
        printf '</array></plist>\n'
    } > "$work/choices.plist"
    sudo /usr/sbin/installer -pkg "$package" -target / -applyChoiceChangesXML "$work/choices.plist"
}

sudo /usr/sbin/installer -pkg "$package" -target / -showChoicesXML > "$work/defaults.plist"
/usr/bin/plutil -lint "$work/defaults.plist"
if install_formats vst3 clap au; then
    echo 'An empty format selection should fail' >&2
    exit 1
fi
test ! -e "$vst3"
test ! -e "$clap"
test ! -e "$au"
install_formats clap au
test -d "$vst3"
test ! -e "$clap"
test ! -e "$au"
sudo rm -rf -- "$vst3"
install_formats vst3 au
test -d "$clap"
test ! -e "$vst3"
test ! -e "$au"
sudo rm -rf -- "$clap"
install_formats vst3 clap
test -d "$au"
test ! -e "$vst3"
test ! -e "$clap"
sudo rm -rf -- "$au"
install_formats
test -d "$vst3"
test -d "$clap"
test -d "$au"
FREEQY_TEST_INSTALLED_VST3="$vst3" FREEQY_TEST_INSTALLED_CLAP="$clap" \
    ctest --test-dir "$build_dir" -C Release --output-on-failure -R '^(vst3_host|clap_host)$'
/usr/bin/auval -v aufx Frqy Dleo
sudo touch "$vst3/.install-smoke-test" "$clap/.install-smoke-test"
install_formats clap au
test ! -e "$vst3/.install-smoke-test"
test -f "$clap/.install-smoke-test"
test -d "$au"
