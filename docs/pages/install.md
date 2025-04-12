# Install

The easiest way to install ttx is using [nix](https://nixos). As of now, the only alternative is compiling from source.
In the future, a statically linked binary will available via a GitHub release. However the user will
still need to install `fzf` to make use to the application. To build ttx from source, see the steps
[here](./build.md).

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
  settings = {
    prefix = "A";
    shell = lib.getExe pkgs.zsh;
  };
};
```

### Install Directly (discouraged)

This way isn't the recommended way to use nix but works just fine.

```sh
nix profile install github:coletrammer/ttx
```
