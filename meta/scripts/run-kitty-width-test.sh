#!/usr/bin/env bash

set -e

TTX_BUILD_DIR=${TTX_BUILD_DIR:-$(realpath .)}

die() {
    echo "$@"
    exit 1
}

output="$TTX_BUILD_DIR/lib/test/cases/kitty-width-output.txt"

if ! command -v kitten >/dev/null; then
    die "kitten executable not found"
fi

# Produce output
"$TTX_BUILD_DIR/ttx" --force-local-terminfo --headless -- sh -c "kitten __width_test__ | tee $output"

status=$?
cat "$output"
rm -f "$output"
exit $status
