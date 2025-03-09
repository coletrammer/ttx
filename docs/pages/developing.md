# Developing

Developers are strongly encouraged to use [nix](https://nixos.org/) for managing the projects dependencies. This makes
setup completely hassle free.

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
to version 19+. Otherwise, `clangd` will choke on any code which uses C++ 26 features.

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

You have a few options to pass `dius_DEVELOPER_MODE` to the configure
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
      "inherits": ["dev-mode", "docs", "ci-<os>"],
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

#### `docs`

Available if `BUILD_DOCS` is enabled. Builds to documentation using
Doxygen. The output will go to `<binary-dir>/docs/html` by default
(customizable using `DOXYGEN_OUTPUT_DIRECTORY`).

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

Building and running the unit tests is as simple as:

```sh
just bt
```

To format all source files, use:

```sh
just format
```

To actually run ttx, use:

```sh
just run
```

To see an overview of all `just` commands, simply run `just` with no arguments.

```sh
just
```

There are several additional commands which should cover everything necessary
to develop the project.
