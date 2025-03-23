#!/usr/bin/env bash

set -e

PARENT_DIR=$(realpath "$(dirname -- "$0")")
TTX_ROOT=$(realpath "$PARENT_DIR"/../..)
TTX_BUILD_DIR=${TTX_BUILD_DIR:-$(realpath .)}

die() {
    echo "$@"
    exit 1
}

if [ $# = 0 ]; then
    die "error: this script must be passed at least 1 argument"
fi

input="$TTX_ROOT/lib/test/cases/$1.input.ansi"
expected="$TTX_ROOT/lib/test/cases/$1.expected.ansi"

if [ ! -e "$input" ]; then
    die "Test case input file does not exit ('$input')"
fi

# Produce output
rm -f "$expected"
"$TTX_BUILD_DIR/ttx" --headless -S "$expected" -r "$input"

# Show output via ttx
"$TTX_BUILD_DIR/ttx" -r "$expected"
