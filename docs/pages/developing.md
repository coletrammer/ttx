# Developing

Developers are strongly encouraged to use [nix](https://nixos.org/) for managing the projects dependencies. This makes
setup completely hassle free. However, it is still possible to setup the dependencies manually.

## Nix Dev Shell

This requires installing [Nix](https://nixos.org/download/). To access the shell simply run:

```sh
# NOTE: you will have to enable the flake and nix-command experimental features.
nix develop
```

Additionally, the development environment can be loaded automatically when inside the project directory using
[direnv](https://direnv.net/). After install, simply run:

```sh
direnv allow .
```

Editors like `VS Code` have extensions like [direnv](https://marketplace.visualstudio.com/items?itemName=mkhl.direnv) for
interfacing with the `direnv` shell. This is can be used so that non-terminal based text editors
pick up on the dependencies provided by the `nix` dev shell.

### Note for Auto-Completion

Because this project uses bleeding edge C++, you may need to override the `clangd` version being used by your editor
to version 19+. Otherwise, `clangd` will choke on any code which uses C++ 26 features. In VS Code, the `clangd` extension can
download the latest `clangd` for you.

If you receive compilation errors from `clangd` concerning missing system headers (like `<stddef.h>` or `<utility>`),
you need to add `--query-driver=**/*` to the arguments `clangd` is launched with. Instead of globbing you can pass
the absolute path of your c++ compiler, but that's very inconvenient when using nix.

## CMake Developer Mode

Build system targets that are only useful for developers of this project are
hidden if the `ttx_DEVELOPER_MODE` option is disabled. Enabling this
option makes tests and other developer targets and options available. Not
enabling this option means that you are a consumer of this project and thus you
have no need for these targets and options.

Developer mode is always set to on in CI workflows.

### Presets

This project makes use of [presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) to simplify the process of configuring
the project. As a developer, you are recommended to always have the [latest
CMake version](https://cmake.org/download/) installed to make use of the latest Quality-of-Life
additions.

You have a few options to pass `ttx_DEVELOPER_MODE` to the configure
command, but this project prefers to use presets.

As a developer, you should create a `CMakeUserPresets.json` file at the root of
the project:

```json
{
  "version": 2,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 21,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "dev",
      "binaryDir": "${sourceDir}/build/dev",
      "inherits": ["dev-mode", "clang", "ci-<os>"],
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "dev",
      "configurePreset": "dev",
      "configuration": "Debug"
    }
  ],
  "testPresets": [
    {
      "name": "dev",
      "configurePreset": "dev",
      "configuration": "Debug",
      "output": {
        "outputOnFailure": true
      }
    }
  ]
}
```

You should replace `<os>` in your newly created presets file with the name of
the operating system you have, which may be `win64`, `linux` or `darwin`. You
can see what these correspond to in the
[`CMakePresets.json`](../../CMakePresets.json) file.

The config file shown uses `clang` for compilation, which is recommended for
development because it produces better error messages and is less likely to
break `clangd`. However, you can still use `gcc` by removing the `"clang"`
preset from the list of presets to inherit from.

Add the "built-in-themes" preset to compile build with all the built-in
themes. When not using nix this requires manually checking out [iTerm2 Color Schemes](github.com/mbadolato/iTerm2-Color-Schemes).

`CMakeUserPresets.json` is also the perfect place in which you can put all
sorts of things that you would otherwise want to pass to the configure command
in the terminal.

> **Note**
> Some editors are pretty greedy with how they open projects with presets.
> Some just randomly pick a preset and start configuring without your consent,
> which can be confusing. Make sure that your editor configures when you
> actually want it to, for example in CLion you have to make sure only the
> `dev-dev preset` has `Enable profile` ticked in
> `File > Settings... > Build, Execution, Deployment > CMake` and in Visual
> Studio you have to set the option `Never run configure step automatically`
> in `Tools > Options > CMake` **prior to opening the project**, after which
> you can manually configure using `Project > Configure Cache`.

### Configure Build and Test

If you followed the above instructions, then you can configure, build and test
the project respectively with the following commands from the project root on
any operating system with any build system:

```sh
cmake --preset=dev
cmake --build --preset=dev
ctest --preset=dev
```

If you are using a compatible editor (e.g. VSCode) or IDE (e.g. CLion, VS), you
will also be able to select the above created user presets for automatic
integration.

Please note that both the build and test commands accept a `-j` flag to specify
the number of jobs to use, which should ideally be specified to the number of
threads your CPU has. You may also want to add that to your preset using the
`jobs` property, see the [presets documentation](https://cmake.org/download/) for more details.

> Note
> When not using nix, the `kitty:width` test may fail due to not having installed
> kitty. You should either ignore the test failure or install kitty. This test
> requires kitty because it validates that our unicode behavior matches kitty's.

### Developer Mode Targets

These are targets you may invoke using the build command from above, with an
additional `-t <target>` flag:

#### `coverage`

Available if `ENABLE_COVERAGE` is enabled. This target processes the output of
the previously run tests when built with coverage configuration. The commands
this target runs can be found in the `COVERAGE_TRACE_COMMAND` and
`COVERAGE_HTML_COMMAND` cache variables. The trace command produces an info
file by default, which can be submitted to services with CI integration. The
HTML command uses the trace command's output to generate an HTML document to
`<binary-dir>/coverage_html` by default.

The GitHub CI uses Codecov to keep track of code coverage for the project, which can be viewed
[here](https://app.codecov.io/gh/coletrammer/ttx). The coverage information is uploaded for
every GitHub PR.

#### `docs`

To enable this you must should add the `"docs"` preset to your dev preset via the inherits property.
The docs are generated using [Doxygen](https://www.doxygen.nl/), so if you're not using nix you'll need
to install it. Additionally, when not using nix you must checkout [Doxygen Awesome](https://jothepro.github.io/doxygen-awesome-css/)
and add a CMake configuration variable `TTX_DOXYGEN_AWESOME_DIR` which should be set to the absolute
path of Doxygen Awesome. The output will go to `<binary-dir>/docs/html` by default
(customizable using `DOXYGEN_OUTPUT_DIRECTORY`).

## Justfile

The `justfile` provides a CLI interface for interfacing with the project, and
can be used as an alternative to IDE support for CMake presets for developers
with more minimal setups.

The `justfile` operates on a given CMake preset which is controlled by the
`PRESET` environment variable. This defaults to the `dev` preset you should've
setup already. If you add multiple presets to your `CMakeUserPresets.json` file,
you can select which preset to use interactively by running:

```sh
eval $(just choose)
```

Whenever a new `justfile` command is run, the selected preset will be auto-configured
if the build directory does not exist. Additionally, the `compile_commands.json` for
the build configuration will be symlinked into the project directory. This ensures
`clangd` will be configured properly for your preset (This happens automatically
when using a VS Code extension like `ms-vscode.cpptools`).

Building and running all tests is as simple as:

```sh
just bt
```

To run an individual unit test, use:

```sh
just but -s $suite -t $test
```

To format all source files, use:

```sh
just format
```

To fix C++ lint violations, use:

```sh
just tidy
```

To actually run ttx (and build), use:

```sh
just br
```

To see an overview of all `just` commands, simply run `just` with no arguments.

```sh
just
```

There are several additional commands which should cover everything necessary
to develop the project.

## libttx vs ttx

This project's source files are split between a library `libttx` and an application
`ttx`. `libttx` includes a functioning terminal emulator as well as additional functionality
necessary for making a TUI (like parsing terminal key presses and rendering to the screen).
This functionality could theoretically be used by other applications as the library is
fully consumable via CMake.

Source code in `ttx` is specific to the `ttx` application and mostly orchestrates components
in `libttx`.

## Testing

`ttx` has 2 different test systems, which are used for different purposes. The
unit test framework is for testing individual components. The actual terminal
emulation code has a higher bar for unit test coverage, with an effective target
near 100% code coverage. Other components in `libttx` should have unit tests but
the bar is not as high. Code in the `ttx` application itself rarely has unit tests.
Instead, we rely on the second test framework for test coverage.

The `terminal` testing framework works by feeding an input file consisting of terminal
escape sequences, then dumping the internal state of `ttx` as terminal escape
sequences, and finally comparing that dump to an expected file. So these are effectively
regression tests. The cool part of the system is that `ttx` includes can natively view
these state dumps (as they are just terminal escape sequences), so you can validate the
expected state is correct.

To update the expected output of a terminal test, use the `justfile`'s `generate_terminal_test`
command. For instance:

```sh
just generate_terminal_test zsh-eza
```

This will open an instance of `ttx` with the state dump from running `ttx` in headless mode,
allowing you to manually inspect the state and ensure it is still correct.

To add a new terminal test, run `ttx` with the `-c` (capture) option. Then inside `ttx`
run whatever terminal commands (you're in a shell inside `ttx` now), and then save
the capture via `prefix > shift+i`. Once you have the test input, you can use the
`generate_terminal_test` to get the expected state and validate that it looks correct
visually.

## Code Style

Coding style conventions are enforced automatically via `clang-tidy`. These are enforced
by the CI system and all code in the project follows these conventions. Your editor
should be able to automatically flag and fix many of style errors on your behalf. You
can also fix all linting violations use the `tidy` build target or `just` command.

Additionally, all C++ code is autoformatted via `clang-format`. When using nix,
the `treefmt` command should be used for formatting, as it also includes auto-formatters
for the other file types which occur in the project's source code.

## dius Library

When using the nix environment, the dius library is automatically available to
the CMake build. However, this copy is immutable. If you need to make changes
to both libraries at once, a custom checkout of dius can be used by creating
a symbolic link named `dius` in the root of this package. For instance, if your
dius checkout is in a sibling directory of your ttx checkout, run:

```sh
ln -s "$(realpath ../dius)" dius
```

If you need to modify the di library instead (dius's dependency), you can
add a symlink to dius's directory like so:

```sh
ln -s "$(realpath ../di)" ../dius
```
