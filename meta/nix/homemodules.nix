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
        };
      };

      config = lib.mkIf config.programs.ttx.enable {
        home.packages =
          if config.programs.ttx.package == null then [ ] else [ config.programs.ttx.package ];
      };
    };
}
