#pragma once
#include "Game.hpp"
#include "risha/core/Project.hpp"
#include "risha/editor/Scene.hpp"
#include "risha/gfx/Canvas.hpp"
#include "risha/platform/Window.hpp"
#include "risha/ui/Ui.hpp"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace risha::app {

enum class Screen { Splash, Menu, Editor };
enum class Drag { None, Move, Resize };

class App {
public:
    App();
    void run();
    void autostart(bool play) { autostart_demo_ = true; autostart_play_ = play; }

private:
    bool init();
    double now() const;
    void toast(const std::wstring& text, bool error = false);
    void log(const std::wstring& text, bool error);
    std::filesystem::path script_path(const editor::SceneNode& node) const;
    void reset_editor_state();
    bool open_project(const std::filesystem::path& directory);
    bool save_all();
    void create_demo();
    void create_blank();
    void push_undo();
    void undo();
    void redo();
    void select_node(int id);
    void recompile();
    std::wstring unique_name(const std::wstring& base) const;
    void add_object(const std::wstring& type);
    void delete_selected();
    void duplicate_selected();
    void play();
    void stop_game();
    void commit_field(bool apply);
    bool field(const std::string& id, gfx::Rect rect, const std::wstring& value);
    void handle_field_key(const platform::Event& event);
    void handle_code_key(const platform::Event& event);
    size_t line_start(size_t position) const;
    size_t line_end(size_t position) const;
    void handle_event(const platform::Event& event);
    void render();
    void render_splash();
    void render_menu();
    void render_editor();
    void render_viewport(gfx::Rect area);
    void render_hierarchy(gfx::Rect area);
    void render_inspector(gfx::Rect area);
    void render_bottom(gfx::Rect area);
    bool hit_box(const editor::SceneNode& node, int px, int py, gfx::Rect& box) const;

    platform::NativeWindow window_;
    gfx::Canvas canvas_;
    gfx::Canvas menu_feather_;
    ui::Ui ui_;
    Game game_;
    Screen screen_{Screen::Splash};
    bool quit_{false};
    bool autostart_demo_{false};
    bool autostart_play_{false};
    bool splash_skip_{false};
    double splash_start_{0.0};
    double last_game_tick_{0.0};
    std::vector<OutputLine> game_output_;

    std::filesystem::path project_dir_;
    core::ProjectSettings settings_;
    editor::Scene scene_;
    std::vector<std::filesystem::path> recent_;
    std::vector<editor::SceneSnapshot> undo_;
    std::vector<editor::SceneSnapshot> redo_;
    std::vector<OutputLine> output_;
    std::wstring status_;
    bool status_error_{false};
    double status_time_{-100.0};

    std::string field_id_;
    int field_node_{0};
    std::wstring field_;
    size_t field_cursor_{0};

    std::wstring code_;
    int code_node_{0};
    bool code_dirty_{false};
    bool script_focus_{false};
    size_t cursor_{0};
    int scroll_line_{0};
    std::wstring compile_error_;
    int bottom_tab_{0};
    bool show_help_{false};

    float zoom_{0.9f};
    float pan_x_{0.0f};
    float pan_y_{0.0f};
    bool panned_init_{false};
    bool panning_{false};
    float vp_ox_{0.0f};
    float vp_oy_{0.0f};
    int last_mx_{0};
    int last_my_{0};
    int wheel_{0};
    int hierarchy_scroll_{0};
    std::unordered_map<std::string, float> row_hover_;
    Drag drag_{Drag::None};
    int drag_start_x_{0};
    int drag_start_y_{0};
    int drag_orig_x_{0};
    int drag_orig_y_{0};
    int drag_orig_w_{0};
    int drag_orig_h_{0};
    bool drag_undo_{false};
};

}
