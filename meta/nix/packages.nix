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

          cmakeFlags = [ "-Dttx_APP_ONLY=ON" ];

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
            ncurses
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
            makeBinaryWrapper
          ];

          unpackPhase = "true";
          installPhase = ''
            mkdir -p $out/bin
            cp ${dep}/bin/* $out/bin

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
      ttx-unwrapped = mkUnwrappedPackage pkgs.stdenv "ttx-unwrapped" ttx-lib;
      ttx-dius-runtime-unwrapped =
        mkUnwrappedPackage pkgs.stdenv "ttx-dius-runtime-unwrapped"
          ttx-lib-dius-runtime;
      ttx = mkWrappedPackage pkgs.stdenv "ttx" ttx-unwrapped [
        pkgs.fzf # fzf is used for popup menus
        pkgs.ncurses # tic may be used to compile terminfo, if not already installed
      ];
      ttx-dius-runtime = mkWrappedPackage pkgs.stdenv "ttx-dius-runtime" ttx-dius-runtime-unwrapped [
        pkgs.fzf
        pkgs.ncurses
      ];

      ttx-app = mkApp ttx;
      ttx-app-dius-runtime = mkApp ttx-dius-runtime;
    in
    {
      apps = {
        default = ttx-app;
        ttx = ttx-app;
        ttx-dius-runtime = ttx-app-dius-runtime;
      };
      packages = {
        default = ttx;
        ttx = ttx;
        ttx-dius-runtime = ttx-dius-runtime;
        ttx-unwrapped = ttx-unwrapped;
        ttx-dius-runtime-unwrapped = ttx-dius-runtime-unwrapped;
        ttx-lib = ttx-lib;
        ttx-lib-dius-runtime = ttx-lib-dius-runtime;
      };
    };
}
