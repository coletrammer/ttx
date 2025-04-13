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
class Pane;

struct PaneHooks {
    /// @brief Application controlled callback when the internal process exits.
    di::Function<void(Pane&)> did_exit;

    /// @brief controlled callback when the terminal buffer has updated.
    di::Function<void(Pane&)> did_update;

    /// @brief Application controlled callback when text is selected.
    di::Function<void(di::Span<byte const>)> did_selection;

    /// @brief Application controlled callback when APC command is set.
    di::Function<void(di::StringView)> apc_passthrough;

    /// @brief Callback with the results on reading from the output pipe.
    di::Function<void(di::StringView)> did_finish_output;
};

struct CreatePaneArgs {
    di::Vector<di::TransparentString> command {};
    di::Optional<di::Path> capture_command_output_path {};
    di::Optional<di::Path> replay_path {};
    di::Optional<di::Path> save_state_path {};
    di::Optional<di::String> pipe_input {};
    bool pipe_output { false };
    PaneHooks hooks {};
};

class Pane {
public:
    static auto create_from_replay(u64 id, di::PathView replay_path, di::Optional<di::Path> save_state_path,
                                   Size const& size, PaneHooks hooks) -> di::Result<di::Box<Pane>>;
    static auto create(u64 id, CreatePaneArgs args, Size const& size) -> di::Result<di::Box<Pane>>;

    // For testing, create a mock pane. This doesn't actually create a psuedo terminal or a subprocess.
    static auto create_mock() -> di::Box<Pane>;

    explicit Pane(u64 id, dius::SyncFile pty_controller, Size const& size, dius::system::ProcessHandle process,
                  PaneHooks hooks)
        : m_id(id)
        , m_pty_controller(di::move(pty_controller))
        , m_terminal(di::in_place, id, m_pty_controller, size)
        , m_process(process)
        , m_hooks(di::move(hooks)) {}
    ~Pane();

    auto id() const { return m_id; }
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
    void soft_reset();
    void exit();

private:
    void reset_viewport_scroll();

    u64 m_id { 0 };
    di::Atomic<bool> m_done { false };
    di::Atomic<bool> m_capture { true };
    di::Optional<MousePosition> m_last_mouse_position;
    dius::SyncFile m_pty_controller;
    di::Synchronized<Terminal> m_terminal;
    dius::system::ProcessHandle m_process;

    u32 m_vertical_scroll_offset { 0 };
    u32 m_horizontal_scroll_offset { 0 };

    PaneHooks m_hooks;

    // These are declared last, for when dius::Thread calls join() in the destructor.
    dius::Thread m_process_thread;
    dius::Thread m_reader_thread;
    dius::Thread m_pipe_writer_thread;
    dius::Thread m_pipe_reader_thread;
};
}
