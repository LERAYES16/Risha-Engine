#include "risha/runtime/Runtime.hpp"
#include <algorithm>
#include <cmath>

namespace risha::runtime {
namespace {

using editor::SceneNode;
using script::Value;

struct Box {
    double x, y, w, h;
};

Box box_of(const SceneNode& node) {
    return Box{static_cast<double>(node.x), static_cast<double>(node.y),
               std::max(1.0, node.width * static_cast<double>(std::fabs(node.scale_x))),
               std::max(1.0, node.height * static_cast<double>(std::fabs(node.scale_y)))};
}

bool overlaps(const Box& a, const Box& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool is_camera(const SceneNode& node) {
    return node.type.find(L"كاميرا") != std::wstring::npos;
}

bool text_arg(const std::vector<Value>& args, size_t index) {
    return index < args.size() && args[index].type == Value::Type::String;
}

bool number_arg(const std::vector<Value>& args, size_t index) {
    return index < args.size() && args[index].type == Value::Type::Number;
}

int clamp_channel(double value) {
    return static_cast<int>(std::clamp(std::lround(value), 0L, 255L));
}

}

bool Runtime::load_scene(const std::wstring& path) {
    editor::Scene loaded;
    if (!loaded.load(path)) return false;
    return load_scene(loaded);
}

bool Runtime::load_scene(const editor::Scene& scene) {
    scene_ = scene;
    scripts_.clear();
    return true;
}

bool Runtime::play() {
    if (scene_.nodes().size() < 2) return false;
    elapsed_ = 0.0;
    delta_ = 0.0;
    frame_ = 0;
    output_.clear();
    diagnostics_.clear();
    messages_.clear();
    stop_requested_ = false;
    running_ = true;
    random_.seed(12345u);
    for (auto& item : input_) item.second = {};
    for (auto& attached : scripts_) {
        attached.globals.clear();
        attached.failed = false;
        attached.first_frame = -1;
    }
    for (auto& node : scene_.nodes()) {
        node.grounded = false;
        node.remainder_x = 0.0f;
        node.remainder_y = 0.0f;
    }
    return true;
}

void Runtime::stop() noexcept {
    running_ = false;
    delta_ = 0.0;
}

bool Runtime::running() const noexcept { return running_; }
bool Runtime::stop_requested() const noexcept { return stop_requested_; }

SceneNode* Runtime::find_by_name(const std::wstring& name) {
    for (auto& node : scene_.nodes()) if (node.name == name) return &node;
    return nullptr;
}

void Runtime::move_node(SceneNode& node, double dx, double dy) {
    const double total_x = node.remainder_x + dx;
    const double total_y = node.remainder_y + dy;
    const double whole_x = std::trunc(total_x);
    const double whole_y = std::trunc(total_y);
    node.x += static_cast<int>(whole_x);
    node.y += static_cast<int>(whole_y);
    node.remainder_x = static_cast<float>(total_x - whole_x);
    node.remainder_y = static_cast<float>(total_y - whole_y);
}

void Runtime::resolve_solids(SceneNode& node) {
    for (int pass = 0; pass < 4; ++pass) {
        bool moved = false;
        for (const auto& other : scene_.nodes()) {
            if (other.id == node.id || other.id == 1 || other.body != 2 || !other.visible) continue;
            if ((node.collision_mask & other.collision_layer) == 0) continue;
            const Box a = box_of(node);
            const Box b = box_of(other);
            if (!overlaps(a, b)) continue;
            const double push_left = a.x + a.w - b.x;
            const double push_right = b.x + b.w - a.x;
            const double push_up = a.y + a.h - b.y;
            const double push_down = b.y + b.h - a.y;
            const double horizontal = std::min(push_left, push_right);
            const double vertical = std::min(push_up, push_down);
            if (horizontal < vertical) {
                node.x += static_cast<int>(push_left < push_right ? -std::ceil(push_left) : std::ceil(push_right));
                node.velocity_x = 0.0f;
            } else if (push_up <= push_down) {
                node.y -= static_cast<int>(std::ceil(push_up));
                if (node.velocity_y > 0.0f) node.velocity_y = 0.0f;
                node.remainder_y = 0.0f;
                node.grounded = true;
            } else {
                node.y += static_cast<int>(std::ceil(push_down));
                if (node.velocity_y < 0.0f) node.velocity_y = 0.0f;
                node.remainder_y = 0.0f;
            }
            moved = true;
        }
        if (!moved) break;
    }
    if (!node.grounded && node.velocity_y >= 0.0f) {
        Box probe = box_of(node);
        probe.y += 1.0;
        for (const auto& other : scene_.nodes()) {
            if (other.id == node.id || other.id == 1 || other.body != 2 || !other.visible) continue;
            if ((node.collision_mask & other.collision_layer) == 0) continue;
            if (overlaps(probe, box_of(other))) { node.grounded = true; break; }
        }
    }
}

void Runtime::physics(double delta_seconds) {
    for (auto& node : scene_.nodes()) {
        if (node.id == 1 || !node.visible) continue;
        node.grounded = false;
        if (node.body == 2) continue;
        node.velocity_y += node.gravity * static_cast<float>(delta_seconds);
        move_node(node, node.velocity_x * delta_seconds, node.velocity_y * delta_seconds);
        if (node.body == 1) resolve_solids(node);
    }
}

void Runtime::update(double delta_seconds) {
    if (!running_) return;
    delta_ = std::clamp(delta_seconds, 0.0, 0.25);
    elapsed_ += delta_;
    ++frame_;
    physics(delta_);
}

void Runtime::add_message(int node_id, int line, std::wstring text, bool error) {
    if (messages_.size() >= 300) messages_.erase(messages_.begin(), messages_.begin() + 100);
    std::wstring name;
    if (const auto* node = scene_.find(node_id)) name = node->name;
    messages_.push_back(ScriptMessage{node_id, std::move(name), line, std::move(text), error});
}

bool Runtime::attach_script(int node_id, const std::wstring& source) {
    if (!scene_.find(node_id)) return false;
    AttachedScript attached;
    attached.node_id = node_id;
    script::Compiler compiler;
    std::vector<script::Diagnostic> errors;
    if (!compiler.compile(source, attached.program, errors)) {
        for (const auto& item : errors) add_message(node_id, item.line, item.message, true);
        attached.failed = true;
        scripts_.push_back(std::move(attached));
        return false;
    }
    scripts_.push_back(std::move(attached));
    return true;
}

void Runtime::step(double delta_seconds) {
    if (!running_) return;
    delta_ = std::clamp(delta_seconds, 0.0, 0.25);
    elapsed_ += delta_;
    ++frame_;
    script::VirtualMachine vm;
    const auto native = native_function();
    for (auto& attached : scripts_) {
        if (attached.failed || !scene_.find(attached.node_id)) continue;
        if (attached.first_frame < 0) attached.first_frame = frame_;
        current_node_ = attached.node_id;
        current_script_first_frame_ = attached.first_frame;
        std::vector<std::wstring> lines;
        std::vector<script::Diagnostic> errors;
        const bool ok = vm.run(attached.program, lines, errors, native, &attached.globals);
        for (auto& line : lines) add_message(attached.node_id, 0, std::move(line), false);
        if (!ok) {
            for (const auto& item : errors) add_message(attached.node_id, item.line, item.message, true);
            attached.failed = true;
        }
    }
    current_node_ = 0;
    physics(delta_);
}

bool Runtime::run_script(const std::wstring& source, int object_id) {
    if (object_id > 0 && !scene_.find(object_id)) return false;
    script::Compiler compiler;
    script::Program program;
    if (!compiler.compile(source, program, diagnostics_)) return false;
    script::VirtualMachine vm;
    current_node_ = object_id;
    current_script_first_frame_ = frame_;
    const bool ok = vm.run(program, output_, diagnostics_, native_function());
    current_node_ = 0;
    return ok;
}

void Runtime::set_action(std::wstring name, bool held) {
    auto& state = input_[std::move(name)];
    if (held && !state.held) state.pressed = true;
    if (!held && state.held) state.released = true;
    state.held = held;
}

void Runtime::set_action_pressed(std::wstring name) {
    auto& state = input_[std::move(name)];
    state.pressed = true;
    state.held = true;
}

void Runtime::clear_input_edges() noexcept {
    for (auto& item : input_) {
        item.second.pressed = false;
        item.second.released = false;
    }
}

editor::Scene& Runtime::scene() noexcept { return scene_; }
const editor::Scene& Runtime::scene() const noexcept { return scene_; }
const std::vector<std::wstring>& Runtime::output() const noexcept { return output_; }
const std::vector<script::Diagnostic>& Runtime::diagnostics() const noexcept { return diagnostics_; }
const std::vector<ScriptMessage>& Runtime::messages() const noexcept { return messages_; }
void Runtime::clear_messages() noexcept { messages_.clear(); }
void Runtime::clear_frame_messages() noexcept { output_.clear(); diagnostics_.clear(); }
double Runtime::elapsed_seconds() const noexcept { return elapsed_; }
double Runtime::delta_seconds() const noexcept { return delta_; }
long long Runtime::frame() const noexcept { return frame_; }
const std::unordered_map<std::wstring, InputState>& Runtime::input() const noexcept { return input_; }
void Runtime::seed(unsigned int value) { random_.seed(value); }

Camera Runtime::camera() const {
    Camera result;
    const SceneNode* chosen = nullptr;
    for (const auto& node : scene_.nodes()) {
        if (node.id == 1 || !is_camera(node)) continue;
        if (node.active_camera) { chosen = &node; break; }
        if (!chosen) chosen = &node;
    }
    if (!chosen) return result;
    result.active = true;
    result.center_x = chosen->x + chosen->width * 0.5;
    result.center_y = chosen->y + chosen->height * 0.5;
    result.zoom = std::max(0.1, static_cast<double>(chosen->camera_zoom));
    return result;
}

script::NativeFunction Runtime::native_function() {
    return [this](std::wstring_view name, const std::vector<Value>& args, Value& result, std::wstring& error) {
        auto done_number = [&](double value) { result = Value::from_number(value); return true; };
        auto done_bool = [&](bool value) { result = Value::from_bool(value); return true; };
        auto missing = [&](const std::wstring& object) { error = L"الكائن غير موجود: " + object; return false; };

        if (name == L"وقت" && args.empty()) return done_number(elapsed_);
        if (name == L"دلتا" && args.empty()) return done_number(delta_);
        if (name == L"الإطار" && args.empty()) return done_number(static_cast<double>(frame_));
        if (name == L"أول_إطار" && args.empty()) return done_bool(frame_ == current_script_first_frame_);
        if (name == L"عرض_العالم" && args.empty()) return done_number(scene_.find(1)->width);
        if (name == L"ارتفاع_العالم" && args.empty()) return done_number(scene_.find(1)->height);
        if (name == L"أوقف_اللعبة" && args.empty()) { stop_requested_ = true; return done_bool(true); }
        if ((name == L"زر_مضغوط" || name == L"زر_مستمر" || name == L"زر_محرر") && args.size() == 1 && text_arg(args, 0)) {
            const auto found = input_.find(args[0].text);
            bool value = false;
            if (found != input_.end()) value = name == L"زر_مستمر" ? found->second.held : (name == L"زر_مضغوط" ? found->second.pressed : found->second.released);
            return done_bool(value);
        }
        if (name == L"عشوائي" && args.size() == 2 && number_arg(args, 0) && number_arg(args, 1)) {
            double low = args[0].number;
            double high = args[1].number;
            if (low > high) std::swap(low, high);
            if (low == std::floor(low) && high == std::floor(high)) {
                std::uniform_int_distribution<long long> distribution(static_cast<long long>(low), static_cast<long long>(high));
                return done_number(static_cast<double>(distribution(random_)));
            }
            std::uniform_real_distribution<double> distribution(low, high);
            return done_number(distribution(random_));
        }
        if (name == L"أنشئ" && (args.size() == 4 || args.size() == 6) && text_arg(args, 0) && text_arg(args, 1) && number_arg(args, 2) && number_arg(args, 3) && (args.size() == 4 || (number_arg(args, 4) && number_arg(args, 5)))) {
            const int id = scene_.add_node(1, args[0].text, args[1].text);
            SceneNode* node = scene_.find(id);
            node->x = static_cast<int>(std::lround(args[2].number));
            node->y = static_cast<int>(std::lround(args[3].number));
            node->width = args.size() == 6 ? std::max(1, static_cast<int>(std::lround(args[4].number))) : 48;
            node->height = args.size() == 6 ? std::max(1, static_cast<int>(std::lround(args[5].number))) : 48;
            if (node->type == L"أرض") { node->body = 2; node->color = 0xFF47865Bu; }
            result = Value::from_text(node->name);
            return true;
        }

        if (!args.empty() && text_arg(args, 0)) {
            SceneNode* node = find_by_name(args[0].text);
            const bool known = name == L"حرك" || name == L"ضع_موضع" || name == L"موضع_س" || name == L"موضع_ص" ||
                name == L"عرض" || name == L"ارتفاع" || name == L"سرعة" || name == L"سرعة_س" || name == L"سرعة_ص" ||
                name == L"اضبط_سرعة" || name == L"اقفز" || name == L"على_الأرض" || name == L"يتصادم" ||
                name == L"الجاذبية" || name == L"أظهر" || name == L"أخف" || name == L"لون" || name == L"اضبط_نص" ||
                name == L"احذف" || name == L"موجود" || name == L"اضبط_الحجم" || name == L"دوّر" || name == L"الشفافية" ||
                name == L"اضبط_الجسم";
            if (known) {
                if (name == L"موجود" && args.size() == 1) return done_bool(node != nullptr);
                if (!node) return missing(args[0].text);
                if (node->id == 1 && name != L"عرض" && name != L"ارتفاع") { error = L"لا يمكن تعديل العالم من السكربت."; return false; }
                if (name == L"حرك" && args.size() == 3 && number_arg(args, 1) && number_arg(args, 2)) { move_node(*node, args[1].number, args[2].number); return done_bool(true); }
                if (name == L"ضع_موضع" && args.size() == 3 && number_arg(args, 1) && number_arg(args, 2)) {
                    node->x = static_cast<int>(std::lround(args[1].number));
                    node->y = static_cast<int>(std::lround(args[2].number));
                    node->remainder_x = node->remainder_y = 0.0f;
                    return done_bool(true);
                }
                if (name == L"موضع_س" && args.size() == 1) return done_number(node->x);
                if (name == L"موضع_ص" && args.size() == 1) return done_number(node->y);
                if (name == L"عرض" && args.size() == 1) return done_number(node->width * static_cast<double>(std::fabs(node->scale_x)));
                if (name == L"ارتفاع" && args.size() == 1) return done_number(node->height * static_cast<double>(std::fabs(node->scale_y)));
                if (name == L"سرعة" && args.size() == 2 && number_arg(args, 1)) { node->velocity_x = static_cast<float>(args[1].number); return done_bool(true); }
                if (name == L"سرعة_س" && args.size() == 1) return done_number(node->velocity_x);
                if (name == L"سرعة_ص" && args.size() == 1) return done_number(node->velocity_y);
                if (name == L"اضبط_سرعة" && args.size() == 3 && number_arg(args, 1) && number_arg(args, 2)) {
                    node->velocity_x = static_cast<float>(args[1].number);
                    node->velocity_y = static_cast<float>(args[2].number);
                    return done_bool(true);
                }
                if (name == L"اقفز" && args.size() == 2 && number_arg(args, 1)) {
                    if (!node->grounded) return done_bool(false);
                    node->velocity_y = -static_cast<float>(std::fabs(args[1].number));
                    node->grounded = false;
                    return done_bool(true);
                }
                if (name == L"على_الأرض" && args.size() == 1) return done_bool(node->grounded);
                if (name == L"يتصادم" && args.size() == 2 && text_arg(args, 1)) {
                    const SceneNode* other = find_by_name(args[1].text);
                    if (!other) return missing(args[1].text);
                    return done_bool(node->id != other->id && node->visible && other->visible && overlaps(box_of(*node), box_of(*other)));
                }
                if (name == L"الجاذبية" && args.size() == 2 && number_arg(args, 1)) { node->gravity = static_cast<float>(args[1].number); return done_bool(true); }
                if (name == L"أظهر" && args.size() == 1) { node->visible = true; return done_bool(true); }
                if (name == L"أخف" && args.size() == 1) { node->visible = false; return done_bool(true); }
                if (name == L"لون" && args.size() == 4 && number_arg(args, 1) && number_arg(args, 2) && number_arg(args, 3)) {
                    node->color = 0xFF000000u | (static_cast<unsigned>(clamp_channel(args[1].number)) << 16) | (static_cast<unsigned>(clamp_channel(args[2].number)) << 8) | static_cast<unsigned>(clamp_channel(args[3].number));
                    return done_bool(true);
                }
                if (name == L"اضبط_نص" && args.size() == 2 && text_arg(args, 1)) { node->text = args[1].text; return done_bool(true); }
                if (name == L"اضبط_الحجم" && args.size() == 3 && number_arg(args, 1) && number_arg(args, 2)) {
                    node->width = std::max(1, static_cast<int>(std::lround(args[1].number)));
                    node->height = std::max(1, static_cast<int>(std::lround(args[2].number)));
                    return done_bool(true);
                }
                if (name == L"دوّر" && args.size() == 2 && number_arg(args, 1)) { node->rotation = static_cast<float>(args[1].number); return done_bool(true); }
                if (name == L"الشفافية" && args.size() == 2 && number_arg(args, 1)) { node->opacity = static_cast<float>(std::clamp(args[1].number, 0.0, 1.0)); return done_bool(true); }
                if (name == L"اضبط_الجسم" && args.size() == 2 && number_arg(args, 1)) { node->body = std::clamp(static_cast<int>(std::lround(args[1].number)), 0, 2); return done_bool(true); }
                if (name == L"احذف" && args.size() == 1) {
                    const int id = node->id;
                    if (id == current_node_) { error = L"لا يمكن للسكربت حذف كائنه الخاص."; return false; }
                    scene_.remove_node(id);
                    return done_bool(true);
                }
                error = L"معاملات الدالة " + std::wstring(name) + L" غير صالحة.";
                return false;
            }
        }
        error = L"دالة المحرك غير معروفة: " + std::wstring(name);
        return false;
    };
}

}
