#pragma once

#include "ttx/terminal/capability.h"

namespace ttx::terminal {
constexpr auto ttx_names = di::Array {
    "xterm-ttx"_tsv,
    "ttx"_tsv,
    "ttx terminal multiplexer"_tsv,
};

// These capabilities are sourced from different places, including:
//   terminfo man page: https://manned.org/man/arch/terminfo.5
//   user_caps man page: https://manned.org/man/arch/user_caps.5
//   tmux man page (TERMINFO extensions): https://manned.org/man/arch/user_caps.5
//   and checked against ghostty and kitty's terminfo via `infocmp -x`
//
// The list is sorted alphabetically (mostly) after grouping by value type. This matches the output
// of `infocmp -x` so we can easily compare this list of the output.
constexpr auto ttx_capabilities = di::Array {
    Capability {
        .long_name = "Automatic right margin"_sv,
        .short_name = "am"_tsv,
        .description = "Automatic margins (autowrap enabled by default)"_sv,
    },
    Capability {
        .long_name = "Background character erase"_sv,
        .short_name = "bce"_tsv,
        .description = "Clearing the screens sets the background color, instead of resetting the cell fully"_sv,
    },
    Capability {
        .long_name = "Modifiable palette"_sv,
        .short_name = "ccc"_tsv,
        .description = "Terminal allows modifying the color palette dynamically"_sv,
        // TODO: enable after support this xterm escape (and the kitty version)
        // https://sw.kovidgoyal.net/kitty/color-stack/
        .enabled = false,
    },
    Capability {
        .long_name = "Has status line"_sv,
        .short_name = "hs"_tsv,
        .description = "Has status line (for displaying window title)"_sv,
        .enabled = false, // TODO: enable after implementing OSC 1 (set window title)
    },
    Capability {
        .long_name = "Has meta key"_sv,
        .short_name = "km"_tsv,
        .description = "Keyboard reports include meta key bit on modifiers"_sv,
    },
    Capability {
        .long_name = "No built-in echo"_sv,
        .short_name = "mc5i"_tsv,
        .description = "Terminal won't echo (presumably key presses) automatically"_sv,
    },
    Capability {
        .long_name = "Move in insert mode"_sv,
        .short_name = "mir"_tsv,
        .description = "Cursor can move in insert mode"_sv,
    },
    Capability {
        .long_name = "Move in standout mode"_sv,
        .short_name = "msgr"_tsv,
        .description = "Cursor can move in standout mode (apparently standout mode is inverse (SGR 7))"_sv,
    },
    Capability {
        .long_name = "No pad character"_sv,
        .short_name = "npc"_tsv,
        .description = "? What is a pad character"_sv,
    },
    Capability {
        .long_name = "Newline ignored after 80 cols"_sv,
        .short_name = "xenl"_tsv,
        .description = "? - This is set by xterm"_sv,
    },
    Capability {
        .long_name = "Default colors"_sv,
        .short_name = "AX"_tsv,
        .description = "Supports resetting the foreground/background via SGR 39/49"_sv,
    },
    Capability {
        .long_name = "Colored underlines"_sv,
        .short_name = "Su"_tsv,
        .description = "Supports changing underline color via SGR 58-59"_sv,
    },
    Capability {
        .long_name = "Truecolor"_sv,
        .short_name = "Tc"_tsv,
        .description = "Supports 24 bit true color via SGR 38/38"_sv,
    },
    Capability {
        .long_name = "Xterm extnesions"_sv,
        .short_name = "XT"_tsv,
        .description = "Supports various xterm extensions (tmux uses this to set some default capabilities)"_sv,
    },
    Capability {
        .long_name = "Kitty keyboard protocol"_sv,
        .short_name = "fullkbd"_tsv,
        .description = "Supports kitty keyboard protocol"_sv,
    },
    Capability {
        .long_name = "Maximum colors"_sv,
        .short_name = "colors"_tsv,
        .value = 256u,
        .description = "Number of colors in the palette"_sv,
    },
    Capability {
        .long_name = "Columns"_sv,
        .short_name = "cols"_tsv,
        .value = 80u,
        .description = "Number of columns on screen (this is dynamic)"_sv,
    },
    Capability {
        .long_name = "Initial tab spacing"_sv,
        .short_name = "it"_tsv,
        .value = 8u,
        .description = "Default spacing used for tab characters"_sv,
    },
    Capability {
        .long_name = "Lines"_sv,
        .short_name = "lines"_tsv,
        .value = 24u,
        .description = "Number of lines (rows) on screen (this is dynamic)"_sv,
    },
    Capability {
        .long_name = "Maximum color pairs"_sv,
        .short_name = "pairs"_tsv,
        .value = 0x7fffu, // We use a 16-bit style id internally
        .description = "Number of different graphics renditions which can co-exist on the screen"_sv,
    },
    Capability {
        .long_name = "UTF-8 always"_sv,
        .short_name = "U8"_tsv,
        // TODO: remove this capability after supporting box drawing characters
        .value = 1u,
        .description = "Disable box drawing characters by saying we only support UTF-8"_sv,
    },
    Capability {
        .long_name = "Alternate charset pairs"_sv,
        .short_name = "acsc"_tsv,
        .value = "``aaffggiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~"_tsv, // Magic copied from xterm's terminfo
        .description = "Alternate charset mapping (this is the identity)"_sv,
    },
    Capability {
        .long_name = "Bell"_sv,
        .short_name = "bel"_tsv,
        .value = "^G"_tsv,
        .description = "Bell character - \\a"_sv,
    },
    Capability {
        .long_name = "Blink"_sv,
        .short_name = "blink"_tsv,
        .value = "\\E[5m"_tsv,
        .description = "Set blinking cell via SGR 5"_sv,
    },
    Capability {
        .long_name = "Bold"_sv,
        .short_name = "bold"_tsv,
        .value = "\\E[1m"_tsv,
        .description = "Set bold cell via SGR 1"_sv,
    },
    Capability {
        .long_name = "Shift tab"_sv,
        .short_name = "cbt"_tsv,
        .value = "\\E[Z"_tsv,
        .description = "Terminal sends CSI Z on shift+tab"_sv,
    },
    Capability {
        .long_name = "Invisible cursor"_sv,
        .short_name = "civis"_tsv,
        .value = "\\E[?25l"_tsv,
        .description = "Hide the cursor via CSI ? 25 l"_sv,
    },
    Capability {
        .long_name = "Clear"_sv,
        .short_name = "clear"_tsv,
        .value = "\\E[H\\E[2J"_tsv,
        .description = "Clear the scren by sending CSI H (cursor to 0,0) and CSI 2 J (clear full screen)"_sv,
    },
    Capability {
        .long_name = "Cursor Normal"_sv,
        .short_name = "cnorm"_tsv,
        // We currently don't support mode 12 to control the cursor blinking, but
        // there's no harm in adding it now.
        .value = "\\E[?12h\\E[?25h"_tsv,
        .description = "Reset the cursor by enabling blinking (CSI ? 12 h) and showing the cursor (CSI ? 25 h)"_sv,
    },
    Capability {
        .long_name = "Carriage return"_sv,
        .short_name = "cr"_tsv,
        .value = "\\r"_tsv,
        .description = "Terminal recognizes \\r as carrigate return"_sv,
    },
    Capability {
        .long_name = "Change scroll region"_sv,
        .short_name = "csr"_tsv,
        .value = "\\E[%i%p1%d;%p2%dr"_tsv, // Copied from ghostty/kitty
        .description = "Set vertical scroll region via CSI b; t r"_sv,
    },
    Capability {
        .long_name = "Paramaterized cursor back"_sv,
        .short_name = "cub"_tsv,
        .value = "\\E[%p1%dD"_tsv, // Copied from ghostty/kitty
        .description = "Move cursor left via CSI Ps D"_sv,
    },
    Capability {
        .long_name = "Cursor back"_sv,
        .short_name = "cub1"_tsv,
        .value = "^H"_tsv,
        .description = "Move cursor left 1 via \\b (^H)"_sv,
    },
    Capability {
        .long_name = "Paramaterized cursor down"_sv,
        .short_name = "cud"_tsv,
        .value = "\\E[%p1%dB"_tsv,
        .description = "Move cursor down via CSI Ps B"_sv,
    },
    Capability {
        .long_name = "Cursor down"_sv,
        .short_name = "cud1"_tsv,
        .value = "\\n"_tsv,
        .description = "Move cursor down 1 via \\n"_sv,
    },
    Capability {
        .long_name = "Paramaterized cursor right"_sv,
        .short_name = "cuf"_tsv,
        .value = "\\E[%p1%dC"_tsv,
        .description = "Move cursor right via CSI Ps C"_sv,
    },
    Capability {
        .long_name = "Cursor right"_sv,
        .short_name = "cuf1"_tsv,
        .value = "\\E[C"_tsv,
        .description = "Move cursor right 1 via CSI C"_sv,
    },
    Capability {
        .long_name = "Cursor address"_sv,
        .short_name = "cup"_tsv,
        .value = "\\E[%i%p1%d;%p2%dH"_tsv, // Copied from ghostty/kitty
        .description = "Move cursor to r,c via CSI r; c H"_sv,
    },
    Capability {
        .long_name = "Paramaterized cursor up"_sv,
        .short_name = "cuu"_tsv,
        .value = "\\E[%p1%dA"_tsv,
        .description = "Move cursor up via CSI Ps A"_sv,
    },
    Capability {
        .long_name = "Cursor up"_sv,
        .short_name = "cuu1"_tsv,
        .value = "\\E[A"_tsv,
        .description = "Move cursor up 1 via CSI A"_sv,
    },
    Capability {
        .long_name = "Cursor visible"_sv,
        .short_name = "cvvis"_tsv,
        // This is the same as cnorm except in a single CSI sequence.
        .value = "\\E[?12;25h"_tsv, // copied from ghostty/kitty
        .description = "Make cursor visible via CSI ? 12 ; 25 h"_sv,
    },
    Capability {
        .long_name = "Delete characters"_sv,
        .short_name = "dch"_tsv,
        .value = "\\E[%p1%dP"_tsv,
        .description = "Delete characters via CSI Ps P"_sv,
    },
    Capability {
        .long_name = "Delete character"_sv,
        .short_name = "dch1"_tsv,
        .value = "\\E[P"_tsv,
        .description = "Delete character via CSI P"_sv,
    },
    Capability {
        .long_name = "Dim"_sv,
        .short_name = "dim"_tsv,
        .value = "\\E[2m"_tsv,
        .description = "Dim the cell via SGR 2"_sv,
    },
    Capability {
        .long_name = "Delete lines"_sv,
        .short_name = "dl"_tsv,
        .value = "\\E[%p1%dM"_tsv,
        .description = "Delete lines via CSI Ps M"_sv,
    },
    Capability {
        .long_name = "Delete line"_sv,
        .short_name = "dl1"_tsv,
        .value = "\\E[M"_tsv,
        .description = "Delete line via CSI M"_sv,
    },
    Capability {
        .long_name = "Disable status line"_sv,
        .short_name = "dsl"_tsv,
        .value = R"(\E]2;\E\\)"_tsv,
        .description = "Disable window title via blank OSC 2"_sv,
        // TODO: enable once we support OSC 1/2
        .enabled = false,
    },
    Capability {
        .long_name = "Erase characters"_sv,
        .short_name = "ech"_tsv,
        .value = "\\E[%p1%dX"_tsv,
        .description = "Erase characters via CSI Ps X"_sv,
    },
    Capability {
        .long_name = "Erase display"_sv,
        .short_name = "ed"_tsv,
        .value = "\\E[J"_tsv,
        .description = "Erase to screen end via CSI J"_sv,
    },
    Capability {
        .long_name = "Erase line"_sv,
        .short_name = "el"_tsv,
        .value = "\\E[K"_tsv,
        .description = "Erase to line end via CSI K"_sv,
    },
    Capability {
        .long_name = "Erase line beginning"_sv,
        .short_name = "el1"_tsv,
        .value = "\\E[1K"_tsv,
        .description = "Erase to beginning of line end via CSI 1 K"_sv,
    },
    Capability {
        .long_name = "Flash"_sv,
        .short_name = "flash"_tsv,
        .value = "\\E[?5h$<100/>\\E[?5l"_tsv, // Copied from ghostty/kitty
        .description = "Flash screen via enabling/disabling video reverse mode (CSI ? 5 h/l), after 100 ms"_sv,
    },
    Capability {
        .long_name = "From status line"_sv,
        .short_name = "fsl"_tsv,
        .value = "^G"_tsv,
        .description = "Terminate OSC sequence via \\a (^G)"_sv,
        // TODO: enable after supporting OSC 2
        .enabled = false,
    },
    Capability {
        .long_name = "Home"_sv,
        .short_name = "home"_tsv,
        .value = "\\E[H"_tsv,
        .description = "Move cursor to home via CSI H"_sv,
    },
    Capability {
        .long_name = "Horizontal position absolute"_sv,
        .short_name = "hpa"_tsv,
        .value = "\\E[%i%p1%dG"_tsv,
        .description = "Set cursor col to n via CSI n G"_sv,
    },
    Capability {
        .long_name = "Horizontal tab"_sv,
        .short_name = "ht"_tsv,
        .value = "^I"_tsv,
        .description = "Terminal recognizes tab as \\t (^I)"_sv,
    },
    Capability {
        .long_name = "Horizontal tab set"_sv,
        .short_name = "hts"_tsv,
        .value = "\\EH"_tsv,
        .description = "Set horizontal tab via CI HTS (ESC H)"_sv,
    },
    Capability {
        .long_name = "Insert characters"_sv,
        .short_name = "ich"_tsv,
        .value = "\\E[%p1%d@"_tsv,
        .description = "Insert characters via CSI Ps @"_sv,
    },
    Capability {
        .long_name = "Insert character"_sv,
        .short_name = "ich1"_tsv,
        .value = "\\E[@"_tsv,
        .description = "Insert character via CSI @"_sv,
    },
    Capability {
        .long_name = "Insert lines"_sv,
        .short_name = "il"_tsv,
        .value = "\\E[%p1%dL"_tsv,
        .description = "Insert lines via CSI Ps L"_sv,
    },
    Capability {
        .long_name = "Insert line"_sv,
        .short_name = "il1"_tsv,
        .value = "\\E[L"_tsv,
        .description = "Insert line via CSI L"_sv,
    },
    Capability {
        .long_name = "Index"_sv,
        .short_name = "ind"_tsv,
        .value = "\\n"_tsv,
        .description = "Scroll text down via \\n (we auto-scroll when moving the cursor down via \\n)"_sv,
    },
    Capability {
        .long_name = "Scroll up"_sv,
        .short_name = "indn"_tsv,
        .value = "\\E[%p1%dS"_tsv,
        .description = "Scroll up via CSI Ps S"_sv,
    },
    Capability {
        .long_name = "Initialize color"_sv,
        .short_name = "initc"_tsv,
        .value =
            R"(\E]4;%p1%d;rgb:%p2%{255}%*%{1000}%/%2.2X/%p3%{255}%*%{1000}%/%2.2X/%p4%{255}%*%{1000}%/%2.2X\E\\)"_tsv,
        .description = "Initialize color value via OSC 4"_sv,
        // TODO: enable once we support setting color palette via OSC 4
        .enabled = false,
    },
    Capability {
        .long_name = "Invisible"_sv,
        .short_name = "invis"_tsv,
        .value = "\\E[8m"_tsv,
        .description = "Make cell invisible via CSI 8"_sv,
    },

// Key board related capabiltiies. It would be good to have this in sync with our parser/generator.
#define KEY_CAP(short, v, name)                                              \
    Capability {                                                             \
        .long_name = name ""_sv,                                             \
        .short_name = short ""_tsv,                                          \
        .value = v ""_tsv,                                                   \
        .description = "Escape terminal sends when " #name " is pressed"_sv, \
    },

#define KEYS(M)                                 \
    M("kBEG", "\\E[1;2E", "Shift+Begin")        \
    M("kDC", "\\E[3;2~", "Shift+Delete")        \
    M("kEND", "\\E[1;2F", "Shift+End")          \
    M("kHOM", "\\E[1;2H", "Shift+Home")         \
    M("kIC", "\\E[2;2~", "Shift+Insert")        \
    M("kLFT", "\\E[1;2D", "Shift+Left")         \
    M("kNXT", "\\E[6;2~", "Shift+PageDown")     \
    M("kPRV", "\\E[5;2~", "Shift+PageUp")       \
    M("kRIT", "\\E[1;2C", "Shift+Right")        \
    M("kbeg", "\\EOE", "Begin")                 \
    M("kbs", "^?", "Backspace")                 \
    M("kcbt", "\\E[Z", "Shift+Tab")             \
    M("kcub1", "\\EOD", "Left")                 \
    M("kcud1", "\\EOB", "Down")                 \
    M("kcuf1", "\\EOC", "Right")                \
    M("kcuu1", "\\EOA", "Up")                   \
    M("kdch1", "\\E[3~", "Delete")              \
    M("kend", "\\EOF", "End")                   \
    M("kf1", "\\EOP", "F1")                     \
    M("kf10", "\\E[21~", "F10")                 \
    M("kf11", "\\E[23~", "F11")                 \
    M("kf12", "\\E[24~", "F12")                 \
    M("kf13", "\\E[1;2P", "Shift+F1")           \
    M("kf14", "\\E[1;2Q", "Shift+F2")           \
    M("kf15", "\\E[1;2R", "Shift+F3")           \
    M("kf16", "\\E[1;2S", "Shift+F4")           \
    M("kf17", "\\E[15;2~", "Shift+F5")          \
    M("kf18", "\\E[17;2~", "Shift+F6")          \
    M("kf19", "\\E[18;2~", "Shift+F7")          \
    M("kf2", "\\EOQ", "F2")                     \
    M("kf20", "\\E[19;2~", "Shift+F8")          \
    M("kf21", "\\E[20;2~", "Shift+F9")          \
    M("kf22", "\\E[21;2~", "Shift+F10")         \
    M("kf23", "\\E[23;2~", "Shift+F11")         \
    M("kf24", "\\E[24;2~", "Shift+F12")         \
    M("kf25", "\\E[1;5P", "Control+F1")         \
    M("kf26", "\\E[1;5Q", "Control+F2")         \
    M("kf27", "\\E[1;5R", "Control+F3")         \
    M("kf28", "\\E[1;5S", "Control+F4")         \
    M("kf29", "\\E[15;5~", "Control+F5")        \
    M("kf3", "\\EOR", "F3")                     \
    M("kf30", "\\E[17;5~", "Control+F6")        \
    M("kf31", "\\E[18;5~", "Control+F7")        \
    M("kf32", "\\E[19;5~", "Control+F8")        \
    M("kf33", "\\E[20;5~", "Control+F9")        \
    M("kf34", "\\E[21;5~", "Control+F10")       \
    M("kf35", "\\E[23;5~", "Control+F11")       \
    M("kf36", "\\E[24;5~", "Control+F12")       \
    M("kf37", "\\E[1;6P", "Control+Shift+F1")   \
    M("kf38", "\\E[1;6Q", "Control+Shift+F2")   \
    M("kf39", "\\E[1;6R", "Control+Shift+F3")   \
    M("kf4", "\\EOS", "F4")                     \
    M("kf40", "\\E[1;6S", "Control+Shift+F4")   \
    M("kf41", "\\E[15;6~", "Control+Shift+F5")  \
    M("kf42", "\\E[17;6~", "Control+Shift+F6")  \
    M("kf43", "\\E[18;6~", "Control+Shift+F7")  \
    M("kf44", "\\E[19;6~", "Control+Shift+F8")  \
    M("kf45", "\\E[20;6~", "Control+Shift+F9")  \
    M("kf46", "\\E[21;6~", "Control+Shift+F10") \
    M("kf47", "\\E[23;6~", "Control+Shift+F11") \
    M("kf48", "\\E[24;6~", "Control+Shift+F12") \
    M("kf49", "\\E[1;3P", "Alt+F1")             \
    M("kf5", "\\E[15~", "F5")                   \
    M("kf50", "\\E[1;3Q", "Alt+F2")             \
    M("kf51", "\\E[1;3R", "Alt+F3")             \
    M("kf52", "\\E[1;3S", "Alt+F4")             \
    M("kf53", "\\E[15;3~", "Alt+F5")            \
    M("kf54", "\\E[17;3~", "Alt+F6")            \
    M("kf55", "\\E[18;3~", "Alt+F7")            \
    M("kf56", "\\E[19;3~", "Alt+F8")            \
    M("kf57", "\\E[20;3~", "Alt+F9")            \
    M("kf58", "\\E[21;3~", "Alt+F10")           \
    M("kf59", "\\E[23;3~", "Alt+F11")           \
    M("kf6", "\\E[17~", "F6")                   \
    M("kf60", "\\E[24;3~", "Alt+F12")           \
    M("kf61", "\\E[1;4P", "Alt+Shift+F1")       \
    M("kf62", "\\E[1;4Q", "Alt+Shift+F2")       \
    M("kf63", "\\E[1;4R", "Alt+Shift+F3")       \
    M("kf64", "\\E[1;4S", "Alt+Shift+F4")       \
    M("kf7", "\\E[18~", "F7")                   \
    M("kf8", "\\E[19~", "F8")                   \
    M("kf9", "\\E[20~", "F9")                   \
    M("khome", "\\EOH", "Home")                 \
    M("kich1", "\\E[2~", "Insert")              \
    M("kind", "\\E[1;2B", "Shift+Down")         \
    M("kmous", "\\E[M", "Mouse")                \
    M("knp", "\\E[6~", "PageDown")              \
    M("kpp", "\\E[5~", "PageUp")                \
    M("kri", "\\E[1;2A", "Shift+Up")

    KEYS(KEY_CAP)
    //

    Capability {
        .long_name = "Original colors"_sv,
        .short_name = "oc"_tsv,
        .value = "\\E]104\\007"_tsv,
        .description = "Reset color palette via OSC 104"_sv,
        // TODO: enable once we support OSC 104 to set the palette
        .enabled = false,
    },
    Capability {
        .long_name = "Original pair"_sv,
        .short_name = "op"_tsv,
        .value = "\\E[39;49m"_tsv,
        .description = "Reset grahics rendition fg/bg via CSI 39;49 m"_sv,
    },
    Capability {
        .long_name = "Restore cursor"_sv,
        .short_name = "rc"_tsv,
        .value = "\\E8"_tsv,
        .description = "Reset cursor via ESC 8 (DECRC)"_sv,
    },
    Capability {
        .long_name = "Repeat character"_sv,
        .short_name = "rep"_tsv,
        .value = "%p1%c\\E[%p2%{1}%-%db"_tsv, // Copied from ghostty/kitty
        .description = "Repeat character via CSI Ps b"_sv,
    },
    Capability {
        .long_name = "Reverse video"_sv,
        .short_name = "rev"_tsv,
        .value = "\\E[7m"_tsv,
        .description = "Invert cell via SGR 7"_sv,
    },
    Capability {
        .long_name = "Reverse index"_sv,
        .short_name = "ri"_tsv,
        .value = "\\EM"_tsv,
        .description = "Reverse index (scroll down) via C1 RI (ESC M)"_sv,
    },
    Capability {
        .long_name = "Scroll down"_sv,
        .short_name = "rin"_tsv,
        .value = "\\E[%p1%dT"_tsv,
        .description = "Scroll down via CSI Ps T"_sv,
    },
    Capability {
        .long_name = "Italic"_sv,
        .short_name = "ritm"_tsv,
        .value = "\\E[23m"_tsv,
        .description = "Italicize cell via SGR 23"_sv,
    },
    Capability {
        .long_name = "End alternate character set"_sv,
        .short_name = "rmacs"_tsv,
        .value = "\\E(B"_tsv,
        .description = "End altnerate character set via ESC ( B"_sv,
        // TODO: enable once box drawing charset is supported
        .enabled = false,
    },
    Capability {
        .long_name = "Reset automatic margins"_sv,
        .short_name = "rmam"_tsv,
        .value = "\\E[?7l"_tsv,
        .description = "Disable auto-wrap via CSI ? 7 l"_sv,
    },
    Capability {
        .long_name = "Reset alternate screen"_sv,
        .short_name = "rmcup"_tsv,
        .value = "\\E[?1049l"_tsv,
        .description = "Leave alternate screen mode via CSI ? 1049 l"_sv,
    },
    Capability {
        .long_name = "Exit insert mode"_sv,
        .short_name = "rmir"_tsv,
        .value = "\\E[4l"_tsv,
        .description = "Leave insert mode via CSI 4 l"_sv,
        // TODO: enable once insert mode is supported
        .enabled = false,
    },
    Capability {
        .long_name = "Exit keyboard transmit mode"_sv,
        .short_name = "rmkx"_tsv,
        .value = "\\E[?1l\\E>"_tsv,
        // We don't yet support alternate keypad mode, but its safe to put the sequence in.
        .description = "Reset keyboard modes via CSI ? 1 l (cursor keys) and ESC > (alternate keypad mode)"_sv,
    },
    Capability {
        .long_name = "Exit standout mode"_sv,
        .short_name = "rmso"_tsv,
        .value = "\\E[27m"_tsv,
        .description = "Exit standout mode via SGR 27 (clears inverted graphics rendition)"_sv,
    },
    Capability {
        .long_name = "Exit underline"_sv,
        .short_name = "rmul"_tsv,
        .value = "\\E[24m"_tsv,
        .description = "Exit underline via SGR 24"_sv,
    },
    Capability {
        .long_name = "Reset string"_sv,
        .short_name = "rs1"_tsv,
        // We don't yet support ESC c (RIS), but its safe to include.
        .value = R"(\E]\E\\\Ec)"_tsv, // Copied from ghostty/kitty
        .description = "Reset via empty OSC sequence followed by ESC c (full reset)"_sv,
    },
    Capability {
        .long_name = "Set cursor"_sv,
        .short_name = "sc"_tsv,
        .value = "\\E7"_tsv,
        .description = "Save the cursor via ESC 7"_sv,
    },
    Capability {
        .long_name = "Set background color"_sv,
        .short_name = "setab"_tsv,
        .value = "\\E[%?%p1%{8}%<%t4%p1%d%e%p1%{16}%<%t10%p1%{8}%-%d%e48;5;%p1%d%;m"_tsv, // Copied from ghostty/kitty
        .description = "Set the graphics background via CSI 48"_sv,
    },
    Capability {
        .long_name = "Set foreground color"_sv,
        .short_name = "setaf"_tsv,
        .value = "\\E[%?%p1%{8}%<%t3%p1%d%e%p1%{16}%<%t9%p1%{8}%-%d%e38;5;%p1%d%;m"_tsv, // Copied from ghostty/kitty
        .description = "Set the graphics foregroup via CSI 38"_sv,
    },
    Capability {
        .long_name = "Set graphics rendition"_sv,
        .short_name = "sgr"_tsv,
        // TODO: since we don't yet support box drawing characters I removed the first conditional.
        // .value =
        //     "%?%p9%t\\E(0%e\\E(B%;\\E[0%?%p6%t;1%;%?%p5%t;2%;%?%p2%t;4%;%?%p1%p3%|%t;7%;%?%p4%t;5%;%?%p7%t;8%;m"_sv,
        //     Copied from xterm
        .value = "\\E[0%?%p6%t;1%;%?%p5%t;2%;%?%p2%t;4%;%?%p1%p3%|%t;7%;%?%p4%t;5%;%?%p7%t;8%;m"_tsv,
        .description = "Set the graphics rendition via CSI m, and charset via ESC ( C"_sv,

    },
    Capability {
        .long_name = "Reset graphics rendition"_sv,
        .short_name = "sgr0"_tsv,
        /// .value = "\\E(B\\E[m"_tsv, // Copied from ghostty/kitty
        // TODO: since we don't yet support box drawing characters I removed the first conditional.
        .value = "\\E[m"_tsv,
        .description = "Reset the graphics rendition via CSI m and charset via ESC ( B"_sv,

    },
    Capability {
        .long_name = "Set italics"_sv,
        .short_name = "sitm"_tsv,
        .value = "\\E[3m"_tsv,
        .description = "Set italics via SGR 3"_sv,
    },
    Capability {
        .long_name = "Enter alternate charset"_sv,
        .short_name = "smacs"_tsv,
        .value = "\\E(0"_tsv,
        .description = "Enter box drawing charset via ESC ( 0"_sv,
        // TODO: enable once we support box drawing charset
        .enabled = false,
    },
    Capability {
        .long_name = "Set automatic margins"_sv,
        .short_name = "smam"_tsv,
        .value = "\\E[?7h"_tsv,
        .description = "Enable auto-wrap via CSI ? 7 h"_sv,
    },
    Capability {
        .long_name = "Set alternate screen"_sv,
        .short_name = "smcup"_tsv,
        .value = "\\E[?1049h"_tsv,
        .description = "Enter alternate screen mode via CSI ? 1049 h"_sv,
    },
    Capability {
        .long_name = "Enter insert mode"_sv,
        .short_name = "smir"_tsv,
        .value = "\\E[4h"_tsv,
        .description = "Enter insert mode via CSI 4 h"_sv,
        // TODO: enable once insert mode is supported
        .enabled = false,
    },
    Capability {
        .long_name = "Enter keyboard transmit mode"_sv,
        .short_name = "smkx"_tsv,
        .value = "\\E[?1h\\E="_tsv,
        // We don't yet support alternate keypad mode, but its safe to put the sequence in.
        .description = "Enter keyboard modes via CSI ? 1 h (cursor keys) and ESC = (alternate keypad mode)"_sv,
    },
    Capability {
        .long_name = "Enter standout mode"_sv,
        .short_name = "smso"_tsv,
        .value = "\\E[7m"_tsv,
        .description = "Enter standout mode via SGR 7 (inverted graphics rendition)"_sv,
    },
    Capability {
        .long_name = "Enter underline"_sv,
        .short_name = "smul"_tsv,
        .value = "\\E[4m"_tsv,
        .description = "Enter underline via SGR 4"_sv,
    },
    Capability {
        .long_name = "Clear all tabs"_sv,
        .short_name = "tbc"_tsv,
        .value = "\\E[3g"_tsv,
        .description = "Clear all tabstops via CSI 3 g"_sv,
    },
    Capability {
        .long_name = "Move to status line"_sv,
        .short_name = "tsl"_tsv,
        .value = "\\E]2;"_tsv,
        .description = "Enter status line (window title) via OSC 2"_sv,
        // TODO: enable after supporting OSC 1/2
        .enabled = false,
    },
    Capability {
        .long_name = "User string 6"_sv,
        .short_name = "u6"_tsv,
        .value = "\\E[%i%d;%dR"_tsv, // Copied from ghostty/kitty
        .description = "String format of cursor position reports CSI Ps ; Ps R"_sv,
    },
    Capability {
        .long_name = "User string 7"_sv,
        .short_name = "u7"_tsv,
        .value = "\\E[6n"_tsv, // Copied from ghostty/kitty
        .description = "Device status report (cursor position) via CSI 6 n"_sv,
    },
    Capability {
        .long_name = "User string 8"_sv,
        .short_name = "u8"_tsv,
        .value = "\\E[?%[;0123456789]c"_tsv, // Copied from ghostty/kitty
        .description = "String format of primary device attributes response - CSI ? Ps c"_sv,
    },
    Capability {
        .long_name = "User string 9"_sv,
        .short_name = "u9"_tsv,
        .value = "\\E[c"_tsv, // Copied from ghostty/kitty
        .description = "Device primary attributes (DA1) via CSI c"_sv,
    },
    Capability {
        .long_name = "Vertical position absolute"_sv,
        .short_name = "vpa"_tsv,
        .value = "\\E[%i%p1%dd"_tsv,
        .description = "Set cursor vertical position via CSI Ps d"_sv,
    },
    Capability {
        .long_name = "Leave bracketed paste"_sv,
        .short_name = "BD"_tsv,
        .value = "\\E[?2004l"_tsv,
        .description = "Leave bracketed paste via CSI ? 2004 l"_sv,
    },
    Capability {
        .long_name = "Enter bracketed paste"_sv,
        .short_name = "BE"_tsv,
        .value = "\\E[?2004h"_tsv,
        .description = "Enter bracketed paste via CSI ? 2004 h"_sv,
    },
    Capability {
        .long_name = "Reset horizontal margins"_sv,
        .short_name = "Clmg"_tsv,
        .value = "\\E[s"_tsv,
        .description = "Reset horizontal margins via CSI s"_sv,
        // TODO: enable with support for horizontal margins
        .enabled = false,
    },
    Capability {
        .long_name = "Set horizontal margins"_sv,
        .short_name = "Cmg"_tsv,
        .value = "\\E[%i%p1%d;%p2%ds"_tsv,
        .description = "Set horizontal margins via CSI Ps ; Ps s"_sv,
        // TODO: enable with support for horizontal margins
        .enabled = false,
    },
    Capability {
        .long_name = "Reset cursor color"_sv,
        .short_name = "Cr"_tsv,
        .value = "\\E]112\\007"_tsv, // Copied from kitty
        .description = "Reset cursor palette color via OSC 112"_sv,
        // TODO: enable after support dynamic palette (OSC 12+112)
        .enabled = false,
    },
    Capability {
        .long_name = "Set cursor color"_sv,
        .short_name = "Cs"_tsv,
        .value = "\\E]12;%p1%s\\007"_tsv, // Copied from kitty
        .description = "Set cursor palette color via OSC 12"_sv,
        // TODO: enable after support dynamic palette (OSC 12+112)
        .enabled = false,
    },
    Capability {
        .long_name = "Disable horizontal margins"_sv,
        .short_name = "Dsmg"_tsv,
        .value = "\\E[?69l"_tsv,
        .description = "Disable horizontal margin mode via CSI ? 69 l"_sv,
        // TODO: enable with support for horizontal margins
        .enabled = false,
    },
    Capability {
        .long_name = "Clear with scroll back"_sv,
        .short_name = "E3"_tsv,
        .value = "\\E[3J"_tsv,
        .description = "Clear screen including scroll back via CSI 3 J"_sv,
    },
    Capability {
        .long_name = "Enable horizontal margins"_sv,
        .short_name = "Enmg"_tsv,
        .value = "\\E[?69h"_tsv,
        .description = "Enable horizontal margin mode via CSI ? 69 h"_sv,
        // TODO: enable with support for horizontal margins
        .enabled = false,
    },
    Capability {
        .long_name = "Save clipboard"_sv,
        .short_name = "Ms"_tsv,
        .value = "\\E]52;%p1%s;%p2%s\\007"_tsv, // Copied from ghostty/kitty
        .description = "Set clipboard via OSC 52"_sv,
    },
    Capability {
        .long_name = "Bracketed paste end"_sv,
        .short_name = "PE"_tsv,
        .value = "\\E[201~"_tsv,
        .description = "Terminal uses CSI 201 ~ to end a bracketed paste"_sv,
    },
    Capability {
        .long_name = "Bracketed paste start"_sv,
        .short_name = "PS"_tsv,
        .value = "\\E[200~"_tsv,
        .description = "Terminal uses CSI 200 ~ to start a bracketed paste"_sv,
    },
    Capability {
        .long_name = "Report version"_sv,
        .short_name = "RV"_tsv,
        .value = "\\E[>c"_tsv,
        .description = "Request secondary device attributes via CSI > c"_sv,
    },
    Capability {
        .long_name = "Reset cursor style"_sv,
        .short_name = "Se"_tsv,
        .value = "\\E[2 q"_tsv,
        .description = "Reset cursor style via CSI 2 SP q (steady block cursor)"_sv,
    },
    Capability {
        .long_name = "Set underline color"_sv,
        .short_name = "Setulc"_tsv,
        .value = "\\E[58:2:%p1%{65536}%/%d:%p1%{256}%/%{255}%&%d:%p1%{255}%&%d%;m"_tsv, // Copied from ghostty/kitty
        .description = "Set underline color via SGR 58"_sv,
    },
    Capability {
        .long_name = "Set extended underline"_sv,
        .short_name = "Smulx"_tsv,
        .value = "\\E[4:%p1%dm"_tsv, // Copied from ghostty/kitty
        .description = "Set extended unline mode via SGR 4:Ps"_sv,
    },
    Capability {
        .long_name = "Set cursor style"_sv,
        .short_name = "Ss"_tsv,
        .value = "\\E[%p1%d q"_tsv,
        .description = "Set cursor style via CSI Ps SP q"_sv,
    },
    Capability {
        .long_name = "Synchronized output"_sv,
        .short_name = "Sync"_tsv,
        .value = "\\E[?2026%?%p1%{1}%-%tl%eh%;"_tsv, // Copied from ghostty (kitty uses a DCS sequence)
        .description = "Toggle synchronized output via CSI ? 2026 h/l"_sv,
    },
    Capability {
        .long_name = "Extended mouse"_sv,
        .short_name = "XM"_tsv,
        .value = "\\E[?1006;1000%?%p1%{1}%=%th%el%;"_tsv, // Copied from ghostty
        .description = "Toggle SGR mouse mode via CSI ? 1006 ; 1000 h/l"_sv,
    },
    Capability {
        .long_name = "Extended version"_sv,
        .short_name = "XR"_tsv,
        .value = "\\E[>0q"_tsv, // Copied from ghostty
        .description = "Request XTVERSION via CSI > 0 q"_sv,
        // TODO: enable after supporting XTVERSION
        .enabled = false,
    },
    Capability {
        .long_name = "Reset focus reports"_sv,
        .short_name = "fd"_tsv,
        .value = "\\E[?1004l"_tsv,
        .description = "Reset focus reports via CSI ? 1004 l"_sv,
    },
    Capability {
        .long_name = "Set focus reports"_sv,
        .short_name = "fe"_tsv,
        .value = "\\E[?1004h"_tsv,
        .description = "Set focus reports via CSI ? 1004 h"_sv,
    },

#define KEYS2(M)                                     \
    M("kBEG3", "\\E[1;3E", "Alt+Begin")              \
    M("kBEG4", "\\E[1;4E", "Alt+Shift+Begin")        \
    M("kBEG5", "\\E[1;5E", "Control+Begin")          \
    M("kBEG6", "\\E[1;6E", "Control+Shift+Begin")    \
    M("kBEG7", "\\E[1;7E", "Control+Alt+Begin")      \
    M("kDC3", "\\E[3;3~", "Alt+Delete")              \
    M("kDC4", "\\E[3;4~", "Alt+Shift+Delete")        \
    M("kDC5", "\\E[3;5~", "Control+Delete")          \
    M("kDC6", "\\E[3;6~", "Control+Shift+Delete")    \
    M("kDC7", "\\E[3;7~", "Control+Alt+Delete")      \
    M("kDN", "\\E[1;2B", "Shift+Down")               \
    M("kDN3", "\\E[1;3B", "Alt+Down")                \
    M("kDN4", "\\E[1;4B", "Alt+Shift+Down")          \
    M("kDN5", "\\E[1;5B", "Control+Down")            \
    M("kDN6", "\\E[1;6B", "Control+Shift+Down")      \
    M("kDN7", "\\E[1;7B", "Control+Alt+Down")        \
    M("kEND3", "\\E[1;3F", "Alt+End")                \
    M("kEND4", "\\E[1;4F", "Alt+Shift+End")          \
    M("kEND5", "\\E[1;5F", "Control+End")            \
    M("kEND6", "\\E[1;6F", "Control+Shift+End")      \
    M("kEND7", "\\E[1;7F", "Control+Alt+End")        \
    M("kHOM3", "\\E[1;3H", "Alt+Home")               \
    M("kHOM4", "\\E[1;4H", "Alt+Shift+Home")         \
    M("kHOM5", "\\E[1;5H", "Control+Home")           \
    M("kHOM6", "\\E[1;6H", "Control+Shift+Home")     \
    M("kHOM7", "\\E[1;7H", "Control+Alt+Home")       \
    M("kIC3", "\\E[2;3~", "Alt+Insert")              \
    M("kIC4", "\\E[2;4~", "Alt+Shift+Insert")        \
    M("kIC5", "\\E[2;5~", "Control+Insert")          \
    M("kIC6", "\\E[2;6~", "Control+Shift+Insert")    \
    M("kIC7", "\\E[2;7~", "Control+Alt+Insert")      \
    M("kLFT3", "\\E[1;3D", "Alt+Left")               \
    M("kLFT4", "\\E[1;4D", "Alt+Shift+Left")         \
    M("kLFT5", "\\E[1;5D", "Control+Left")           \
    M("kLFT6", "\\E[1;6D", "Control+Shift+Left")     \
    M("kLFT7", "\\E[1;7D", "Control+Alt+Left")       \
    M("kNXT3", "\\E[6;3~", "Alt+PageDown")           \
    M("kNXT4", "\\E[6;4~", "Alt+Shift+PageDown")     \
    M("kNXT5", "\\E[6;5~", "Control+PageDown")       \
    M("kNXT6", "\\E[6;6~", "Control+Shift+PageDown") \
    M("kNXT7", "\\E[6;7~", "Control+Alt+PageDown")   \
    M("kPRV3", "\\E[5;3~", "Alt+PageUp")             \
    M("kPRV4", "\\E[5;4~", "Alt+Shift+PageUp")       \
    M("kPRV5", "\\E[5;5~", "Control+PageUp")         \
    M("kPRV6", "\\E[5;6~", "Control+Shift+PageUp")   \
    M("kPRV7", "\\E[5;7~", "Control+Alt+PageUp")     \
    M("kRIT3", "\\E[1;3C", "Alt+Right")              \
    M("kRIT4", "\\E[1;4C", "Alt+Shift+Right")        \
    M("kRIT5", "\\E[1;5C", "Control+Right")          \
    M("kRIT6", "\\E[1;6C", "Control+Shift+Right")    \
    M("kRIT7", "\\E[1;7C", "Control+Alt+Right")      \
    M("kUP", "\\E[1;2A", "Shift+Up")                 \
    M("kUP3", "\\E[1;3A", "Alt+Up")                  \
    M("kUP4", "\\E[1;4A", "Alt+Shift+Up")            \
    M("kUP5", "\\E[1;5A", "Control+Up")              \
    M("kUP6", "\\E[1;6A", "Control+Shift+Up")        \
    M("kUP7", "\\E[1;7A", "Control+Alt+Up")

    KEYS2(KEY_CAP)
    //

    Capability {
        .long_name = "Focus in"_sv,
        .short_name = "kxIN"_tsv,
        .value = "\\E[I"_tsv,
        .description = "Report send by terminal when gaining focus - CSI I"_sv,
    },
    Capability {
        .long_name = "Focus out"_sv,
        .short_name = "kxOUT"_tsv,
        .value = "\\E[O"_tsv,
        .description = "Report send by terminal when losing focus - CSI o"_sv,
    },
    Capability {
        .long_name = "Reset strikethrough"_sv,
        .short_name = "rmxx"_tsv,
        .value = "\\E[29m"_tsv,
        .description = "Reset strikethrough cell via SGR 29"_sv,
    },
    Capability {
        .long_name = "Report version response"_sv,
        .short_name = "rv"_tsv,
        // NOTE: this value looks wrong, as the response should include a '<' after the CSI. However,
        // both ghostty and xterm leave it off. Probably no one uses this string.
        .value = R"(\E\\[[0-9]+;[0-9]+;[0-9]+c)"_tsv, // Copied from ghostty
        .description = "String format of device secondary attirbutes response - CSI < Ps ; Ps ; Ps c"_sv,
    },
    Capability {
        .long_name = "Set RGB background"_sv,
        .short_name = "setrgbb"_tsv,
        .value = "\\E[48:2:%p1%d:%p2%d:%p3%dm"_tsv, // Copied from ghostty/kitty
        .description = "Set RGB background via SGR 48"_sv,
    },
    Capability {
        .long_name = "Set RGB foreground"_sv,
        .short_name = "setrgbf"_tsv,
        .value = "\\E[38:2:%p1%d:%p2%d:%p3%dm"_tsv, // Copied from ghostty/kitty
        .description = "Set RGB foreground via SGR 38"_sv,
    },
    Capability {
        .long_name = "Strikethrough"_sv,
        .short_name = "smxx"_tsv,
        .value = "\\E[9m"_tsv,
        .description = "Set strikethrough cell via SGR 9"_sv,
    },
    Capability {
        .long_name = "Extended mouse report"_sv,
        .short_name = "xm"_tsv,
        .value = "\\E[<%i%p3%d;%p1%d;%p2%d;%?%p4%tM%em%;"_tsv, // Copied from ghostty
        .description = "Format of extended mouse reports - CSI Ps ; Ps ; Ps M/m"_sv,
    },
    Capability {
        .long_name = "Extended version report"_sv,
        .short_name = "xr"_tsv,
        .value = R"(\EP>\|[ -~]+a\E\\)"_tsv, // Copied from ghostty
        .description = "Format of XTVERSION response - DCS > version ST"_sv,
        // TODO: enable after supporting XTVERSION
        .enabled = false,
    },
};

constexpr auto ttx_terminfo = Terminfo {
    .names = ttx_names,
    .capabilities = ttx_capabilities,
};
}
