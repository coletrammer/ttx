#pragma once

#include "di/container/string/string_view.h"
#include "di/container/vector/vector.h"
#include "di/function/container/function.h"
#include "di/sync/atomic.h"
#include "di/sync/synchronized.h"
#include "di/vocab/error/result.h"
#include "di/vocab/pointer/box.h"
#include "dius/sync_file.h"
#include "dius/system/process.h"
#include "dius/thread.h"
#include "ttx/direction.h"
#include "ttx/key_event.h"
#include "ttx/mouse.h"
#include "ttx/paste_event.h"
#include "ttx/renderer.h"
#include "ttx/size.h"
#include "ttx/terminal.h"

namespace ttx {
struct CreatePaneArgs {
    di::Vector<di::TransparentStringView> command {};
    di::Optional<di::Path> capture_command_output_path {};
    di::Optional<di::Path> replay_path {};
    di::Optional<di::Path> save_state_path {};
};

class Pane {
public:
    static auto create_from_replay(di::PathView replay_path, di::Optional<di::Path> save_state_path, Size const& size,
                                   di::Function<void(Pane&)> did_exit, di::Function<void(Pane&)> did_update,
                                   di::Function<void(di::Span<byte const>)> did_selection,
                                   di::Function<void(di::StringView)> apc_passthrough) -> di::Result<di::Box<Pane>>;
    static auto create(CreatePaneArgs args, Size const& size, di::Function<void(Pane&)> did_exit,
                       di::Function<void(Pane&)> did_update, di::Function<void(di::Span<byte const>)> did_selection,
                       di::Function<void(di::StringView)> apc_passthrough) -> di::Result<di::Box<Pane>>;

    // For testing, create a mock pane. This doesn't actually create a psuedo terminal or a subprocess.
    static auto create_mock() -> di::Box<Pane>;

    explicit Pane(dius::SyncFile pty_controller, dius::system::ProcessHandle process,
                  di::Function<void(Pane&)> did_exit, di::Function<void(Pane&)> did_update,
                  di::Function<void(di::Span<byte const>)> did_selection,
                  di::Function<void(di::StringView)> apc_passthrough)
        : m_pty_controller(di::move(pty_controller))
        , m_terminal(m_pty_controller)
        , m_process(process)
        , m_did_exit(di::move(did_exit))
        , m_did_update(di::move(did_update))
        , m_did_selection(di::move(did_selection))
        , m_apc_passthrough(di::move(apc_passthrough)) {}
    ~Pane();

    auto draw(Renderer& renderer) -> RenderedCursor;

    auto event(KeyEvent const& event) -> bool;
    auto event(MouseEvent const& event) -> bool;
    auto event(FocusEvent const& event) -> bool;
    auto event(PasteEvent const& event) -> bool;

    void invalidate_all();
    void resize(Size const& size);
    void scroll(Direction direction, i32 amount_in_cells);
    auto save_state(di::PathView path) -> di::Result<>;
    void stop_capture();
    void exit();

private:
    auto selection_text() -> di::String;
    auto in_selection(MouseCoordinate coordinate) -> bool;
    void clear_selection();
    void reset_scroll();

    di::Atomic<bool> m_done { false };
    di::Atomic<bool> m_capture { true };
    di::Optional<MousePosition> m_last_mouse_position;
    dius::SyncFile m_pty_controller;
    di::Synchronized<Terminal> m_terminal;
    dius::system::ProcessHandle m_process;

    u32 m_vertical_scroll_offset { 0 };
    u32 m_horizontal_scroll_offset { 0 };

    di::Optional<MouseCoordinate> m_selection_start;
    di::Optional<MouseCoordinate> m_selection_end;

    // Application controlled callback when the internal process exits.
    di::Function<void(Pane&)> m_did_exit;

    // Application controlled callback when the terminal buffer has updated.
    di::Function<void(Pane&)> m_did_update;

    // Application controlled callback when text is selected.
    di::Function<void(di::Span<byte const>)> m_did_selection;

    // Application controlled callback when APC command is set.
    di::Function<void(di::StringView)> m_apc_passthrough;

    // These are declared last, for when dius::Thread calls join() in the destructor.
    dius::Thread m_process_thread;
    dius::Thread m_reader_thread;
};
}
