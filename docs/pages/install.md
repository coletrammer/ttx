# Install

ttx can be installed via [nix](https://nixos) or by downloading a statically linked binary created in CI.
Using nix is preferred because it will install dependencies and associated files automatically.
To build ttx from source, see the steps
[here](./build.md).

## Installing from GitHub Release

The latest builds of ttx are available by [here](https://github.com/coletrammer/ttx/releases/tag/latest).
You must download the correct version of ttx for your platform.
If you are using a platform other than an ARM mac or x86_64 linux you must compile from source
or use nix.

### Install Script

The following script downloads the latest release to `~/.local/bin`, installs shell completion scripts,
and compiles the ttx terminfo.

```sh
curl -sS https://github.com/coletrammer/ttx/releases/download/latest/install.sh | sh
```

The script installs the following files:

| File                                                 | Desciption                                                            |
| ---------------------------------------------------- | --------------------------------------------------------------------- |
| ~/.local/bin/ttx                                     | The actual `ttx` binary.                                              |
| ~/.local/share/bash-completions/completions/ttx.bash | Shell completion script for bash.                                     |
| ~/.local/share/zsh/site-functions/\_ttx              | Shell completion script for zsh.                                      |
| ~/.terminfo/<VARIES>/xterm-ttx                       | Terminfo for `ttx`. The actual path varies depending on the platform. |

Once installed, ensure you add `~/.local/bin` to your `PATH` in your shell's init scripts, via:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Terminfo should be found automatically as the files have been installed to their standard
locations. Bash completions should also work with no additional steps. For zsh, if the completion
directory is not already in your `fpath` you can add it as follows:

```sh
fpath=(~/.local/share/zsh/site-functions $fpath)
```

## Installing with Nix

### Prerequisites

- Install [nix](https://nixos.org/download/).
- Enable the following [experimental features](https://nix.dev/manual/nix/latest/development/experimental-features):
  - nix-command
  - flakes

### Run Without Installation

Using nix, its easy to run ttx directly without installing it.

```sh
nix run github:coletrammer/ttx $SHELL
```

> [!NOTE]
> If the nix command fails, subsequent runs will fail as well because the result is cached by nix. To actually retry the
> build (and fetch the latest commit), pass the `--refresh` to `nix`:

### Install Using Home Manager (recommended)

This project provides a home manager module which you can import into your own configuration. The following steps
assuming your home manager configuration is managed by a flake.

Add this project as an input to your flake:

```nix
ttx = {
  url = "github:coletrammer/ttx";

  # Only include this if you're using nixpkgs unstable. Otherwise the default compiler will
  # be too old.
  inputs.nixpkgs.follows = "nixpkgs";
};
```

Import the home manager module:

```nix
imports = [
  inputs.ttx.homeModules.default
];
```

Enable in your configuration:

```nix
programs.ttx = {
  enable = true;
};
```

### Install Directly (discouraged)

This way isn't the recommended way to use nix but works just fine.

```sh
nix profile install github:coletrammer/ttx
```
