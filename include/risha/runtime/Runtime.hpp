#pragma once
#include "risha/editor/Scene.hpp"
#include "risha/script/Script.hpp"
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace risha::runtime {

struct InputState {
    bool pressed{false};
    bool held{false};
    bool released{false};
};

struct Camera {
    bool active{false};
    double center_x{0.0};
    double center_y{0.0};
    double zoom{1.0};
};

struct ScriptMessage {
    int node_id{0};
    std::wstring node_name;
    int line{0};
    std::wstring text;
    bool error{false};
};

class Runtime final {
public:
    bool load_scene(const std::wstring& path);
    bool load_scene(const editor::Scene& scene);
    bool play();
    void stop() noexcept;
    bool running() const noexcept;
    bool stop_requested() const noexcept;
    void update(double delta_seconds);
    void step(double delta_seconds);
    bool attach_script(int node_id, const std::wstring& source);
    bool run_script(const std::wstring& source, int object_id = 0);
    void set_action(std::wstring name, bool held);
    void set_action_pressed(std::wstring name);
    void clear_input_edges() noexcept;
    editor::Scene& scene() noexcept;
    const editor::Scene& scene() const noexcept;
    const std::vector<std::wstring>& output() const noexcept;
    const std::vector<script::Diagnostic>& diagnostics() const noexcept;
    const std::vector<ScriptMessage>& messages() const noexcept;
    void clear_messages() noexcept;
    void clear_frame_messages() noexcept;
    double elapsed_seconds() const noexcept;
    double delta_seconds() const noexcept;
    long long frame() const noexcept;
    Camera camera() const;
    const std::unordered_map<std::wstring, InputState>& input() const noexcept;
    void seed(unsigned int value);

private:
    struct AttachedScript {
        int node_id{0};
        script::Program program;
        script::Globals globals;
        bool failed{false};
        long long first_frame{-1};
    };

    script::NativeFunction native_function();
    editor::SceneNode* find_by_name(const std::wstring& name);
    void physics(double delta_seconds);
    void resolve_solids(editor::SceneNode& node);
    void move_node(editor::SceneNode& node, double dx, double dy);
    void add_message(int node_id, int line, std::wstring text, bool error);
    editor::Scene scene_;
    std::unordered_map<std::wstring, InputState> input_;
    std::vector<std::wstring> output_;
    std::vector<script::Diagnostic> diagnostics_;
    std::vector<ScriptMessage> messages_;
    std::vector<AttachedScript> scripts_;
    std::mt19937 random_{12345u};
    int current_node_{0};
    long long current_script_first_frame_{0};
    double elapsed_{0.0};
    double delta_{0.0};
    long long frame_{0};
    bool running_{false};
    bool stop_requested_{false};
};

}
