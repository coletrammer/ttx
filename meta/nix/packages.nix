{ inputs, lib, ... }:
{
  perSystem =
    { pkgs, system, ... }:
    let
      version = "0.1.0";

      mkLibPackage =
        stdenv: name: dius:
        stdenv.mkDerivation {
          name = "${name}-${version}";
          version = version;
          src = ../../.;

          cmakeFlags = [ "-Dttx_LIB_ONLY=ON" ];

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
          ];

          propagatedBuildInputs = [
            inputs.dius.packages.${system}.${dius}
          ];
        };

      mkUnwrappedPackage =
        stdenv: name: dep:
        stdenv.mkDerivation {
          name = "${name}-${version}";
          version = version;
          src = ../../.;

          cmakeFlags = [
            "-Dttx_APP_ONLY=ON"
            "-Dttx_BUILT_IN_THEMES=ON"
            "-Dttx_iterm2_color_schemes_SOURCE_DIR=${inputs.iterm2-color-schemes}"
          ];

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            ncurses
            python3
          ];

          buildInputs = [ dep ];
        };

      mkWrappedPackage =
        stdenv: name: dep: runtime-deps:
        let
          runtimeDeps = runtime-deps;
        in
        stdenv.mkDerivation {
          name = "${name}-${version}";
          version = version;

          outputs = [
            "out"
            "terminfo"
          ];

          nativeBuildInputs = with pkgs; [
            installShellFiles
            makeBinaryWrapper
          ];

          unpackPhase = "true";
          installPhase = ''
            mkdir -p $out/bin
            cp ${dep}/bin/* $out/bin

            installShellCompletion --zsh ${dep}/share/zsh/site-functions/_ttx
            installShellCompletion --bash ${dep}/share/bash-completions/completions/ttx.bash

            mkdir -p $terminfo/share
            cp -r -p "${dep}/share/terminfo" $terminfo/share/terminfo

            mkdir -p "$out/nix-support"
            echo "$terminfo" >>$out/nix-support/propagated-user-env-packages
          '';
          postFixup = ''
            wrapProgram $out/bin/ttx \
              --suffix PATH : ${lib.makeBinPath runtimeDeps}
          '';
        };

      mkApp = pkg: {
        type = "app";
        program = "${pkg}/bin/ttx";
      };

      ttx-lib = mkLibPackage pkgs.stdenv "ttx-lib" "dius";
      ttx-lib-dius-runtime = mkLibPackage pkgs.stdenv "ttx-lib-dius-runtime" "dius-runtime";
      ttx-lib-static = mkLibPackage pkgs.pkgsStatic.stdenv "ttx-lib-static" "dius-static";
      ttx-unwrapped = mkUnwrappedPackage pkgs.stdenv "ttx-unwrapped" ttx-lib;
      ttx-dius-runtime-unwrapped =
        mkUnwrappedPackage pkgs.stdenv "ttx-dius-runtime-unwrapped"
          ttx-lib-dius-runtime;
      ttx-static-unwrapped =
        mkUnwrappedPackage pkgs.pkgsStatic.stdenv "ttx-static-unwrapped"
          ttx-lib-static;
      ttx = mkWrappedPackage pkgs.stdenv "ttx" ttx-unwrapped [
        pkgs.fzf # fzf is used for popup menus
        pkgs.ncurses # tic may be used to compile terminfo, if not already installed
      ];
      ttx-dius-runtime = mkWrappedPackage pkgs.stdenv "ttx-dius-runtime" ttx-dius-runtime-unwrapped [
        pkgs.fzf
        pkgs.ncurses
      ];
      ttx-static = mkWrappedPackage pkgs.pkgsStatic.stdenv "ttx-static" ttx-static-unwrapped [
        pkgs.fzf
        pkgs.ncurses
      ];

      ttx-app = mkApp ttx;
      ttx-app-dius-runtime = mkApp ttx-dius-runtime;
      ttx-app-static = mkApp ttx-static;
    in
    {
      apps = {
        default = ttx-app;
        ttx = ttx-app;
        ttx-dius-runtime = ttx-app-dius-runtime;
        ttx-static = ttx-app-static;
      };
      packages = {
        default = ttx;
        ttx = ttx;
        ttx-dius-runtime = ttx-dius-runtime;
        ttx-static = ttx-static;
        ttx-unwrapped = ttx-unwrapped;
        ttx-dius-runtime-unwrapped = ttx-dius-runtime-unwrapped;
        ttx-static-unwrapped = ttx-static-unwrapped;
        ttx-lib = ttx-lib;
        ttx-lib-dius-runtime = ttx-lib-dius-runtime;
        ttx-lib-static = ttx-lib-static;
      };
    };
}
