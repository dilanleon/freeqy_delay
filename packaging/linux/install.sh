#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf 'Usage: %s [--vst3] [--clap]\n' "${0##*/}"
}

vst3=false
clap=false
if (( $# == 0 )); then
    if [[ ! -t 0 ]]; then
        usage >&2
        exit 2
    fi
    printf 'Install FREEQY DELAY for this user.\n'
    read -r -p 'Install VST3 [Y/n]? ' vst3_answer
    read -r -p 'Install CLAP [Y/n]? ' clap_answer
    case "$vst3_answer" in ''|[yY]|[yY][eE][sS]) vst3=true ;; esac
    case "$clap_answer" in ''|[yY]|[yY][eE][sS]) clap=true ;; esac
else
    for option in "$@"; do
        case "$option" in
            --vst3) vst3=true ;;
            --clap) clap=true ;;
            --help|-h) usage; exit 0 ;;
            *) usage >&2; exit 2 ;;
        esac
    done
fi

if ! "$vst3" && ! "$clap"; then
    echo 'Select at least one plug-in format.' >&2
    exit 2
fi
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 || $EUID == 0 ]]; then
    echo 'Run the installer on x86-64 Linux as the user who runs the plug-in host.' >&2
    exit 1
fi

payload=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
: "${HOME:?HOME must point to the current user home}"

# Copy before replacing the installed artifact and restore it if the final move fails.
install_format() (
    local name=$1 target_dir=$2 staging
    mkdir -p -- "$target_dir"
    exec 9< "$target_dir"
    flock -x 9
    staging=$(mktemp -d "$target_dir/.freeqy-install.XXXXXXXX")
    trap 'rmdir -- "$staging" 2>/dev/null || true' EXIT
    if ! cp -a -- "$payload/$name" "$staging/new"; then
        rm -rf -- "$staging/new"
        exit 1
    fi
    if [[ -e "$target_dir/$name" || -L "$target_dir/$name" ]]; then
        mv -T -- "$target_dir/$name" "$staging/previous"
    fi
    if ! mv -T -- "$staging/new" "$target_dir/$name"; then
        if [[ -e "$staging/previous" || -L "$staging/previous" ]]; then
            mv -T -- "$staging/previous" "$target_dir/$name" ||
                printf 'Previous plug-in is saved in %s\n' "$staging/previous" >&2
        fi
        rm -rf -- "$staging/new"
        exit 1
    fi
    rm -rf -- "$staging/previous"
    printf 'Installed %s\n' "$target_dir/$name"
)

if "$vst3"; then install_format 'FREEQY DELAY.vst3' "$HOME/.vst3"; fi
if "$clap"; then install_format 'FREEQY DELAY.clap' "$HOME/.clap"; fi
