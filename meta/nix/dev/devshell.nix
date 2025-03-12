{ inputs, ... }:
{
  perSystem =
    {
      config,
      pkgs,
      system,
      ...
    }:
    let
      constants = import ../constants.nix { inherit system; };
      gcc = pkgs."gcc${constants.gccVersion}";
      clang = pkgs."llvmPackages_${constants.clangVersion}".libcxxClang;
      stdenv = pkgs."gcc${constants.gccVersion}Stdenv";
    in
    {
      devShells.default = pkgs.mkShell.override { inherit stdenv; } {
        shellHook = ''
          export TTX_DOXYGEN_AWESOME_DIR=${inputs.doxygen-awesome-css};
        '';
        packages =
          [
            # Compilers
            clang
            gcc
          ]
          # Treefmt and all individual formatters
          ++ [ config.treefmt.build.wrapper ]
          ++ builtins.attrValues config.treefmt.build.programs
          ++ (with pkgs; [
            # Build support
            cmake
            ninja
            ccache

            # justfile support
            just
            jq
            fzf

            # Docs
            doxygen
            graphviz

            # Coverage
            lcov

            # Dependencies
            inputs.dius.packages.${system}.default
          ]);
      };
    };
}
