#pragma once
#include <string>
#include <vector>

namespace risha::editor {

struct SceneNode {
    int id{0};
    int parent_id{0};
    std::wstring name;
    std::wstring type;
    int x{0};
    int y{0};
    int width{120};
    int height{80};
    bool visible{true};
    bool locked{false};
    float opacity{1.0f};
    float rotation{0.0f};
    float scale_x{1.0f};
    float scale_y{1.0f};
    int z{0};
    unsigned int color{0xFF6EA8FEu};
    bool flip_x{false};
    bool flip_y{false};
    std::wstring resource;
    std::wstring script;
    float velocity_x{0.0f};
    float velocity_y{0.0f};
    float gravity{0.0f};
    float camera_zoom{1.0f};
    bool active_camera{false};
    int collision_layer{1};
    int collision_mask{1};
    bool audio_loop{false};
    float audio_volume{1.0f};
    int body{0};
    std::wstring text;
    bool grounded{false};
    float remainder_x{0.0f};
    float remainder_y{0.0f};
};

struct SceneSnapshot {
    std::vector<SceneNode> nodes;
    int next_id{2};
    int selected_id{1};
};

class Scene final {
public:
    Scene();
    void clear();
    int add_node(int parent_id, std::wstring name, std::wstring type);
    bool remove_node(int id);
    bool rename_node(int id, std::wstring name);
    bool reparent_node(int id, int parent_id);
    int duplicate_node(int id);
    SceneNode* find(int id);
    const SceneNode* find(int id) const;
    std::vector<SceneNode>& nodes() noexcept;
    const std::vector<SceneNode>& nodes() const noexcept;
    bool save(const std::wstring& path);
    bool load(const std::wstring& path);
    int selected_id() const noexcept;
    void set_selected_id(int id) noexcept;
    const std::wstring& path() const noexcept;
    bool dirty() const noexcept;
    void set_dirty(bool value) noexcept;
    SceneSnapshot capture() const;
    void restore(const SceneSnapshot& snapshot);
    int depth_of(int id) const;
    bool is_descendant(int id, int ancestor_id) const;

private:
    int next_id_{1};
    int selected_id_{1};
    std::vector<SceneNode> nodes_;
    std::wstring path_;
    bool dirty_{false};
};

}
