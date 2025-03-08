{ inputs, ... }:
{
  imports = [
    inputs.treefmt-nix.flakeModule
  ];

  perSystem =
    {
      pkgs,
      ...
    }:
    let
      constants = import ../constants.nix;
      clangTools = pkgs."llvmPackages_${constants.clangVersion}".clang-tools.override {
        enableLibcxx = true;
      };
    in
    {

      treefmt = {
        programs = {
          clang-format = {
            enable = true;
            package = clangTools;
          };
          nixfmt.enable = true;
          prettier.enable = true;
          shfmt = {
            enable = true;
            indent_size = 4;
          };
          just.enable = true;
          cmake-format.enable = true;
        };

        settings.formatter.cmake-format.includes = [
          "CMakeLists.txt"
          "**/CMakeLists.txt"
          "*.cmake"
        ];

        settings.formatter.prettier.includes = [
          "**/.clang-tidy"
          ".clang-format"
          ".clang-tidy"
          ".clangd"
          ".prettierrc"
          "**/flake.lock"
          "flake.lock"
        ];

        settings.excludes = [
          ".editorconfig"
          ".git-blame-ignore-revs"
          ".github/CODEOWNERS"
          ".gitignore"
          ".prettierignore"
          "**/*.xml"
          "**/*.in"
          "docs/header.html"
          "LICENSE"
        ];
      };
    };
}
