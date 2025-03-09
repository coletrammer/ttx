# Building

This package can be built either directly through [CMake](https://cmake.org/) or using [nix](https://nixos.org/).

## Building with CMake

### Prerequisites

- Install CMake version 3.21 or later.
- Install either GCC 14+ or Clang 19+.

### Dependencies

- The [dius](https://github.com/coletrammer/dius) library.
- The [di](https://github.com/coletrammer/di) library (dependency of dius).

The dius library will be found using CMake `find_package` unless the source is available
at the path specified by `ttx_dius_DIRECTORY`. By default, that CMake variable is set
`./dius`. When using nix, the library's flake is included automatically.

The recommended setup for building ttx from source involves the following shell commands.

```sh
# Clone all repositories
git clone https://github.com/coletrammer/di
git clone https://github.com/coletrammer/dius
git clone https://github.com/coletrammer/ttx

# Setup dependencies
ln -s "$(realpath ./di)" ./dius/di
ln -s "$(realpath ./dius)" ./ttx/dius
```

### Manual Build Commands

To manually build ttx and its library, use the following commands.

```sh
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build
```

### Install Commands

```sh
cmake --install build
```

### Consuming the ttx Library via CMake

This project exports a CMake package to be used with the
[`find_package`](https://cmake.org/cmake/help/latest/command/find_package.html)
command of CMake:

- Package name: `ttx`
- Target name: `ttx::ttx`

In general, you can either include `find_package(ttx REQUIRED)` somewhere in your CMake build or call
`add_subdirectory` on the ttx source code. When using `find_package`, it probably makes sense to use
fetch content to download the library during the build. When using `add_subdirectory`, this project
can be included as a git submodule. Because CMake is CMake, there's several other ways to make
things work, and this library tries to be as flexible as possible so that both of the above methods
will succeed.

Afterwards, use the library via `target_link_libraries(target PRIVATE ttx::ttx)`.

### Note to packagers

The `CMAKE_INSTALL_INCLUDEDIR` is set to a path other than just `include` if
the project is configured as a top level project to avoid indirectly including
other libraries when installed to a common prefix. Please review the
[install-rules.cmake](cmake/install-rules.cmake) file for the full set of
install rules.

## Building with nix

### Prerequisites

- Install [nix](https://nixos.org/download/).
- Enable the following [experimental features](https://nix.dev/manual/nix/latest/development/experimental-features):
  - nix-command
  - flakes

### Consuming via Flake

This section assumes you are interested in the ttx library. See [./install.md] for instructions on
installing the ttx application using nix.

To consume the library in your flake, add ttx as an input:

```nix
ttx = {
  url = "github.com/coletrammer/ttx";
  inputs.nixpkgs.follows = "nixpkgs";
};
```

Then include `inputs.ttx.packages.${system}.ttx-lib` in the `buildInputs` of your derivation. Assuming your
project is using CMake, `find_package(ttx)` will succeed automatically.

This flake provides takes the di and dius libraries as a flake input, so it can be overridden easily.

### Manual Build Commands

Alternatively, use the `nix` command to build the library manually.

```sh
nix build .#ttx-lib
```

This outputs the result to `./result`.
