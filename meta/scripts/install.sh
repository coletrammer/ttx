#!/bin/sh

set -eu

die() {
    echo "$@"
    exit 1
}

system="$(uname -s)"
arch="$(uname -m)"

case "$system,$arch" in
Darwin*,arm64)
    binary=ttx-macos-arm64
    ;;
Linux*,x86_64)
    binary=ttx-linux-x86_64
    ;;
*)
    die "Unsupported platform system=$system arch=$arch"
    ;;
esac

# Download binary
mkdir -p ~/.local/bin
curl -L "https://github.com/coletrammer/ttx/releases/download/latest/$binary" --output ~/.local/bin/ttx
chmod +x ~/.local/bin/ttx

# Compile terminfo
export PATH="$HOME/.local/bin:$PATH"
ttx terminfo >/tmp/ttx.terminfo
tic -x /tmp/ttx.terminfo

# Bash completions
mkdir -p ~/.local/share/bash-completions/completions
ttx completions bash >~/.local/share/bash-completions/completions/ttx.bash

# Zsh completions
mkdir -p ~/.local/share/zsh/site-functions
ttx completions zsh >~/.local/share/zsh/site-functions/_ttx
