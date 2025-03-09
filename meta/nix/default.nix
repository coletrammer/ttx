{ inputs, ... }:
{
  imports = [
    inputs.flake-parts.flakeModules.partitions
    ./homemodules.nix
    ./packages.nix
  ];

  partitions = {
    # Define the dev partition, which will be used to define things like
    # dev shell and formatting. The actual C++ library package is not
    # in this partion.
    dev = {
      module = ./dev;
      extraInputsFlake = ./dev;
    };
  };

  partitionedAttrs = {
    checks = "dev";
    devShells = "dev";
    formatter = "dev";
  };
}
