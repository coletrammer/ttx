/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "ttx", "index.html", [
    [ "ttx Overview", "index.html", "index" ],
    [ "Architecture Docs", "md_docs_2pages_2architecture__docs.html", "md_docs_2pages_2architecture__docs" ],
    [ "Building", "md_docs_2pages_2build.html", [
      [ "Building with CMake", "md_docs_2pages_2build.html#autotoc_md12", [
        [ "Prerequisites", "md_docs_2pages_2build.html#autotoc_md13", null ],
        [ "Dependencies", "md_docs_2pages_2build.html#autotoc_md14", null ],
        [ "Manual Build Commands", "md_docs_2pages_2build.html#autotoc_md15", null ],
        [ "Install Commands", "md_docs_2pages_2build.html#autotoc_md16", null ],
        [ "Consuming the ttx Library via CMake", "md_docs_2pages_2build.html#autotoc_md17", null ],
        [ "Note to packagers", "md_docs_2pages_2build.html#autotoc_md18", null ]
      ] ],
      [ "Building with nix", "md_docs_2pages_2build.html#autotoc_md19", [
        [ "Prerequisites", "md_docs_2pages_2build.html#autotoc_md20", null ],
        [ "Consuming via Flake", "md_docs_2pages_2build.html#autotoc_md21", null ],
        [ "Manual Build Commands", "md_docs_2pages_2build.html#autotoc_md22", null ]
      ] ]
    ] ],
    [ "Configuration", "md_docs_2pages_2configuration.html", [
      [ "Nix Home Manager", "md_docs_2pages_2configuration.html#autotoc_md29", null ],
      [ "Configuration Format", "md_docs_2pages_2configuration.html#autotoc_md30", [
        [ "Config", "md_docs_2pages_2configuration.html#autotoc_md31", null ],
        [ "Theme", "md_docs_2pages_2configuration.html#autotoc_md32", null ],
        [ "Input", "md_docs_2pages_2configuration.html#autotoc_md33", null ],
        [ "Key", "md_docs_2pages_2configuration.html#autotoc_md34", null ],
        [ "Render", "md_docs_2pages_2configuration.html#autotoc_md35", null ],
        [ "Colors", "md_docs_2pages_2configuration.html#autotoc_md36", null ],
        [ "Color", "md_docs_2pages_2configuration.html#autotoc_md37", null ],
        [ "Clipboard", "md_docs_2pages_2configuration.html#autotoc_md38", null ],
        [ "ClipboardMode", "md_docs_2pages_2configuration.html#autotoc_md39", null ],
        [ "Session", "md_docs_2pages_2configuration.html#autotoc_md40", null ],
        [ "Shell", "md_docs_2pages_2configuration.html#autotoc_md41", null ],
        [ "Fzf", "md_docs_2pages_2configuration.html#autotoc_md42", null ],
        [ "FzfColors", "md_docs_2pages_2configuration.html#autotoc_md43", null ],
        [ "StatusBar", "md_docs_2pages_2configuration.html#autotoc_md44", null ],
        [ "StatusBarPosition", "md_docs_2pages_2configuration.html#autotoc_md45", null ],
        [ "TabNameSource", "md_docs_2pages_2configuration.html#autotoc_md46", null ],
        [ "StatusBarColors", "md_docs_2pages_2configuration.html#autotoc_md47", null ],
        [ "Terminfo", "md_docs_2pages_2configuration.html#autotoc_md48", null ]
      ] ],
      [ "Default JSON Configuration", "md_docs_2pages_2configuration.html#autotoc_md49", null ]
    ] ],
    [ "Developing", "md_docs_2pages_2developing.html", [
      [ "Nix Dev Shell", "md_docs_2pages_2developing.html#autotoc_md51", [
        [ "Note for Auto-Completion", "md_docs_2pages_2developing.html#autotoc_md52", null ]
      ] ],
      [ "CMake Developer Mode", "md_docs_2pages_2developing.html#autotoc_md53", [
        [ "Presets", "md_docs_2pages_2developing.html#autotoc_md54", null ],
        [ "Configure Build and Test", "md_docs_2pages_2developing.html#autotoc_md55", null ],
        [ "Developer Mode Targets", "md_docs_2pages_2developing.html#autotoc_md56", [
          [ "<span class=\"tt\">coverage</span>", "md_docs_2pages_2developing.html#autotoc_md57", null ],
          [ "<span class=\"tt\">docs</span>", "md_docs_2pages_2developing.html#autotoc_md58", null ]
        ] ]
      ] ],
      [ "Justfile", "md_docs_2pages_2developing.html#autotoc_md59", null ],
      [ "libttx vs ttx", "md_docs_2pages_2developing.html#autotoc_md60", null ],
      [ "Testing", "md_docs_2pages_2developing.html#autotoc_md61", null ],
      [ "Code Style", "md_docs_2pages_2developing.html#autotoc_md62", null ],
      [ "dius Library", "md_docs_2pages_2developing.html#autotoc_md63", null ]
    ] ],
    [ "Install", "md_docs_2pages_2install.html", [
      [ "Installing from GitHub Release", "md_docs_2pages_2install.html#autotoc_md65", [
        [ "Install Script", "md_docs_2pages_2install.html#autotoc_md66", null ]
      ] ],
      [ "Installing with Nix", "md_docs_2pages_2install.html#autotoc_md67", [
        [ "Prerequisites", "md_docs_2pages_2install.html#autotoc_md68", null ],
        [ "Run Without Installation", "md_docs_2pages_2install.html#autotoc_md69", null ],
        [ "Install Using Home Manager (recommended)", "md_docs_2pages_2install.html#autotoc_md70", null ],
        [ "Install Directly (discouraged)", "md_docs_2pages_2install.html#autotoc_md71", null ]
      ] ]
    ] ],
    [ "Terminal Escape Sequences", "md_docs_2pages_2terminal__escape__sequences.html", "md_docs_2pages_2terminal__escape__sequences" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "GitHub", "^https://github.com/coletrammer/ttx", null ]
  ] ]
];

var NAVTREEINDEX =
[
"absolute__position_8h.html",
"classttx_1_1Pane.html#adbf29da17d390524305346fe453f557d",
"classttx_1_1terminal_1_1Commands.html#a75196ad828bb61246aefb7e16895b1fd",
"classttx_1_1terminal_1_1Screen.html#aceefe6b10c3f88c9287e609b9d64275d",
"md_docs_2pages_2configuration.html#autotoc_md48",
"namespacettx.html#a7cf06cab699a7eba09888ab790619c43a7da88eef9c6c59c9eb59e1f2c4ae23bc",
"namespacettx_1_1terminal.html#a4d53e6328b4b65a4cbf44725a054e9cfa5ec9d687f32598ebd8e00d03f736d8df",
"structttx_1_1CreatePaneArgs.html#a25b546cc20e7324b37038fa7a44b5c2c",
"structttx_1_1Size.html#aa2238244fd13ef5d5d46231ed35ccb5e",
"structttx_1_1terminal_1_1Color.html#afad61c29aa585dbdad32afae087b2f53a66e8629ace3c9092107f02f8e5de26b3",
"structttx_1_1terminal_1_1OSC2.html#ae6cd3e022392b744a174fd86afe21a62",
"terminal__input_8h_source.html"
];

var SYNCONMSG = 'click to disable panel synchronization';
var SYNCOFFMSG = 'click to enable panel synchronization';
var LISTOFALLMEMBERS = 'List of all members';