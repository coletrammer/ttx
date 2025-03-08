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

cd "$TTX_ROOT"

tidy_args="-p ${TTX_BUILD_DIR} -use-color 1 -allow-no-checks -quiet"

# clang-tidy is broken in nixpkgs apparently, and is unable to find
# any standard library headers. This code manually grabs the needed
# include paths from clang++'s verbose output. This logic replicates
# the query-driver option in clangd, (which of course is not supported
# by clang-tidy, (and of course shouldn't be needed because the compiler
# already is clang)).
in_search=0
while read -r path; do
    if echo "$path" | grep -q "<...> search starts here"; then
        in_search=1
        continue
    elif echo "$path" | grep -q "End of search list"; then
        in_search=0
        continue
    fi
    if [ "$in_search" = 1 ]; then
        tidy_args="${tidy_args} -extra-arg-before=-I$path"
    fi
done < <(clang++ -v -c -xc++ /dev/null -o /dev/null 2>&1)

action="$1"
shift

case $action in
tidy)
    tidy_args="$tidy_args -fix -format"
    ;;
analyze)
    tidy_args="$tidy_args -checks=-*,clang-analyzer-*,-clang-analyzer-cplusplus*"
    ;;
check) ;;
*) die "error: action '$action' is not valid" ;;
esac

while getopts ":s:a:" opt; do
    case $opt in
    s)
        tidy_args="$tidy_args -source-filter $OPTARG"
        ;;
    a)
        tidy_args="$tidy_args $OPTARG"
        ;;
    \?)
        die "Invalid option: -$OPTARG"
        ;;
    esac
done

set -x
! run-clang-tidy $tidy_args 2>&1 | grep -vE '^$|Applying fixes|clang-tidy|[[:digit:]]+ warnings? generated'
