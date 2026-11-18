#!/usr/bin/env bash
set -euo pipefail

installer_dir=${1:?pass the directory containing install.sh and its payload}
build_dir=${2:-}
test_dir=$(mktemp -d)
trap 'rm -rf -- "$test_dir"' EXIT
export HOME="$test_dir/home"
mkdir -p "$HOME"

vst3='FREEQY DELAY.vst3'
clap='FREEQY DELAY.clap'
export FREEQY_TEST_INSTALLER="$installer_dir/install.sh"
if bash "$installer_dir/install.sh" </dev/null; then
    echo 'Installation without a terminal or explicit selection should fail' >&2
    exit 1
fi
if bash "$installer_dir/install.sh" --unknown; then
    echo 'Installation with an invalid option should fail' >&2
    exit 1
fi
if printf 'n\nn\n' | script -qec 'bash "$FREEQY_TEST_INSTALLER"' /dev/null; then
    echo 'An empty interactive selection should fail' >&2
    exit 1
fi

bash "$installer_dir/install.sh" --vst3
diff -qr "$installer_dir/$vst3" "$HOME/.vst3/$vst3"
test ! -e "$HOME/.clap/$clap"

rm -r "$HOME/.vst3"
bash "$installer_dir/install.sh" --clap
cmp "$installer_dir/$clap" "$HOME/.clap/$clap"
test ! -e "$HOME/.vst3/$vst3"

rm -r "$HOME/.clap"
printf '\n\n' | script -qec 'bash "$FREEQY_TEST_INSTALLER"' /dev/null
diff -qr "$installer_dir/$vst3" "$HOME/.vst3/$vst3"
cmp "$installer_dir/$clap" "$HOME/.clap/$clap"
touch "$HOME/.vst3/$vst3/obsolete"
printf 'old CLAP\n' > "$HOME/.clap/$clap"
bash "$installer_dir/install.sh" --vst3
diff -qr "$installer_dir/$vst3" "$HOME/.vst3/$vst3"
test "$(cat "$HOME/.clap/$clap")" = 'old CLAP'
bash "$installer_dir/install.sh" --clap
cmp "$installer_dir/$clap" "$HOME/.clap/$clap"

if [[ -n $build_dir ]]; then
    export FREEQY_TEST_INSTALLED_VST3="$HOME/.vst3/$vst3"
    export FREEQY_TEST_INSTALLED_CLAP="$HOME/.clap/$clap"
    for module in "$FREEQY_TEST_INSTALLED_VST3"/Contents/x86_64-linux/*.so "$FREEQY_TEST_INSTALLED_CLAP"; do
        dependencies=$(ldd "$module")
        if [[ $dependencies == *'not found'* ]]; then
            printf '%s\n' "$dependencies" >&2
            exit 1
        fi
    done
    xvfb-run -a ctest --test-dir "$build_dir" -C Release --output-on-failure -R '^(vst3_host|clap_host)$'
fi
