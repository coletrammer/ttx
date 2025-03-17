{ inputs, ... }:
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

      mkAppPackage =
        stdenv: name: dep:
        stdenv.mkDerivation {
          name = "${name}-${version}";
          version = version;
          src = ../../.;

          cmakeFlags = [ "-Dttx_APP_ONLY=ON" ];

          nativeBuildInputs = with pkgs; [
            cmake
            ninja
          ];

          buildInputs = [ dep ];
        };

      mkApp = pkg: {
        type = "app";
        program = "${pkg}/bin/ttx";
      };

      ttx-lib = mkLibPackage pkgs.stdenv "ttx-lib" "dius";
      ttx-lib-dius-runtime = mkLibPackage pkgs.stdenv "ttx-lib-dius-runtime" "dius-runtime";
      ttx = mkAppPackage pkgs.stdenv "ttx" ttx-lib;
      ttx-dius-runtime = mkAppPackage pkgs.stdenv "ttx-dius-runtime" ttx-lib-dius-runtime;

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
        ttx-lib = ttx-lib;
        ttx-lib-dius-runtime = ttx-lib-dius-runtime;
      };
    };
}
