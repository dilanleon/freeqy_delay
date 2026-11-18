#!/usr/bin/env bash
set -euo pipefail

payload=${1:?pass the CMake installer payload directory}
output=${2:?pass the output directory}
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
package='FREEQY-DELAY-Linux-x86_64'

mkdir -p "$work/$package" "$output"
install -m 755 "$script_dir/install.sh" "$work/$package/install.sh"
cp -a -- "$payload/FREEQY DELAY.vst3" "$payload/FREEQY DELAY.clap" "$work/$package/"
tar -czf "$output/$package.tar.gz" -C "$work" "$package"
