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
            description = ''Package to install (nullable)'';
          };

          settings = {
            shell = lib.mkOption {
              type = with lib.types; str;
              default = "$SHELL";
              description = ''Default shell when opening panes'';
            };

            prefix = lib.mkOption {
              type = with lib.types; str;
              default = "B";
              description = "Prefix key for key bindings. The control modifier is always applied.";
            };
          };
        };
      };

      config = lib.mkIf config.programs.ttx.enable {
        home.packages =
          if config.programs.ttx.package == null then [ ] else [ config.programs.ttx.package ];

        home.shellAliases = {
          ttx = "ttx --prefix ${config.programs.ttx.settings.prefix} -- ${config.programs.ttx.settings.shell}";
        };
      };
    };
}
