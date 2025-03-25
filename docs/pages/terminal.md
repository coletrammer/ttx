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

For text, it probably makes sense to store each line of text separately and have each cell refer to an offset within
its corresponding lines text. This allows for arbitrary width cells with an arbitrary amount of text, while still be
reasonably efficient. This is however unideal for resizing operations because reflowing the text requires modifying
every cell. I think this means that the reflow operation would have to be lazy with respect to the scroll buffer, as
otherwise the operation would be too inefficient.

For multi-line cells, they would have no corresponding text but will have some attribute set to indicate the cell's
true information is stored elsewhere.

Cells also need damage tracking as an individual flag for optimizations when rendering.

So we end up with something like this as the central data structure:

```cpp
struct Cell {
    u16 graphics_rendition_index; // 0 means default
    u16 hyperlink_id;             // 0 means none
    u16 multicell_index;          // 0 means none (single cell)
    u16 text_offset : 15;         // 0 means no text (this is 1 indexed)
    u16 dirty : 1;                // bit flag for damage tracking
};

struct Row {
    Vector<Cell> cells;
    String text;             // Text associated with the row.
    bool overflow { false }; // Use for rewrapping on resize. Set if the cursor overflowed when at this row.
};

struct Grid {
    Ring<Row> rows; // Ring buffer to optimize for scrolling the screen.
    Map<u16, GraphicsRendition> graphics_renditions;
    Map<u16, Hyperlink> hyperlinks;
    Map<u16, MultiCellInfo> multi_cell_info;
};
```

## Scroll Back

Because scroll back cells are read-only, and the amount of scroll back the user may want is very large, a different
data structure should be used for scroll back. In particular, the 16 bit integers used for the modifiable screen
contents won't work at all when the scroll back becomes large enough. Additionally, most of the cells in scroll back
will be completely empty, so it makes sense to optimize them out.

To handle the large size restriction, we can either cells with larger index types, and "global" maps for all indirect
metadata, or split these up into independent chunks/blocks/pages. The latter approach is clearly more efficient but
is more complicated, especially because hyperlinks and multi cell info can span multiple terminal rows. This can be
handled by either choosing boundaries which do not induce this behavior (and splitting hyperlink / drop multi cells in
pathological cases), or with special markers which indicate to look at a separate block. Splitting hyperlinks is
reasonable, but dropping multi cells is non-ideal (although realistically multi cells are going to be used by full
screen applications so its not super important.
