#pragma once

#include "di/container/string/string_view.h"
#include "di/container/tree/tree_map.h"
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
#include "ttx/mouse_click_tracker.h"
#include "ttx/paste_event.h"
#include "ttx/renderer.h"
#include "ttx/size.h"
#include "ttx/terminal.h"
#include "ttx/terminal/escapes/osc_52.h"
#include "ttx/terminal/escapes/osc_7.h"

namespace ttx {
class Pane;

struct PaneHooks {
    /// @brief Application controlled callback when the internal process exits.
    di::Function<void(Pane&, di::Optional<dius::system::ProcessResult>)> did_exit;

    /// @brief controlled callback when the terminal buffer has updated.
    di::Function<void(Pane&)> did_update;

    /// @brief Application controlled callback when a clipboard set/request is invoked.
    di::Function<void(terminal::OSC52, bool)> did_selection;

    /// @brief Application controlled callback when APC command is set.
    di::Function<void(di::StringView)> apc_passthrough;

    /// @brief Callback with the results on reading from the output pipe.
    di::Function<void(di::StringView)> did_finish_output;

    /// @brief Callback when the pane's current working directory changes.
    di::Function<void()> did_update_cwd;
};

struct CreatePaneArgs {
    auto clone() const -> CreatePaneArgs {
        // NOTE: the hooks aren't cloned.
        return { command.clone(),
                 capture_command_output_path.clone(),
                 replay_path.clone(),
                 save_state_path.clone(),
                 pipe_input.clone(),
                 cwd.clone(),
                 terminfo_dir.clone(),
                 term,
                 pipe_output,
                 mock,
                 {} };
    }

    auto with_cwd(di::Optional<di::Path> cwd) const& -> CreatePaneArgs {
        auto result = clone();
        result.cwd = di::move(cwd);
        return result;
    }

    di::Vector<di::TransparentString> command {};
    di::Optional<di::Path> capture_command_output_path {};
    di::Optional<di::Path> replay_path {};
    di::Optional<di::Path> save_state_path {};
    di::Optional<di::String> pipe_input {};
    di::Optional<di::Path> cwd {};
    di::Optional<di::Path> terminfo_dir {};
    di::TransparentStringView term { "xterm-ttx"_tsv };
    bool pipe_output { false };
    bool mock { false };
    PaneHooks hooks {};
};

class Pane {
public:
    static auto create_from_replay(u64 id, di::Optional<di::Path> cwd, di::PathView replay_path,
                                   di::Optional<di::Path> save_state_path, Size const& size, PaneHooks hooks)
        -> di::Result<di::Box<Pane>>;
    static auto create(u64 id, CreatePaneArgs args, Size const& size) -> di::Result<di::Box<Pane>>;

    // For testing, create a mock pane. This doesn't actually create a psuedo terminal or a subprocess.
    static auto create_mock(u64 id = 0, di::Optional<di::Path> cwd = {}) -> di::Box<Pane>;

    explicit Pane(u64 id, di::Optional<di::Path> cwd, dius::SyncFile pty_controller, Size const& size,
                  dius::system::ProcessHandle process, PaneHooks hooks)
        : m_id(id)
        , m_pty_controller(di::move(pty_controller))
        , m_terminal(di::in_place, id, size)
        , m_process(process)
        , m_cwd(di::move(cwd))
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
    void scroll_prev_command();
    void scroll_next_command();
    void copy_last_command(bool include_command);
    auto save_state(di::PathView path) -> di::Result<>;
    void send_clipboard(terminal::SelectionType selection_type, di::Vector<byte> data);
    void stop_capture();
    void soft_reset();
    void exit();

    /// @brief Get the pane's current working directory
    ///
    /// In the future, this method could fall back to (or prefer) scanning the procfs
    /// to determine this information. For now, this value is determined via OSC 7
    /// reports, which require shell integration to work.
    auto current_working_directory() const -> di::Optional<di::PathView> { return m_cwd.transform(&di::Path::view); }

private:
    void handle_terminal_event(TerminalEvent&& event);
    void write_pty_string(di::StringView data);
    void write_pty_string(di::TransparentStringView data);

    void update_cwd(terminal::OSC7&& path_with_hostname);
    void reset_viewport_scroll();

    u64 m_id { 0 };
    di::Atomic<bool> m_done { false };
    di::Atomic<bool> m_capture { true };
    di::Optional<MousePosition> m_last_mouse_position;
    di::Optional<terminal::AbsolutePosition> m_pending_selection_start;
    MouseClickTracker m_mouse_click_tracker { 3 };
    di::Synchronized<dius::SyncFile> m_pty_controller;
    di::Function<void()> m_restore_termios;
    di::Synchronized<Terminal> m_terminal;
    di::Optional<Size> m_desired_visible_size;
    dius::system::ProcessHandle m_process;

    u32 m_vertical_scroll_offset { 0 };
    u32 m_horizontal_scroll_offset { 0 };

    di::Optional<di::Path> m_cwd;
    PaneHooks m_hooks;

    // These are declared last, for when dius::Thread calls join() in the destructor.
    dius::Thread m_process_thread;
    dius::Thread m_reader_thread;
    dius::Thread m_pipe_writer_thread;
    dius::Thread m_pipe_reader_thread;
};
}
