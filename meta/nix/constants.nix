{ system, ... }:
{
  clangVersion = if system == "aarch64-darwin" then "19" else "20";
  gccVersion = "14";
}
