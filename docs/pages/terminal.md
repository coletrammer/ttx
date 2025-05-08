# Terminal

Terminal is modelled as a grid of cells. In a naive implementation, each cell has its own graphics attributes
and text. However, in practice, this data model breaks down because of a number of requirements around handling
unicode text and certain terminal features.

## Terminal Features

1. Graphics Rendition (CSI m) - Each individual cell has its own graphics rendition.
1. Hyperlinks (OSC 8) - Each individual cell may have an associated hyperlink.
1. Text - Each cell has some associated text.
   1. Because of Unicode zero-width characters, a single cell can consist of multiple code points.
   1. Certain code points have a width of 2, and thus occupy two cell.
   1. Terminals usually measure text width naively, but can also use grapheme clustering to determine width (DECSET 2027)
   1. Applications can specify arbitrary text width via text sizing protocol (OSC 66)
   1. Old terminal extensions allow specifying double width lines (DECDWL)
1. Cell height considerations - Applications can request cells to have logical height > 1
   1. Text sizing protocol allows specifying up to 7 cells in height (OSC 66), as well as fractional scaling attributes
   1. Old termination extension for double height line (DECDHL)
1. Images - certain protocols enable terminal images
   1. Kitty image protocol (APC G) allows placement of images referencing cells indirectly, or directly via the
      passthrough mode
   1. Sixels are probably storing data within cells
   1. Iterm image protocol places image indirectly
1. Random OSCs - these either associate arbitrary metadata with the terminal or request some sort of UI action
   1. Window icon/name (OSC 0,1,2) - functionally just modify an attribute of the terminal
   1. Working directory (OSC 7) - this is just setting an attribute, although it is useful for controlling the initial
      CWD in new panes
   1. Clipboard (OSC 52) - this needs to be passed through to the outer application
   1. Shell prompt regions (OSC 133) - this is metadata associated with the terminal
   1. Desktop notifications (OSC 9,99) - pass through to outer application
   1. Color scheme information (OSC 4,10,11) - probably metadata associated with the terminal, maybe this should be
      global across all panes.

## Terminal Operations

Some operations are controlled by the process running in the terminal (escape sequences), while others are controlled by
the user (scrolling).

1. Editing text in cell (this is obvious and the most common operation)
1. Setting graphics rendition of a cell
1. Cursor movement
1. Inserting/deleting characters (this can be done via escape sequences) - this shifts over the contents of cells on a
   line
1. Inserting/Deleting a line (this can be done via escape sequences) - this is like scrolling except only affects
   visible cells
1. Scrolling - this conceptually doesn't change the active contents of terminal cells, but does affect what gets
   rendered
1. Resizing - when modifying the horizontal size we want to reflow text (lines which overflowed may become a single line)

## Data Structure

Because attributes like hyperlinks and the graphics rendition will be shared across cells, it makes sense to refer to
these indirectly in a cell (if we store cells at all). This means there would be map from id to value and each cell
would have an id for each of these attributes.

For text, we can store the size of the associated text for each cell. This allows for arbitrary width cells with an
arbitrary amount of text, while still be reasonably efficient. The cursor will cache the actual text offset for each
cell, which makes normal text operations efficient. By storing the width, we don't need to update every cell when
performing certain operations (specifically overwriting a cell with new text). However, this approach is non-ideal
when implementing insert/delete lines when a horizontal margin is present (but this is super rare).

For multi-line cells, they would have no corresponding text but will have some attribute set to indicate the cell's
true information is stored elsewhere.

Cells also need damage tracking as an individual flag for optimizations when rendering.

## Scroll Back

Because scroll back cells are read-only, and the amount of scroll back the user may want is very large, a different
data structure should be used for scroll back. In particular, the 16 bit integers used for the modifiable screen
contents won't work at all when the scroll back becomes large enough. Additionally, most of the cells in scroll back
will be completely empty, so it makes sense to optimize them out.

To handle the large size restriction, we can either use cells with larger index types, and "global" maps for all indirect
metadata, or split these up into independent chunks/blocks/pages. The latter approach is clearly more efficient but
is more complicated, especially because hyperlinks and multi cell info can span multiple terminal rows. This can be
handled by either choosing boundaries which do not induce this behavior (and splitting hyperlink / drop multi cells in
pathological cases), or with special markers which indicate to look at a separate block. Splitting hyperlinks is
reasonable, but dropping multi cells is non-ideal (although realistically multi cells are going to be used by full
screen applications with no scroll back so its not super important.

The other important operation with the scroll back is reflowing lines which we auto-wrapped. This is very useful
functionality, but has important performance constraints, because the scroll back buffer can be potentially very
large. This is especially problematic in cases where there is an extremely large line (1 million characters) when
using a chunk based approach, since the entire line cannot fit in scroll back. Calling `cat` on a large file with no
newline needs to not hang the terminal. This implies that the scroll back limit should not be measured in lines (
which makes sense anyway because lines have no fixed meaning when we reflow text dynamically).

For this reason, we cannot consolidate logical lines into a single line for the purpose of scroll back. However,
handling the extremely large line case is still tricky (unless we enforce a maximum line length).

Multi cells with a height greater than 0 are also very difficult to deal with when reflowing lines, when the cells have
mixed height.

## Multi Cells

Combined cells come in 2 forms: wide characters as determined by the grapheme cluster width, and explicitly
sized text specified by the kitty text-sizing protocol. Although wide characters represent an extreme subset
of the text-sizing protocol, wide characters are more common and may deserve special optimizations.

To represent combined cells, it likely makes the most sense to store the associated text and graphics rendition
in the top-left cell. This is useful because it lets us re-use several fields, and maps naturally when rendering
cells. Every other "cell" linked to the primary cell can be ignored.

However, blank cells cannot be ignored when modifying terminal contents. In particular, erasing or overwriting
a single cell which is part of a combined cell clears the whole mulit-cell. This gives us to ways of handling
this operation: eager clearing or lazy clearing.

### Eager Clearing

In this model, when we "clear" a cell which is part of a multi-cell, we need to find the all other joined cells
and erase those too. This is somewhat inefficient and often redundant, as depending on the mutation, we're going
to erase those other cells next (think something clearing the current line).

### Lazy Clearing

In this model, clearing a part of a multi-cell simply sets a flag on the shared metadata indicating the cell is
no longer valid. When rendering, we can check this flag and treat the cell as blank if this flag is set. To
properly handle background character erase, we'd need to store an associated background color on the multicell
object. Additionally, we can longer share multi-cell metadata objects between "equivalent" multi-cells. This
is especially bad when considering text consisting of all width 2 characters. We'd effectively need 1.5 as many
objects as we would if we used the eager approach.

### Solution

Given these constraints, it makes the most sense to lazily clear multicells with height > 1 and eagerly clear
cells with height = 1. Almost all terminal mutations are implemented on a per-row basis, and so will be able
to eagerly clear multicells based by checking the boundaries of the operation. For the common case of width=2,
we'll still give the cell a multicell id, but it will have a fast path for lookup.

To determine which cell of a multicell is "primary", we'll need a 2 bit flags per cell. The primary cell will
have both bits set, while the top-most and left-most cells will have only 1 of the bits set. Finding the primary
cell can be done by iterating up and to the left looking for these flags.
