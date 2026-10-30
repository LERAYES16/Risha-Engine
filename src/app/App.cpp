#include "App.hpp"
#include "risha/core/Paths.hpp"
#include "risha/core/Text.hpp"
#include "risha/gfx/Text.hpp"
#include "risha/script/Script.hpp"
#include "risha/ui/Feather.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>

namespace risha::app {
namespace fs = std::filesystem;
using gfx::Color;
using gfx::Rect;
using platform::Event;
using platform::EventType;
using platform::Key;
using ui::ButtonStyle;
namespace th = ui::theme;

namespace {

constexpr double kSplashSeconds = 4.6;
const std::wstring kTitleEnglish = L"Risha Engine";
const std::wstring kTitleArabic = L"محرك ريشة";

std::wstring read_file(const fs::path& path, bool* ok = nullptr) {
    std::ifstream file(path, std::ios::binary);
    if (ok) *ok = static_cast<bool>(file);
    if (!file) return L"";
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return core::from_utf8(data);
}

bool write_file(const fs::path& path, const std::wstring& text) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    const std::string data = core::to_utf8(text);
    file.write(data.data(), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

fs::path projects_root() {
    return core::make_path(platform::home_directory()) / core::make_path(L"Documents") / core::make_path(L"Risha Projects");
}

fs::path recent_file() {
    return core::make_path(platform::data_directory()) / core::make_path(L"recent.txt");
}

std::vector<fs::path> load_recent() {
    std::vector<fs::path> result;
    const std::wstring text = read_file(recent_file());
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();
        std::wstring line = text.substr(start, end - start);
        while (!line.empty() && (line.back() == L'\r' || line.back() == L' ')) line.pop_back();
        std::error_code error;
        if (!line.empty() && fs::exists(core::make_path(line) / "project.rishaproject", error)) result.push_back(core::make_path(line));
        start = end + 1;
    }
    return result;
}

void remember_project(const fs::path& directory) {
    auto list = load_recent();
    list.erase(std::remove(list.begin(), list.end(), directory), list.end());
    list.insert(list.begin(), directory);
    if (list.size() > 6) list.resize(6);
    std::wstring text;
    for (const auto& item : list) text += core::path_string(item) + L"\n";
    write_file(recent_file(), text);
}

const wchar_t* kDemoScript =
    L"عام السرعة = 280\n"
    L"عام قوة_القفز = 560\n"
    L"عرف الاتجاه = 0\n"
    L"إذا زر_مستمر(\"حركة_يمين\")\n"
    L"    الاتجاه = 1\n"
    L"نهاية\n"
    L"إذا زر_مستمر(\"حركة_يسار\")\n"
    L"    الاتجاه = -1\n"
    L"نهاية\n"
    L"سرعة(\"الكتلة\", الاتجاه * السرعة)\n"
    L"إذا زر_مضغوط(\"قفز\") و على_الأرض(\"الكتلة\")\n"
    L"    اقفز(\"الكتلة\", قوة_القفز)\n"
    L"نهاية\n"
    L"إذا موضع_س(\"الكتلة\") < 0\n"
    L"    ضع_موضع(\"الكتلة\", 0, موضع_ص(\"الكتلة\"))\n"
    L"نهاية\n"
    L"إذا موضع_س(\"الكتلة\") + عرض(\"الكتلة\") > عرض_العالم()\n"
    L"    ضع_موضع(\"الكتلة\", عرض_العالم() - عرض(\"الكتلة\"), موضع_ص(\"الكتلة\"))\n"
    L"نهاية\n";

struct Palette { unsigned int value; };
const Palette kPalette[] = {{0xFF6EA8FE}, {0xFF4C9AFF}, {0xFF5BD68A}, {0xFFF2C14E}, {0xFFE9694F}, {0xFFB07CF2}, {0xFFF2F4F8}, {0xFF6B4E2E}};

std::wstring number_text(double value) {
    return core::from_utf8(core::format_double(value, 4));
}

}

App::App() = default;

bool App::init() {
    if (!window_.create(L"Risha Engine - محرك ريشة", 1280, 760, 960, 600)) return false;
    canvas_.resize(window_.width(), window_.height());
    menu_feather_.resize(240, 300);
    menu_feather_.clear(th::background);
    ui::draw_feather(menu_feather_, 120.0f, 150.0f, 260.0f, 1.0f, 0.0);
    recent_ = load_recent();
    return true;
}

double App::now() const {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    return std::chrono::duration<double>(clock::now() - start).count();
}

void App::toast(const std::wstring& text, bool error) {
    status_ = text;
    status_error_ = error;
    status_time_ = ui_.time;
}

void App::log(const std::wstring& text, bool error) {
    output_.push_back({text, error});
    if (output_.size() > 300) output_.erase(output_.begin(), output_.begin() + 100);
}

fs::path App::script_path(const editor::SceneNode& node) const {
    return project_dir_ / core::make_path(node.script);
}

void App::reset_editor_state() {
    undo_.clear();
    redo_.clear();
    field_.clear();
    script_focus_ = false;
    code_.clear();
    code_node_ = 0;
    code_dirty_ = false;
    cursor_ = 0;
    scroll_line_ = 0;
    output_.clear();
    panned_init_ = false;
    pan_x_ = 0.0f;
    pan_y_ = 0.0f;
    hierarchy_scroll_ = 0;
    compile_error_.clear();
}

bool App::open_project(const fs::path& directory) {
    core::ProjectSettings settings = core::default_project_settings();
    std::error_code error;
    if (!fs::exists(directory / "project.rishaproject", error) || !settings.load(core::path_string(directory / "project.rishaproject"))) {
        toast(L"لا يوجد مشروع ريشة صالح في هذا المجلد.", true);
        return false;
    }
    editor::Scene scene;
    const fs::path scene_path = directory / core::make_path(settings.main_scene);
    if (fs::exists(scene_path, error) && !scene.load(core::path_string(scene_path))) {
        toast(L"ملف المشهد تالف أو غير مدعوم.", true);
        return false;
    }
    project_dir_ = directory;
    settings_ = settings;
    scene_ = scene;
    reset_editor_state();
    select_node(scene_.selected_id());
    remember_project(directory);
    recent_ = load_recent();
    screen_ = Screen::Editor;
    window_.set_title(settings_.name + L" - Risha Engine");
    return true;
}

bool App::save_all() {
    bool ok = true;
    if (code_dirty_ && code_node_ != 0) {
        if (const auto* node = scene_.find(code_node_)) {
            if (!node->script.empty()) ok = write_file(script_path(*node), code_) && ok;
        }
        code_dirty_ = false;
    }
    ok = settings_.save(core::path_string(project_dir_ / "project.rishaproject")) && ok;
    ok = scene_.save(core::path_string(project_dir_ / core::make_path(settings_.main_scene))) && ok;
    return ok;
}

void App::create_demo() {
    const fs::path directory = projects_root() / core::make_path(L"لعبة تجريبية");
    std::error_code error;
    if (!fs::exists(directory / "project.rishaproject", error)) {
        core::ProjectSettings settings = core::default_project_settings();
        settings.name = L"لعبة تجريبية";
        settings.background_color = 0xFF9BD1F0u;
        editor::Scene scene;
        if (auto* root = scene.find(1)) root->name = L"العالم";
        const int ground = scene.add_node(1, L"الأرض", L"أرض");
        if (auto* node = scene.find(ground)) { node->x = 0; node->y = 440; node->width = 960; node->height = 100; node->color = 0xFF8A5A33u; node->body = 2; }
        const int block = scene.add_node(1, L"الكتلة", L"مربع");
        if (auto* node = scene.find(block)) {
            node->x = 200; node->y = 300; node->width = 56; node->height = 56; node->color = 0xFFE9694F; node->body = 1; node->gravity = 1400.0f;
            node->script = L"الكتلة.rishascript"; node->z = 2;
        }
        const int title = scene.add_node(1, L"العنوان", L"نص");
        if (auto* node = scene.find(title)) { node->x = 280; node->y = 40; node->width = 400; node->height = 50; node->text = L"الأسهم للحركة، المسافة للقفز"; node->color = 0xFF1B2A41u; node->z = 1; }
        write_file(directory / "الكتلة.rishascript", kDemoScript);
        settings.save(core::path_string(directory / "project.rishaproject"));
        scene.save(core::path_string(directory / core::make_path(settings.main_scene)));
    }
    open_project(directory);
}

void App::create_blank() {
    const fs::path root = projects_root();
    fs::path directory;
    for (int i = 1; i < 1000; ++i) {
        directory = root / core::make_path(L"مشروع " + std::to_wstring(i));
        std::error_code error;
        if (!fs::exists(directory, error)) break;
    }
    core::ProjectSettings settings = core::default_project_settings();
    editor::Scene scene;
    if (auto* r = scene.find(1)) r->name = L"العالم";
    const int ground = scene.add_node(1, L"الأرض", L"أرض");
    if (auto* node = scene.find(ground)) { node->x = 0; node->y = 440; node->width = 960; node->height = 100; node->color = 0xFF6B4E2E; node->body = 2; }
    settings.name = directory.filename().wstring().empty() ? L"مشروع جديد" : core::from_utf8(directory.filename().u8string());
    write_file(directory / "project.rishaproject", L"");
    settings.save(core::path_string(directory / "project.rishaproject"));
    scene.save(core::path_string(directory / core::make_path(settings.main_scene)));
    open_project(directory);
}

void App::push_undo() {
    undo_.push_back(scene_.capture());
    if (undo_.size() > 100) undo_.erase(undo_.begin());
    redo_.clear();
    scene_.set_dirty(true);
}

void App::undo() {
    if (undo_.empty()) { toast(L"لا يوجد ما يمكن التراجع عنه."); return; }
    redo_.push_back(scene_.capture());
    scene_.restore(undo_.back());
    undo_.pop_back();
    scene_.set_dirty(true);
    select_node(scene_.selected_id());
}

void App::redo() {
    if (redo_.empty()) { toast(L"لا يوجد ما يمكن إعادته."); return; }
    undo_.push_back(scene_.capture());
    scene_.restore(redo_.back());
    redo_.pop_back();
    scene_.set_dirty(true);
    select_node(scene_.selected_id());
}

void App::select_node(int id) {
    if (code_dirty_ && code_node_ != 0) {
        if (const auto* old = scene_.find(code_node_)) if (!old->script.empty()) write_file(script_path(*old), code_);
        code_dirty_ = false;
    }
    commit_field(false);
    scene_.set_selected_id(id);
    code_.clear();
    cursor_ = 0;
    scroll_line_ = 0;
    code_node_ = id;
    if (const auto* node = scene_.find(id)) {
        if (!node->script.empty()) {
            bool ok = false;
            code_ = read_file(script_path(*node), &ok);
        }
    }
    recompile();
}

void App::recompile() {
    compile_error_.clear();
    if (code_.empty()) return;
    script::Program program;
    std::vector<script::Diagnostic> errors;
    script::Compiler compiler;
    if (!compiler.compile(code_, program, errors) && !errors.empty()) {
        compile_error_ = L"سطر " + std::to_wstring(errors[0].line) + L": " + errors[0].message;
    }
}

std::wstring App::unique_name(const std::wstring& base) const {
    auto taken = [&](const std::wstring& name) {
        for (const auto& node : scene_.nodes()) if (node.name == name) return true;
        return false;
    };
    if (!taken(base)) return base;
    for (int i = 2; i < 10000; ++i) {
        const std::wstring candidate = base + L" " + std::to_wstring(i);
        if (!taken(candidate)) return candidate;
    }
    return base;
}

void App::add_object(const std::wstring& type) {
    push_undo();
    const int id = scene_.add_node(1, unique_name(type), type);
    if (auto* node = scene_.find(id)) {
        node->x = 400; node->y = 200;
        if (type == L"لاعب") { node->width = 48; node->height = 64; node->color = 0xFF4C9AFF; node->body = 1; node->gravity = 1400.0f; }
        else if (type == L"مخلوق") { node->width = 56; node->height = 56; node->color = 0xFFE9694F; node->body = 1; node->gravity = 1400.0f; }
        else if (type == L"أرض") { node->width = 320; node->height = 48; node->y = 440; node->color = 0xFF6B4E2E; node->body = 2; }
        else if (type == L"كاميرا") { node->width = 320; node->height = 180; node->active_camera = true; }
        else if (type == L"نص") { node->width = 200; node->height = 40; node->text = L"نص جديد"; node->color = 0xFFF2F4F8; }
        else { node->width = 64; node->height = 64; }
    }
    select_node(id);
}

void App::delete_selected() {
    const int id = scene_.selected_id();
    if (id <= 1) { toast(L"لا يمكن حذف العالم.", true); return; }
    push_undo();
    scene_.remove_node(id);
    select_node(1);
}

void App::duplicate_selected() {
    const int id = scene_.selected_id();
    if (id <= 1) return;
    push_undo();
    const int copy = scene_.duplicate_node(id);
    if (copy > 0) select_node(copy);
}

void App::play() {
    if (game_.open()) { game_.window().raise(); return; }
    if (!save_all()) toast(L"تعذر الحفظ قبل التشغيل.", true);
    scene_.set_dirty(false);
    std::vector<OutputLine> sink;
    game_.start(scene_, settings_, project_dir_, sink);
    for (auto& line : sink) log(line.text, line.error);
    for (const auto& line : sink) if (line.error) { bottom_tab_ = 1; break; }
    last_game_tick_ = now();
}

void App::stop_game() {
    if (game_.open()) { game_.stop(); log(L"توقف التشغيل.", false); }
}

void App::commit_field(bool apply) {
    if (field_id_.empty()) return;
    const std::string id = field_id_;
    const std::wstring value = field_;
    field_id_.clear();
    if (!apply) return;
    auto* node = scene_.find(field_node_);
    if (!node) return;
    auto as_int = [&](int& target) {
        long long parsed = 0;
        if (core::parse_int(core::to_utf8(value), parsed) && parsed > -100000 && parsed < 100000) { if (target != parsed) { push_undo(); target = static_cast<int>(parsed); } }
        else toast(L"قيمة غير صالحة.", true);
    };
    if (id == "name") {
        if (!value.empty() && value != node->name) {
            push_undo();
            if (!scene_.rename_node(node->id, value)) { undo_.pop_back(); toast(L"الاسم مستخدم أو غير صالح.", true); }
        }
    } else if (id == "x") as_int(node->x);
    else if (id == "y") as_int(node->y);
    else if (id == "w") { as_int(node->width); node->width = std::max(1, node->width); }
    else if (id == "h") { as_int(node->height); node->height = std::max(1, node->height); }
    else if (id == "g") {
        double parsed = 0;
        if (core::parse_double(core::to_utf8(value), parsed) && std::fabs(parsed) < 100000) { push_undo(); node->gravity = static_cast<float>(parsed); }
        else toast(L"قيمة غير صالحة.", true);
    } else if (id == "text") { if (value != node->text) { push_undo(); node->text = value; } }
}

bool App::field(const std::string& id, Rect rect, const std::wstring& value) {
    const bool focused = field_id_ == id;
    const bool inside = ui_.hot(rect);
    if (ui_.pressed && inside && !focused) {
        commit_field(true);
        field_id_ = id;
        field_node_ = scene_.selected_id();
        field_ = value;
        field_cursor_ = field_.size();
        script_focus_ = false;
    } else if (ui_.pressed && focused && !inside) {
        commit_field(true);
    }
    const bool now_focused = field_id_ == id;
    canvas_.fill_rounded(rect, 6.0f, now_focused ? Color(14, 20, 30) : (inside ? Color(24, 32, 46) : Color(16, 22, 32)));
    canvas_.stroke_rounded(rect, 6.0f, 1.0f, now_focused ? th::accent : (inside ? Color(80, 100, 130) : th::border));
    gfx::ClipScope clip(canvas_, rect.inset(3));
    const std::wstring& shown = now_focused ? field_ : value;
    const gfx::TextLayout& layout = gfx::text_layout(shown, 14.0f);
    const bool rtl = layout.rtl;
    const float left = rtl ? static_cast<float>(rect.x + rect.w - 8) - layout.width : static_cast<float>(rect.x + 8);
    gfx::draw_text(canvas_, shown, left, static_cast<float>(rect.y + 18), 14.0f, th::text, gfx::Anchor::Left);
    if (now_focused && std::fmod(ui_.time, 1.0) < 0.55) {
        const size_t index = std::min(field_cursor_, layout.caret.size() ? layout.caret.size() - 1 : 0);
        const float cx = left + (layout.caret.empty() ? 0.0f : layout.caret[index]);
        canvas_.fill_rect(Rect{static_cast<int>(cx), rect.y + 5, 1, rect.h - 10}, th::accent);
    }
    return now_focused;
}

void App::handle_field_key(const Event& event) {
    if (event.type == EventType::Text) {
        for (wchar_t c : event.text) if (c >= 32 && c != 127) { field_.insert(field_cursor_, 1, c); ++field_cursor_; }
        return;
    }
    switch (event.key) {
        case Key::Enter: case Key::Tab: commit_field(true); break;
        case Key::Escape: commit_field(false); break;
        case Key::Backspace: if (field_cursor_ > 0) { field_.erase(field_cursor_ - 1, 1); --field_cursor_; } break;
        case Key::Delete: if (field_cursor_ < field_.size()) field_.erase(field_cursor_, 1); break;
        case Key::Left: if (field_cursor_ > 0) --field_cursor_; break;
        case Key::Right: if (field_cursor_ < field_.size()) ++field_cursor_; break;
        case Key::Home: field_cursor_ = 0; break;
        case Key::End: field_cursor_ = field_.size(); break;
        default: break;
    }
}

size_t App::line_start(size_t position) const {
    while (position > 0 && code_[position - 1] != L'\n') --position;
    return position;
}

size_t App::line_end(size_t position) const {
    while (position < code_.size() && code_[position] != L'\n') ++position;
    return position;
}

void App::handle_code_key(const Event& event) {
    bool changed = false;
    if (event.type == EventType::Text) {
        for (wchar_t c : event.text) if (c >= 32 && c != 127) { code_.insert(cursor_, 1, c); ++cursor_; changed = true; }
    } else {
        switch (event.key) {
            case Key::Enter: {
                const size_t start = line_start(cursor_);
                size_t indent = start;
                while (indent < code_.size() && code_[indent] == L' ') ++indent;
                const std::wstring pad(std::min(indent, cursor_) - start, L' ');
                code_.insert(cursor_, L"\n" + pad);
                cursor_ += 1 + pad.size();
                changed = true;
                break;
            }
            case Key::Tab: code_.insert(cursor_, L"    "); cursor_ += 4; changed = true; break;
            case Key::Backspace: if (cursor_ > 0) { code_.erase(cursor_ - 1, 1); --cursor_; changed = true; } break;
            case Key::Delete: if (cursor_ < code_.size()) { code_.erase(cursor_, 1); changed = true; } break;
            case Key::Left: if (cursor_ > 0) --cursor_; break;
            case Key::Right: if (cursor_ < code_.size()) ++cursor_; break;
            case Key::Home: cursor_ = line_start(cursor_); break;
            case Key::End: cursor_ = line_end(cursor_); break;
            case Key::Up: case Key::Down: {
                const size_t start = line_start(cursor_);
                const size_t column = cursor_ - start;
                if (event.key == Key::Up) {
                    if (start == 0) cursor_ = 0;
                    else { const size_t previous = line_start(start - 1); cursor_ = previous + std::min(column, start - 1 - previous); }
                } else {
                    const size_t end = line_end(cursor_);
                    if (end >= code_.size()) cursor_ = code_.size();
                    else { const size_t next_end = line_end(end + 1); cursor_ = end + 1 + std::min(column, next_end - end - 1); }
                }
                break;
            }
            case Key::PageUp: scroll_line_ = std::max(0, scroll_line_ - 8); break;
            case Key::PageDown: scroll_line_ += 8; break;
            default: break;
        }
    }
    if (changed) {
        code_dirty_ = true;
        scene_.set_dirty(true);
        recompile();
    }
}

void App::handle_event(const Event& event) {
    switch (event.type) {
        case EventType::Close: quit_ = true; return;
        case EventType::Resize: canvas_.resize(window_.width(), window_.height()); return;
        case EventType::MouseMove: ui_.mouse_x = event.x; ui_.mouse_y = event.y; return;
        case EventType::Leave: ui_.mouse_x = -1000; ui_.mouse_y = -1000; return;
        case EventType::MouseDown:
            ui_.mouse_x = event.x; ui_.mouse_y = event.y;
            if (event.button == 1) { ui_.pressed = true; ui_.mouse_down = true; }
            else if (event.button == 3) { panning_ = true; }
            return;
        case EventType::MouseUp:
            ui_.mouse_x = event.x; ui_.mouse_y = event.y;
            if (event.button == 1) { ui_.released = true; ui_.mouse_down = false; }
            else if (event.button == 3) panning_ = false;
            return;
        case EventType::MouseWheel: wheel_ += event.wheel; return;
        case EventType::LostFocus: ui_.mouse_down = false; panning_ = false; return;
        default: break;
    }
    if (screen_ == Screen::Splash) {
        if (event.type == EventType::KeyDown) splash_skip_ = true;
        return;
    }
    if (screen_ != Screen::Editor) return;
    if (event.type == EventType::KeyDown && show_help_) { show_help_ = false; return; }
    if (event.type == EventType::KeyDown && event.ctrl) {
        switch (event.key) {
            case Key::S: if (save_all()) { scene_.set_dirty(false); toast(L"تم الحفظ."); } else toast(L"تعذر الحفظ.", true); return;
            case Key::Z: undo(); return;
            case Key::Y: redo(); return;
            case Key::D: duplicate_selected(); return;
            default: break;
        }
    }
    if (event.type == EventType::KeyDown) {
        if (event.key == Key::F5) { play(); return; }
        if (event.key == Key::F6) { stop_game(); return; }
        if (event.key == Key::F1) { show_help_ = !show_help_; return; }
    }
    if (!field_id_.empty()) { if (event.type == EventType::KeyDown || event.type == EventType::Text) handle_field_key(event); return; }
    if (script_focus_ && (event.type == EventType::KeyDown || event.type == EventType::Text)) {
        if (event.type == EventType::KeyDown && event.key == Key::Escape) { script_focus_ = false; return; }
        handle_code_key(event);
        return;
    }
    if (event.type == EventType::KeyDown && event.key == Key::Delete) delete_selected();
}

void App::run() {
    if (!init()) return;
    double previous = now();
    splash_start_ = previous;
    if (autostart_demo_) { create_demo(); if (autostart_play_) play(); }
    while (!quit_) {
        const bool game_open = game_.open();
        const int timeout = (screen_ == Screen::Splash || ui_.animating() || game_open) ? 8 : 400;
        std::vector<platform::NativeWindow*> windows{&window_};
        if (game_open) windows.push_back(&game_.window());
        platform::wait_for_events(timeout, windows);
        const double t = now();
        const double dt = t - previous;
        previous = t;
        wheel_ = 0;
        for (const auto& event : window_.poll()) handle_event(event);
        if (game_open && game_.open()) game_.handle(game_.window().poll());
        if (game_open) {
            if (game_.open()) { game_.tick(t - last_game_tick_, game_output_); }
            for (auto& line : game_output_) log(line.text, line.error);
            game_output_.clear();
            last_game_tick_ = t;
            if (!game_.open()) log(L"توقف التشغيل.", false);
        }
        if (!window_.valid()) break;
        ui_.begin_frame(dt, &canvas_);
        render();
        ui_.end_frame();
        window_.present(canvas_);
    }
    game_.stop();
    if (screen_ == Screen::Editor && (scene_.dirty() || code_dirty_)) save_all();
}

void App::render() {
    canvas_.clear(th::background);
    switch (screen_) {
        case Screen::Splash: render_splash(); break;
        case Screen::Menu: render_menu(); break;
        case Screen::Editor: render_editor(); break;
    }
}

void App::render_splash() {
    const double t = ui_.time;
    const int w = canvas_.width();
    const int h = canvas_.height();
    canvas_.gradient_rect(Rect{0, 0, w, h}, Color(10, 14, 22), Color(20, 30, 48));
    const float progress = static_cast<float>(std::min(1.0, t / (kSplashSeconds - 1.0)));
    ui::draw_feather(canvas_, w * 0.5f, h * 0.40f, h * 0.62f, progress, t);
    const double typed = std::min(1.0, t / (kSplashSeconds - 1.2));
    const int total = static_cast<int>(kTitleEnglish.size() + kTitleArabic.size());
    const int count = static_cast<int>(typed * total + 0.999);
    const int english = std::min<int>(count, static_cast<int>(kTitleEnglish.size()));
    const int arabic = std::max(0, count - static_cast<int>(kTitleEnglish.size()));
    const float size = 34.0f;
    const float ew = gfx::text_width(kTitleEnglish, size);
    const float dash = gfx::text_width(L" - ", size);
    const float aw = gfx::text_width(kTitleArabic, size);
    const float total_w = ew + dash + aw;
    const float left = w * 0.5f - total_w * 0.5f;
    const float baseline = h * 0.86f;
    gfx::draw_text(canvas_, kTitleEnglish.substr(0, static_cast<size_t>(english)), left, baseline, size, th::text, gfx::Anchor::Left, true, gfx::Direction::LeftToRight);
    if (count > static_cast<int>(kTitleEnglish.size())) gfx::draw_text(canvas_, L" - ", left + ew, baseline, size, th::muted, gfx::Anchor::Left, false, gfx::Direction::LeftToRight);
    if (arabic > 0) gfx::draw_text(canvas_, kTitleArabic.substr(0, static_cast<size_t>(arabic)), left + total_w, baseline, size, Color(120, 180, 255), gfx::Anchor::Right, true, gfx::Direction::RightToLeft);
    if (count < total && std::fmod(t, 0.5) < 0.3) {
        const float caret_x = arabic > 0 ? left + total_w - gfx::text_width(kTitleArabic.substr(0, static_cast<size_t>(arabic)), size, gfx::Direction::RightToLeft) - 4.0f : left + gfx::text_width(kTitleEnglish.substr(0, static_cast<size_t>(english)), size, gfx::Direction::LeftToRight) + 2.0f;
        canvas_.fill_rect(Rect{static_cast<int>(caret_x), static_cast<int>(baseline - size * 0.9f), 2, static_cast<int>(size * 1.1f)}, th::accent);
    }
    const Rect bar{w / 2 - 180, h - 44, 360, 6};
    canvas_.fill_rounded(bar, 3.0f, Color(255, 255, 255, 30));
    canvas_.fill_rounded(Rect{bar.x, bar.y, std::max(6, static_cast<int>(bar.w * std::min(1.0, t / kSplashSeconds))), bar.h}, 3.0f, th::accent);
    gfx::draw_text(canvas_, L"جارٍ التحميل…", w * 0.5f, static_cast<float>(h - 54), 13.0f, th::muted, gfx::Anchor::Center);
    if (t >= kSplashSeconds || splash_skip_) { screen_ = Screen::Menu; }
    ui_.pressed = false;
}

void App::render_menu() {
    const int w = canvas_.width();
    const int h = canvas_.height();
    const int side = 320;
    const Rect panel{w - side, 0, side, h};
    canvas_.fill_rect(panel, th::panel);
    canvas_.fill_rect(Rect{panel.x, 0, 1, h}, th::border);
    ui_.text(L"محرك ريشة", w - 28, 78, 36.0f, th::text, true);
    ui_.text(L"Risha Engine 0.5", w - 28, 106, 14.0f, th::muted);
    const int bx = panel.x + 28;
    const int bw = side - 56;
    int y = 150;
    if (ui_.button("m_new", Rect{bx, y, bw, 48}, L"مشروع جديد", ButtonStyle::Primary, true, 17.0f)) create_blank();
    y += 60;
    if (ui_.button("m_demo", Rect{bx, y, bw, 48}, L"لعبة تجريبية", ButtonStyle::Normal, true, 17.0f)) create_demo();
    y += 60;
    if (ui_.button("m_open", Rect{bx, y, bw, 48}, L"فتح مشروع…", ButtonStyle::Normal, true, 17.0f)) {
        const std::wstring chosen = platform::open_folder_dialog(L"اختر مجلد المشروع");
        if (!chosen.empty()) open_project(core::make_path(chosen));
    }
    y += 60;
    if (ui_.button("m_exit", Rect{bx, y, bw, 48}, L"خروج", ButtonStyle::Danger, true, 17.0f)) quit_ = true;
    canvas_.blit(menu_feather_, 40 + (w - side - 240) / 2 - 20, 40);
    const int area_right = w - side - 48;
    ui_.text(L"المشاريع الأخيرة", area_right, 360, 22.0f, th::text, true);
    int cy = 384;
    if (recent_.empty()) ui_.text(L"لا توجد مشاريع بعد. ابدأ بمشروع جديد أو جرّب اللعبة التجريبية.", area_right, cy + 30, 15.0f, th::muted);
    int index = 0;
    for (const auto& item : recent_) {
        const Rect card{48, cy, area_right - 48, 52};
        const std::wstring name = core::from_utf8(item.filename().u8string());
        if (ui_.button("recent" + std::to_string(index), card, name + L"   —   " + core::path_string(item), ButtonStyle::Normal, true, 14.0f)) { open_project(item); break; }
        cy += 60;
        ++index;
        if (cy + 52 > h - 20) break;
    }
    if (!status_.empty() && ui_.time - status_time_ < 4.0) gfx::draw_text(canvas_, status_, 48.0f, static_cast<float>(h - 24), 15.0f, status_error_ ? th::danger : th::muted, gfx::Anchor::Left);
}

bool App::hit_box(const editor::SceneNode& node, int px, int py, Rect& box) const {
    const float x = vp_ox_ + static_cast<float>(node.x) * zoom_;
    const float y = vp_oy_ + static_cast<float>(node.y) * zoom_;
    box = Rect{static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)),
               std::max(4, static_cast<int>(std::lround(node.width * std::fabs(node.scale_x) * zoom_))),
               std::max(4, static_cast<int>(std::lround(node.height * std::fabs(node.scale_y) * zoom_)))};
    return box.contains(px, py);
}

void App::render_viewport(Rect area) {
    canvas_.fill_rect(area, Color(9, 12, 18));
    gfx::ClipScope clip(canvas_, area);
    if (!panned_init_) {
        pan_x_ = 0; pan_y_ = 0; panned_init_ = true;
        zoom_ = std::clamp(std::min(static_cast<float>(area.w - 40) / static_cast<float>(settings_.width), static_cast<float>(area.h - 40) / static_cast<float>(settings_.height)), 0.2f, 2.0f);
    }
    vp_ox_ = area.x + (area.w - settings_.width * zoom_) * 0.5f + pan_x_;
    vp_oy_ = area.y + (area.h - settings_.height * zoom_) * 0.5f + pan_y_;
    const Rect world{static_cast<int>(vp_ox_), static_cast<int>(vp_oy_), static_cast<int>(settings_.width * zoom_), static_cast<int>(settings_.height * zoom_)};
    canvas_.fill_rect(world, Color::from_argb(settings_.background_color));
    const float step = 40.0f * zoom_;
    if (step > 8.0f) {
        for (float gx = 0; gx < world.w; gx += step) canvas_.fill_rect(Rect{world.x + static_cast<int>(gx), world.y, 1, world.h}, Color(255, 255, 255, 14));
        for (float gy = 0; gy < world.h; gy += step) canvas_.fill_rect(Rect{world.x, world.y + static_cast<int>(gy), world.w, 1}, Color(255, 255, 255, 14));
    }
    canvas_.stroke_rounded(world.inset(-1), 0.0f, 1.0f, Color(120, 150, 200, 120));
    std::vector<const editor::SceneNode*> order;
    for (const auto& node : scene_.nodes()) if (node.parent_id != 0) order.push_back(&node);
    std::stable_sort(order.begin(), order.end(), [](auto* a, auto* b) { return a->z < b->z; });
    for (const auto* node : order) ui::draw_node(canvas_, *node, vp_ox_, vp_oy_, zoom_, true, ui_.time);
    auto* selected = scene_.find(scene_.selected_id());
    Rect sel{};
    bool has_sel = false;
    if (selected && selected->parent_id != 0) { hit_box(*selected, -1, -1, sel); has_sel = true; }
    const Rect handle{sel.x + sel.w - 7, sel.y + sel.h - 7, 14, 14};
    if (has_sel) {
        canvas_.stroke_rounded(sel.inset(-2), 4.0f, 1.5f, th::accent);
        canvas_.fill_rounded(handle, 3.0f, th::accent);
    }
    if (panning_) { pan_x_ += static_cast<float>(ui_.mouse_x - last_mx_); pan_y_ += static_cast<float>(ui_.mouse_y - last_my_); }
    if (wheel_ != 0 && area.contains(ui_.mouse_x, ui_.mouse_y)) {
        const float before = zoom_;
        zoom_ = std::clamp(zoom_ * (wheel_ > 0 ? 1.1f : 1.0f / 1.1f), 0.2f, 4.0f);
        const float k = zoom_ / before;
        const float cx = area.x + area.w * 0.5f + pan_x_ - (settings_.width * before) * 0.5f;
        (void)cx;
        pan_x_ *= k; pan_y_ *= k;
    }
    if (ui_.pressed && area.contains(ui_.mouse_x, ui_.mouse_y) && !ui_.blocked) {
        script_focus_ = false;
        commit_field(true);
        if (has_sel && !selected->locked && handle.inset(-4).contains(ui_.mouse_x, ui_.mouse_y)) {
            drag_ = Drag::Resize;
        } else {
            int hit = 0;
            for (auto it = order.rbegin(); it != order.rend(); ++it) {
                Rect box;
                if ((*it)->visible && hit_box(**it, ui_.mouse_x, ui_.mouse_y, box)) { hit = (*it)->id; break; }
            }
            if (hit != 0) {
                if (hit != scene_.selected_id()) select_node(hit);
                const auto* node = scene_.find(hit);
                drag_ = node && !node->locked ? Drag::Move : Drag::None;
            } else { select_node(1); drag_ = Drag::None; }
        }
        drag_start_x_ = ui_.mouse_x; drag_start_y_ = ui_.mouse_y;
        drag_undo_ = false;
        if (auto* node = scene_.find(scene_.selected_id())) { drag_orig_x_ = node->x; drag_orig_y_ = node->y; drag_orig_w_ = node->width; drag_orig_h_ = node->height; }
    }
    if (drag_ != Drag::None) {
        if (!ui_.mouse_down) drag_ = Drag::None;
        else if (auto* node = scene_.find(scene_.selected_id())) {
            const int dx = static_cast<int>(std::lround((ui_.mouse_x - drag_start_x_) / zoom_));
            const int dy = static_cast<int>(std::lround((ui_.mouse_y - drag_start_y_) / zoom_));
            if ((dx != 0 || dy != 0) && !drag_undo_) {
                const int x = node->x, y = node->y, w = node->width, h = node->height;
                node->x = drag_orig_x_; node->y = drag_orig_y_; node->width = drag_orig_w_; node->height = drag_orig_h_;
                push_undo();
                node->x = x; node->y = y; node->width = w; node->height = h;
                drag_undo_ = true;
            }
            if (drag_ == Drag::Move) { node->x = drag_orig_x_ + dx; node->y = drag_orig_y_ + dy; }
            else { node->width = std::max(8, drag_orig_w_ + dx); node->height = std::max(8, drag_orig_h_ + dy); }
            scene_.set_dirty(true);
        }
    }
    last_mx_ = ui_.mouse_x; last_my_ = ui_.mouse_y;
}

void App::render_editor() {
    const int w = canvas_.width();
    const int h = canvas_.height();
    const int tool_h = 54;
    const int right_w = 250;
    const int left_w = 290;
    const int bottom_h = std::clamp(h / 3, 190, 280);
    canvas_.fill_rect(Rect{0, 0, w, h}, th::background);
    const Rect view{left_w, tool_h, w - left_w - right_w, h - tool_h - bottom_h};
    render_viewport(view);
    ui_.blocked = show_help_;

    canvas_.fill_rect(Rect{0, 0, w, tool_h}, th::panel);
    canvas_.fill_rect(Rect{0, tool_h - 1, w, 1}, th::border);
    int x = w - 12;
    auto tool = [&](const char* id, const std::wstring& label, int width, ButtonStyle style, bool enabled = true) {
        x -= width;
        const bool clicked = ui_.button(id, Rect{x, 9, width, 36}, label, style, enabled, 15.0f);
        x -= 8;
        return clicked;
    };
    if (tool("t_save", L"حفظ", 70, ButtonStyle::Normal)) { if (save_all()) { scene_.set_dirty(false); toast(L"تم الحفظ."); } else toast(L"تعذر الحفظ.", true); }
    if (!game_.open()) { if (tool("t_play", L"▶ تشغيل", 100, ButtonStyle::Primary)) play(); }
    else if (tool("t_stop", L"■ إيقاف", 100, ButtonStyle::Danger)) stop_game();
    x -= 10;
    if (tool("t_undo", L"تراجع", 78, ButtonStyle::Normal, !undo_.empty())) undo();
    if (tool("t_redo", L"إعادة", 78, ButtonStyle::Normal, !redo_.empty())) redo();
    x -= 10;
    if (tool("t_help", L"تعليمات", 86, ButtonStyle::Ghost)) show_help_ = true;
    int lx = 12;
    if (ui_.button("t_menu", Rect{lx, 9, 96, 36}, L"القائمة", ButtonStyle::Ghost, true, 15.0f)) {
        if (save_all()) scene_.set_dirty(false);
        stop_game();
        screen_ = Screen::Menu;
        recent_ = load_recent();
        window_.set_title(L"Risha Engine - محرك ريشة");
        return;
    }
    gfx::draw_text(canvas_, settings_.name + (scene_.dirty() || code_dirty_ ? L" •" : L""), static_cast<float>(lx + 112), 33.0f, 16.0f, th::text, gfx::Anchor::Left, true);
    if (!status_.empty() && ui_.time - status_time_ < 3.5) {
        const float tw = gfx::text_width(status_, 14.0f);
        gfx::draw_text(canvas_, status_, w * 0.5f - tw * 0.1f - 60.0f, 33.0f, 14.0f, status_error_ ? th::danger : th::warning, gfx::Anchor::Center);
    }

    render_hierarchy(Rect{w - right_w, tool_h, right_w, h - tool_h});
    render_inspector(Rect{0, tool_h, left_w, h - tool_h});
    render_bottom(Rect{left_w, h - bottom_h, w - left_w - right_w, bottom_h});
    if (show_help_) {
        canvas_.fill_rect(Rect{0, 0, w, h}, Color(0, 0, 0, 170));
        const Rect box{w / 2 - 300, h / 2 - 210, 600, 420};
        canvas_.fill_rounded(box, 14.0f, th::panel);
        canvas_.stroke_rounded(box, 14.0f, 1.0f, th::border);
        const wchar_t* lines[] = {
            L"تعليمات المحرر",
            L"Ctrl+S  حفظ المشروع",
            L"Ctrl+Z / Ctrl+Y  تراجع / إعادة",
            L"Ctrl+D  تكرار الكائن المحدد",
            L"Delete  حذف الكائن المحدد",
            L"F5  تشغيل اللعبة في نافذة جديدة",
            L"F6  إيقاف اللعبة",
            L"عجلة الفأرة  تكبير وتصغير المشهد",
            L"الزر الأوسط  تحريك المشهد",
            L"اسحب الكائن لتحريكه، والمربع الأزرق لتغيير حجمه",
            L"في اللعبة: A D أو Q D أو الأسهم للحركة، المسافة للقفز، Esc للإغلاق",
            L"انقر في أي مكان للإغلاق"};
        int y = box.y + 50;
        for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
            gfx::draw_text(canvas_, lines[i], static_cast<float>(box.x + box.w - 30), static_cast<float>(y), i == 0 ? 22.0f : 15.0f, i == 0 ? th::text : th::muted, gfx::Anchor::Right, i == 0);
            y += i == 0 ? 46 : 28;
        }
        if (ui_.pressed) show_help_ = false;
    }
}

void App::render_hierarchy(Rect area) {
    canvas_.fill_rect(area, th::panel);
    canvas_.fill_rect(Rect{area.x, area.y, 1, area.h}, th::border);
    ui_.text(L"الكائنات", area.x + area.w - 16, area.y + 30, 17.0f, th::text, true);
    const wchar_t* types[] = {L"مربع", L"لاعب", L"أرض", L"مخلوق", L"كاميرا", L"نص"};
    const int bw = (area.w - 32 - 12) / 3;
    for (int i = 0; i < 6; ++i) {
        const int col = i % 3;
        const int row = i / 3;
        const Rect r{area.x + area.w - 16 - bw - col * (bw + 6), area.y + 44 + row * 38, bw, 32};
        if (ui_.button("add" + std::to_string(i), r, types[i], ButtonStyle::Normal, true, 13.0f)) add_object(types[i]);
    }
    const int half = (area.w - 32 - 6) / 2;
    if (ui_.button("dup", Rect{area.x + area.w - 16 - half, area.y + 122, half, 30}, L"تكرار", ButtonStyle::Normal, scene_.selected_id() > 1, 13.0f)) duplicate_selected();
    if (ui_.button("del", Rect{area.x + 16, area.y + 122, half, 30}, L"حذف", ButtonStyle::Danger, scene_.selected_id() > 1, 13.0f)) delete_selected();
    const Rect list{area.x + 8, area.y + 164, area.w - 16, area.h - 172};
    gfx::ClipScope clip(canvas_, list);
    std::vector<const editor::SceneNode*> rows;
    auto walk = [&](auto&& self, int parent) -> void {
        for (const auto& node : scene_.nodes()) if (node.parent_id == parent) { rows.push_back(&node); self(self, node.id); }
    };
    for (const auto& node : scene_.nodes()) if (node.parent_id == 0) { rows.push_back(&node); walk(walk, node.id); }
    const int row_h = 30;
    const int max_scroll = std::max(0, static_cast<int>(rows.size()) * row_h - list.h);
    if (wheel_ != 0 && list.contains(ui_.mouse_x, ui_.mouse_y)) hierarchy_scroll_ -= wheel_ * row_h;
    hierarchy_scroll_ = std::clamp(hierarchy_scroll_, 0, max_scroll);
    int y = list.y - hierarchy_scroll_;
    for (const auto* node : rows) {
        const Rect r{list.x, y, list.w, row_h - 2};
        y += row_h;
        if (r.y + r.h < list.y || r.y > list.y + list.h) continue;
        const bool selected = node->id == scene_.selected_id();
        const bool inside = ui_.hot(r) && list.contains(ui_.mouse_x, ui_.mouse_y);
        const std::string id = "row" + std::to_string(node->id);
        float& hv = row_hover_[id];
        hv += ((inside ? 1.0f : 0.0f) - hv) * 0.35f;
        if (std::fabs((inside ? 1.0f : 0.0f) - hv) > 0.02f) ui_.keep_animating();
        const Color fill = selected ? th::accent_soft : gfx::mix(th::panel, th::panel_raised, hv);
        if (selected || hv > 0.03f) canvas_.fill_rounded(r, 6.0f, fill);
        const int depth = std::min(4, scene_.depth_of(node->id));
        ui_.text(node->name + (node->visible ? L"" : L"  (مخفي)") , r.x + r.w - 10 - depth * 14, r.y + 19, 14.0f, node->visible ? th::text : th::muted, selected);
        ui_.text_left(node->type, r.x + 10, r.y + 19, 12.0f, th::muted);
        if (ui_.pressed && inside && !ui_.blocked) { if (node->id != scene_.selected_id()) select_node(node->id); }
    }
}

void App::render_inspector(Rect area) {
    canvas_.fill_rect(area, th::panel);
    canvas_.fill_rect(Rect{area.x + area.w - 1, area.y, 1, area.h}, th::border);
    ui_.text(L"الخصائص", area.x + area.w - 16, area.y + 30, 17.0f, th::text, true);
    auto* node = scene_.find(scene_.selected_id());
    if (!node) return;
    const int right = area.x + area.w - 16;
    const int fw = 150;
    int y = area.y + 48;
    auto row = [&](const wchar_t* label, const char* id, const std::wstring& value, bool editable = true) {
        ui_.text(label, right, y + 19, 14.0f, th::muted);
        const Rect r{right - 92 - fw, y, fw, 28};
        if (editable) field(id, r, value);
        else gfx::draw_text(canvas_, value, static_cast<float>(r.x + r.w - 8), static_cast<float>(y + 19), 14.0f, th::text, gfx::Anchor::Right);
        y += 36;
    };
    row(L"الاسم", "name", node->name, node->id != 1);
    if (node->id == 1) { ui_.text(L"النوع: عالم", right, y + 10, 13.0f, th::muted); y += 24; }
    else {
        ui_.text(L"النوع: " + node->type, right, y + 6, 13.0f, th::muted);
        y += 20;
        row(L"س", "x", std::to_wstring(node->x));
        row(L"ص", "y", std::to_wstring(node->y));
        row(L"العرض", "w", std::to_wstring(node->width));
        row(L"الارتفاع", "h", std::to_wstring(node->height));
        if (node->type == L"نص") row(L"النص", "text", node->text);
        row(L"الجاذبية", "g", number_text(node->gravity));
        ui_.text(L"الجسم", right, y + 19, 14.0f, th::muted);
        const wchar_t* bodies[] = {L"بلا", L"متحرك", L"ثابت"};
        for (int i = 0; i < 3; ++i) {
            if (ui_.button("body" + std::to_string(i), Rect{right - 92 - 56 - i * 60, y, 56, 28}, bodies[i], ButtonStyle::Toggle, true, 12.0f, node->body == i)) {
                if (node->body != i) { push_undo(); node->body = i; }
            }
        }
        y += 38;
        const int half = (area.w - 32 - 6) / 2;
        if (ui_.button("vis", Rect{right - half, y, half, 28}, node->visible ? L"ظاهر" : L"مخفي", ButtonStyle::Toggle, true, 13.0f, node->visible)) { push_undo(); node->visible = !node->visible; }
        if (ui_.button("lock", Rect{area.x + 16, y, half, 28}, node->locked ? L"مقفل" : L"غير مقفل", ButtonStyle::Toggle, true, 13.0f, node->locked)) { push_undo(); node->locked = !node->locked; }
        y += 40;
        ui_.text(L"اللون", right, y + 16, 14.0f, th::muted);
        int px = right - 70;
        for (size_t i = 0; i < sizeof(kPalette) / sizeof(kPalette[0]); ++i) {
            const Rect sw{px - 22, y, 22, 22};
            const bool inside = ui_.hot(sw);
            const Rect drawn = inside ? sw.inset(-2) : sw;
            canvas_.fill_rounded(drawn, 6.0f, Color::from_argb(kPalette[i].value));
            if (node->color == kPalette[i].value) canvas_.stroke_rounded(drawn.inset(-2), 7.0f, 1.5f, th::text);
            if (inside && ui_.pressed) { push_undo(); node->color = kPalette[i].value; }
            px -= 26;
            if (i == 3) { px = right - 70; y += 28; }
        }
        y += 34;
        const bool has_script = !node->script.empty();
        if (ui_.button("script", Rect{area.x + 16, y, area.w - 32, 32}, has_script ? L"سكربت: " + node->script : L"إنشاء سكربت للكائن", has_script ? ButtonStyle::Normal : ButtonStyle::Primary, true, 13.0f)) {
            if (!has_script) {
                push_undo();
                node->script = node->name + L".rishascript";
                std::error_code error;
                if (!fs::exists(script_path(*node), error)) write_file(script_path(*node), L"");
                code_.clear();
                code_node_ = node->id;
            }
            bottom_tab_ = 0;
            script_focus_ = true;
        }
    }
}

void App::render_bottom(Rect area) {
    canvas_.fill_rect(area, th::panel);
    canvas_.fill_rect(Rect{area.x, area.y, area.w, 1}, th::border);
    if (ui_.button("tab_code", Rect{area.x + area.w - 100, area.y + 6, 92, 30}, L"السكربت", ButtonStyle::Toggle, true, 14.0f, bottom_tab_ == 0)) bottom_tab_ = 0;
    if (ui_.button("tab_out", Rect{area.x + area.w - 200, area.y + 6, 92, 30}, L"المخرجات", ButtonStyle::Toggle, true, 14.0f, bottom_tab_ == 1)) bottom_tab_ = 1;
    const Rect body{area.x + 8, area.y + 42, area.w - 16, area.h - 50};
    canvas_.fill_rounded(body, 8.0f, Color(10, 14, 21));
    gfx::ClipScope clip(canvas_, body.inset(2));
    const int line_h = 22;
    if (bottom_tab_ == 1) {
        if (ui_.button("clear_out", Rect{area.x + 12, area.y + 6, 90, 30}, L"مسح", ButtonStyle::Ghost, true, 13.0f)) output_.clear();
        const int visible = body.h / line_h;
        int y = body.y + 18;
        const int start = std::max(0, static_cast<int>(output_.size()) - visible);
        for (size_t i = static_cast<size_t>(start); i < output_.size(); ++i) {
            ui_.text(output_[i].text, body.x + body.w - 12, y, 14.0f, output_[i].error ? th::danger : th::text);
            y += line_h;
        }
        if (output_.empty()) ui_.text(L"لا توجد مخرجات. اضغط تشغيل لتجربة اللعبة.", body.x + body.w - 12, body.y + 22, 14.0f, th::muted);
        return;
    }
    const auto* node = scene_.find(scene_.selected_id());
    if (!node || node->script.empty()) {
        ui_.text(L"لا يوجد سكربت لهذا الكائن. أنشئ سكربت من لوحة الخصائص.", body.x + body.w - 12, body.y + 26, 15.0f, th::muted);
        return;
    }
    if (!compile_error_.empty()) gfx::draw_text(canvas_, compile_error_, static_cast<float>(area.x + 12), static_cast<float>(area.y + 26), 13.0f, th::danger, gfx::Anchor::Left);
    else gfx::draw_text(canvas_, L"السكربت سليم", static_cast<float>(area.x + 12), static_cast<float>(area.y + 26), 13.0f, Color(110, 210, 140), gfx::Anchor::Left);
    std::vector<std::pair<size_t, size_t>> lines;
    size_t start = 0;
    for (size_t i = 0; i <= code_.size(); ++i) if (i == code_.size() || code_[i] == L'\n') { lines.push_back({start, i}); start = i + 1; }
    const int visible = std::max(1, body.h / line_h);
    int cursor_line = 0;
    for (size_t i = 0; i < lines.size(); ++i) if (cursor_ >= lines[i].first && cursor_ <= lines[i].second) { cursor_line = static_cast<int>(i); break; }
    if (cursor_line < scroll_line_) scroll_line_ = cursor_line;
    if (cursor_line >= scroll_line_ + visible) scroll_line_ = cursor_line - visible + 1;
    if (wheel_ != 0 && body.contains(ui_.mouse_x, ui_.mouse_y)) scroll_line_ -= wheel_ * 3;
    scroll_line_ = std::clamp(scroll_line_, 0, std::max(0, static_cast<int>(lines.size()) - 1));
    const int number_w = 44;
    const int text_right = body.x + body.w - number_w - 10;
    canvas_.fill_rect(Rect{body.x + body.w - number_w, body.y, number_w, body.h}, Color(14, 19, 28));
    if (ui_.pressed && body.contains(ui_.mouse_x, ui_.mouse_y)) {
        commit_field(true);
        script_focus_ = true;
        int line = std::clamp(scroll_line_ + (ui_.mouse_y - body.y) / line_h, 0, static_cast<int>(lines.size()) - 1);
        const std::wstring text = code_.substr(lines[static_cast<size_t>(line)].first, lines[static_cast<size_t>(line)].second - lines[static_cast<size_t>(line)].first);
        const auto& layout = gfx::text_layout(text, 15.0f);
        const float left = layout.rtl ? static_cast<float>(text_right) - layout.width : static_cast<float>(body.x + 10);
        cursor_ = lines[static_cast<size_t>(line)].first + gfx::TextEngine::instance().caret_index_at(layout, static_cast<float>(ui_.mouse_x) - left);
    }
    int y = body.y + 17;
    for (int i = scroll_line_; i < static_cast<int>(lines.size()) && y < body.y + body.h + line_h; ++i, y += line_h) {
        const std::wstring text = code_.substr(lines[static_cast<size_t>(i)].first, lines[static_cast<size_t>(i)].second - lines[static_cast<size_t>(i)].first);
        const auto& layout = gfx::text_layout(text, 15.0f);
        const float left = layout.rtl ? static_cast<float>(text_right) - layout.width : static_cast<float>(body.x + 10);
        gfx::draw_text(canvas_, std::to_wstring(i + 1), static_cast<float>(body.x + body.w - number_w / 2), static_cast<float>(y), 12.0f, th::muted, gfx::Anchor::Center, false, gfx::Direction::LeftToRight);
        gfx::draw_text(canvas_, text, left, static_cast<float>(y), 15.0f, th::text, gfx::Anchor::Left, false);
        if (i == cursor_line && script_focus_ && std::fmod(ui_.time, 1.0) < 0.55) {
            const size_t col = std::min(cursor_ - lines[static_cast<size_t>(i)].first, layout.caret.size() ? layout.caret.size() - 1 : 0);
            const float cx = left + (layout.caret.empty() ? 0.0f : layout.caret[col]);
            canvas_.fill_rect(Rect{static_cast<int>(cx), y - 15, 2, 19}, th::accent);
        }
    }
}

}
