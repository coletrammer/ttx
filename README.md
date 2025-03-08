# dius Library

A cross-platform C++ platform runtime library. Optionally provides a no libc runtime for Linux.

## Features

- Supports Linux and MacOS (Windows is not yet supported)
- Optional C++ runtime implemented with no libc on Linux.
- Implements an abstraction for accessing the following resources in a cross-platform manner:
  - Process spawning and managemnet
  - Threads and synchronization primitives
  - File IO
  - File system access
  - Async IO execution context (io_uring on Linux)
- Light-weight unit test runner (dius_test_main library).

## Dependencies

This project depends on and integrates with the [di](https://github.com/coletrammer/di) library, by providing a custom
`di` platform implementation.

## Architecture Docs

See [here](docs/pages/architecture_docs.md).

## Building

See [here](docs/pages/build.md).

## Developing

See [here](docs/pages/developing.md).
