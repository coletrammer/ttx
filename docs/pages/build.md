# Building

This package can be built either directly through [CMake](https://cmake.org/) or using [nix](https://nixos.org/).

## Building with CMake

### Prerequisites

- Install CMake version 3.21 or later.
- Install either GCC 14+ or Clang 19+ (only necessary to actual use the header files).

### Dependencies

- The [di](https://github.com/coletrammer) library.

The di library will be found using CMake `find_package` unless the source is available
at the path specified by `dius_di_DIRECTORY`. By default, that CMake variable is set
`./di`. When using nix, the library's flake is included automatically.

### Consuming via CMake

This project exports a CMake package to be used with the
[`find_package`](https://cmake.org/cmake/help/latest/command/find_package.html)
command of CMake:

- Package name: `dius`
- Target name: `dius::dius`

In general, you can either include `find_package(dius REQUIRED)` somewhere in your CMake build or call
`add_subdirectory` on the dius source code. When using `find_package`, it probably makes sense to use
fetch content to download the library during the build. When using `add_subdirectory`, this project
can be included as a git submodule. Because CMake is CMake, there's several other ways to make
things work, and this library tries to be as flexible as possible so that both of the above methods
will succeed.

Afterwards, use the library via `target_link_libraries(target PRIVATE dius::dius)`.

### Manual Build Commands

To manual build the library, use the following commands.

```sh
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build
```

### Install Commands

Afterwards, `find_package` should succeed by using a system installation of the `di` library.

```sh
cmake --install build
```

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

To consume the library in your flake, add `dius` as an input:

```nix
dius = {
  url = "github.com/coletrammer/dius";
  inputs.nixpkgs.follows = "nixpkgs";
};
```

Then include `inputs.dius.packages.${system}.default` in the `buildInputs` or your derivation. Assuming your
project is using CMake, `find_package(dius)` will succeed automatically.

This flake provides takes the di library as a flake input, so it can be overridden easily.

### Manual Build Commands

Alternatively, use the `nix` command to build the library manually.

```sh
nix build .
```

To build the dius runtime variable, use:

```sh
nix build.#dius-runtime
```

This outputs the result to `./result`.
