{ inputs, ... }:
{
  flake.homeModules.default =
    {
      config,
      lib,
      pkgs,
      ...
    }:
    let
      pkg = inputs.self.packages.${pkgs.system}.default;
    in
    {
      options = {
        programs.ttx = {
          enable = lib.mkEnableOption "ttx";

          package = lib.mkOption {
            type = with lib.types; nullOr package;
            default = pkg;
            description = "Package to install (nullable)";
          };

          settings = (import ./homeoptions.nix) { lib = lib; };
        };
      };

      config =
        let
          jsonFormat = pkgs.formats.json { };
          removeNulls = lib.filterAttrsRecursive (_: v: v != null);
        in
        lib.mkIf config.programs.ttx.enable {
          home.packages =
            if config.programs.ttx.package == null then [ ] else [ config.programs.ttx.package ];

          xdg.configFile = lib.mapAttrs' (
            name: value:
            lib.nameValuePair "ttx/${name}.json" {
              source = jsonFormat.generate "${name}.json" (
                {
                  "$schema" = "https://github.com/coletrammer/ttx/raw/refs/heads/main/meta/schema/config.json";
                  version = 1;
                }
                // (removeNulls value)
              );
            }
          ) config.programs.ttx.settings;
        };
    };
}
