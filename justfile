preset := env("PRESET", "dev")

alias c := configure
alias b := build
alias t := test
alias ut := unit_test
alias tt := terminal_test
alias gtt := generate_terminal_test
alias cb := configure_build
alias bt := build_test
alias but := build_unit_test
alias cbt := configure_build_test
alias r := run
alias br := build_run
alias cbr := configure_build_run
alias bf := build_file
alias vh := verify_headers

# Default command: list rules
default:
    @just --list

# Configure
configure *args="":
    cmake --preset {{ preset }} {{ args }}

# Build
build *args="": ensure_configured
    cmake --build --preset {{ preset }} {{ args }}

# Run tests
test *args="": ensure_configured
    ctest --preset {{ preset }} {{ args }}

# Run unit tests
unit_test *args="": ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    build_directory=$(
        jq -rc '.configurePresets.[] | select(.name == "{{ preset }}") | .binaryDir' CMakePresets.json CMakeUserPresets.json |
            sed s/\${sourceDir}/./g
    )

    $build_directory/lib/test/ttx_test {{ args }}

# Run terminal tests
terminal_test *args="": ensure_configured
    ctest --preset {{ preset }} -R "^terminal:" {{ args }}

# Generate expected result terminal test
generate_terminal_test name: ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    export TTX_GENERATE_TERMINAL_TEST_ARGS="{{ name }}"
    cmake --build --preset {{ preset }} -t generate-terminal-test

# Run ttx
run *args="": ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    build_directory=$(
        jq -rc '.configurePresets.[] | select(.name == "{{ preset }}") | .binaryDir' CMakePresets.json CMakeUserPresets.json |
            sed s/\${sourceDir}/./g
    )

    $build_directory/ttx {{ args }}

# Simulate base CI
ci:
    @just preset=ci-ubuntu-gcc cleanall
    @just preset=ci-ubuntu-gcc configure
    @just preset=ci-ubuntu-gcc check
    @just preset=ci-ubuntu-gcc build_docs
    @just preset=ci-ubuntu-gcc check_tidy
    @just preset=ci-ubuntu-gcc build_test

# Configure and build
configure_build:
    @just preset={{ preset }} configure
    @just preset={{ preset }} build

# Build and test
build_test *args="":
    @just preset={{ preset }} build -t ttx_test
    @just preset={{ preset }} test {{ args }}

# Build and run unit tests
build_unit_test *args="":
    @just preset={{ preset }} build -t ttx_test
    @just preset={{ preset }} unit_test {{ args }}

# Configure and build and test
configure_build_test *args="":
    @just preset={{ preset }} configure
    @just preset={{ preset }} bt {{ args }}

# Build and run
build_run *args="":
    @just preset={{ preset }} build -t ttx
    @just preset={{ preset }} run {{ args }}

# Configure and build and run
configure_build_run *args="":
    @just preset={{ preset }} configure
    @just preset={{ preset }} br {{ args }}

# Compile a specific file (regex matching)
build_file name: ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    targets=$(
        cmake --build --preset {{ preset }} -- -t targets all |
            cut -d ' ' -f 1 |
            tr -d '[:]' |
            grep -E "{{ name }}"
    )

    echo -e '\e[1m'"cmake --build --preset {{ preset }} -t "$targets'\e[m'
    cmake --build --preset {{ preset }} -t ${targets}

# Verify all header files
verify_headers:
    @just preset={{ preset }} build -t all_verify_interface_header_sets

# Run clang-tidy and perform fixes
tidy *args="": ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    export ttx_TIDY_ARGS="{{ args }}"
    cmake --build --preset {{ preset }} -t tidy

# Run static analysis
analyze *args="": ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    export ttx_TIDY_ARGS="{{ args }}"
    cmake --build --preset {{ preset }} -t analyze

# Run clang-tidy and output failures
check_tidy *args="": ensure_configured
    #!/usr/bin/env bash
    set -euo pipefail

    export ttx_TIDY_ARGS="{{ args }}"
    cmake --build --preset {{ preset }} -t check_tidy

# Clean
clean: ensure_configured
    @just preset={{ preset }} build --target clean

# Build docs
build_docs: ensure_configured
    @just preset={{ preset }} build --target docs

# Open docs
open_docs: ensure_configured
    @just preset={{ preset }} build --target open_docs

# Build and open docs
docs:
    @just preset={{ preset }} build_docs
    @just preset={{ preset }} open_docs

# Auto-format source code
format:
    nix fmt

# Validate format and lint code
check:
    nix flake check

# Update flakes
update:
    nix flake update --flake .
    nix flake update --flake ./meta/nix/dev

# Select a CMake preset (meant to be run with eval, e.g. `eval $(just choose)`)
choose:
    @echo "export PRESET=\$(cmake --list-presets=configure | tail +2 | fzf | awk '{ print \$1 }' | tr -d '[\"]')"

# Clean all
cleanall:
    rm -rf build/

[private]
ensure_configured preset=preset: ensure_user_presets
    #!/usr/bin/env bash
    set -euo pipefail

    build_directory=$(
        jq -rc '.configurePresets.[] | select(.name == "{{ preset }}") | .binaryDir' CMakePresets.json CMakeUserPresets.json |
            sed s/\${sourceDir}/./g
    )
    if [ ! -d "$build_directory" ]; then
        echo -e '\e[1m'"cmake --preset {{ preset }}"'\e[m'
        cmake --preset {{ preset }}
    fi

    build_directory="$(realpath $build_directory)"
    if [ "$(readlink build/compile_commands.json)" != "$build_directory"/compile_commands.json ]; then
        rm -f compile_commands.json
        ln -s "$build_directory"/compile_commands.json compile_commands.json
    fi

[private]
ensure_user_presets:
    #!/usr/bin/env bash
    set -euo pipefail

    if [ ! -e "CMakeUserPresets.json" ]; then
        echo '{"version":2,"configurePresets":[]}' > CMakeUserPresets.json
    fi
