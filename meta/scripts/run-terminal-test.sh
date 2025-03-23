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
output="$TTX_BUILD_DIR/lib/test/cases/$1.output.ansi"
expected="$TTX_ROOT/lib/test/cases/$1.expected.ansi"

if [ ! -e "$input" ]; then
    die "Test case input file does not exit ('$input')"
fi

if [ ! -e "$expected" ]; then
    die "Test case input file does not exit ('$expected')"
fi

# Setup output directory
mkdir -p "$(dirname "$output")"
rm -f "$output"

# Produce output
"$TTX_BUILD_DIR/ttx" --headless -S "$output" -r "$input"

# Validate output
diff <(xxd "$expected") <(xxd "$output") || {
    echo "Test failed: $1."
    echo "  Input file: $input"
    echo "  Expected file: $expected"
    echo "  Output file: $output"
    echo "  Diff command: $TTX_BUILD_DIR/ttx -r $expected $output"
    exit 1
}
