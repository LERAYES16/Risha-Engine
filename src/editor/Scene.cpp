#include "risha/editor/Scene.hpp"
#include "risha/core/Text.hpp"
#include "risha/core/Paths.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <unordered_set>

namespace risha::editor {
namespace {

std::vector<std::string> split(const std::string& value) {
    std::vector<std::string> result;
    size_t start = 0;
    while (true) {
        const size_t end = value.find('|', start);
        result.push_back(value.substr(start, end == std::string::npos ? std::string::npos : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

bool to_int(const std::string& text, int& value) {
    long long parsed = 0;
    if (!core::parse_int(text, parsed) || parsed < -2000000000LL || parsed > 2000000000LL) return false;
    value = static_cast<int>(parsed);
    return true;
}

bool to_float(const std::string& text, float& value) {
    double parsed = 0.0;
    if (!core::parse_double(text, parsed)) return false;
    value = static_cast<float>(parsed);
    return true;
}

bool to_bool(const std::string& text, bool& value) {
    int parsed = 0;
    if (!to_int(text, parsed)) return false;
    value = parsed != 0;
    return true;
}

bool to_unsigned(const std::string& text, unsigned int& value) {
    long long parsed = 0;
    if (!core::parse_int(text, parsed) || parsed < 0 || parsed > 0xFFFFFFFFLL) return false;
    value = static_cast<unsigned int>(parsed);
    return true;
}

bool parse_node(const std::vector<std::string>& parts, SceneNode& node) {
    const size_t count = parts.size();
    if (count != 9 && count != 27 && count != 29 && count != 31) return false;
    bool ok = to_int(parts[0], node.id) && to_int(parts[1], node.parent_id);
    ok = ok && core::decode_hex(parts[2], node.name) && core::decode_hex(parts[3], node.type);
    ok = ok && to_int(parts[4], node.x) && to_int(parts[5], node.y) && to_int(parts[6], node.width) && to_int(parts[7], node.height);
    ok = ok && to_bool(parts[8], node.visible);
    if (!ok) return false;
    if (node.type.empty()) node.type = L"كائن";
    if (count == 9) return true;
    const size_t offset = count >= 29 ? 2 : 0;
    if (count >= 29) ok = ok && to_bool(parts[9], node.locked) && to_float(parts[10], node.opacity);
    ok = ok && to_float(parts[9 + offset], node.rotation) && to_float(parts[10 + offset], node.scale_x) && to_float(parts[11 + offset], node.scale_y);
    ok = ok && to_int(parts[12 + offset], node.z) && to_unsigned(parts[13 + offset], node.color);
    ok = ok && to_bool(parts[14 + offset], node.flip_x) && to_bool(parts[15 + offset], node.flip_y);
    ok = ok && core::decode_hex(parts[16 + offset], node.resource) && core::decode_hex(parts[17 + offset], node.script);
    ok = ok && to_float(parts[18 + offset], node.velocity_x) && to_float(parts[19 + offset], node.velocity_y) && to_float(parts[20 + offset], node.gravity);
    ok = ok && to_float(parts[21 + offset], node.camera_zoom) && to_bool(parts[22 + offset], node.active_camera);
    ok = ok && to_int(parts[23 + offset], node.collision_layer) && to_int(parts[24 + offset], node.collision_mask);
    ok = ok && to_bool(parts[25 + offset], node.audio_loop) && to_float(parts[26 + offset], node.audio_volume);
    if (count == 31) ok = ok && to_int(parts[29], node.body) && core::decode_hex(parts[30], node.text);
    return ok;
}

}

Scene::Scene() {
    clear();
}

void Scene::clear() {
    nodes_.clear();
    SceneNode root;
    root.id = 1;
    root.parent_id = 0;
    root.name = L"العالم";
    root.type = L"عالم";
    root.width = 960;
    root.height = 540;
    nodes_.push_back(std::move(root));
    next_id_ = 2;
    selected_id_ = 1;
    path_.clear();
    dirty_ = false;
}

int Scene::add_node(int parent_id, std::wstring name, std::wstring type) {
    if (!find(parent_id)) parent_id = 1;
    const int id = next_id_++;
    SceneNode node;
    node.id = id;
    node.parent_id = parent_id;
    node.type = std::move(type);
    if (node.type.empty()) node.type = L"كائن";
    if (name.empty()) name = node.type;
    const std::wstring base = name;
    int suffix = 2;
    while (std::any_of(nodes_.begin(), nodes_.end(), [&](const SceneNode& other) { return other.name == name; })) {
        name = base + L" " + std::to_wstring(suffix++);
    }
    node.name = std::move(name);
    node.x = 40;
    node.y = 40;
    node.width = 120;
    node.height = 80;
    nodes_.push_back(std::move(node));
    selected_id_ = id;
    dirty_ = true;
    return id;
}

bool Scene::remove_node(int id) {
    if (id == 1 || !find(id)) return false;
    std::vector<int> removed{id};
    for (size_t i = 0; i < removed.size(); ++i) {
        for (const SceneNode& node : nodes_) {
            if (node.parent_id == removed[i]) removed.push_back(node.id);
        }
    }
    nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), [&](const SceneNode& node) {
        return std::find(removed.begin(), removed.end(), node.id) != removed.end();
    }), nodes_.end());
    if (!find(selected_id_)) selected_id_ = 1;
    dirty_ = true;
    return true;
}

bool Scene::rename_node(int id, std::wstring name) {
    auto* node = find(id);
    if (!node || id == 1 || name.empty()) return false;
    for (const auto& other : nodes_) if (other.id != id && other.name == name) return false;
    node->name = std::move(name);
    dirty_ = true;
    return true;
}

bool Scene::reparent_node(int id, int parent_id) {
    if (id == 1 || id == parent_id || !find(id) || !find(parent_id)) return false;
    int cursor = parent_id;
    while (cursor != 0) {
        if (cursor == id) return false;
        const auto* node = find(cursor);
        if (!node) return false;
        cursor = node->parent_id;
    }
    find(id)->parent_id = parent_id;
    dirty_ = true;
    return true;
}

int Scene::duplicate_node(int id) {
    if (id == 1) return 0;
    const auto* source = find(id);
    if (!source) return 0;
    SceneNode copy = *source;
    copy.id = next_id_++;
    const std::wstring base = source->name + L" نسخة";
    copy.name = base;
    int suffix = 2;
    while (std::any_of(nodes_.begin(), nodes_.end(), [&](const SceneNode& node) { return node.name == copy.name; })) {
        copy.name = base + L" " + std::to_wstring(suffix++);
    }
    copy.x += 24;
    copy.y += 24;
    nodes_.push_back(std::move(copy));
    selected_id_ = nodes_.back().id;
    dirty_ = true;
    return selected_id_;
}

SceneNode* Scene::find(int id) {
    for (SceneNode& node : nodes_) if (node.id == id) return &node;
    return nullptr;
}

const SceneNode* Scene::find(int id) const {
    for (const SceneNode& node : nodes_) if (node.id == id) return &node;
    return nullptr;
}

std::vector<SceneNode>& Scene::nodes() noexcept { return nodes_; }
const std::vector<SceneNode>& Scene::nodes() const noexcept { return nodes_; }

bool Scene::save(const std::wstring& path) {
    std::ofstream file(core::make_path(path), std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.imbue(std::locale::classic());
    file.precision(9);
    file << "RISHA_SCENE_0_5\n";
    for (const SceneNode& node : nodes_) {
        file << node.id << '|' << node.parent_id << '|' << core::encode_hex(node.name) << '|' << core::encode_hex(node.type) << '|'
             << node.x << '|' << node.y << '|' << node.width << '|' << node.height << '|'
             << (node.visible ? 1 : 0) << '|' << (node.locked ? 1 : 0) << '|' << node.opacity << '|'
             << node.rotation << '|' << node.scale_x << '|' << node.scale_y << '|'
             << node.z << '|' << node.color << '|' << (node.flip_x ? 1 : 0) << '|' << (node.flip_y ? 1 : 0) << '|'
             << core::encode_hex(node.resource) << '|' << core::encode_hex(node.script) << '|'
             << node.velocity_x << '|' << node.velocity_y << '|' << node.gravity << '|'
             << node.camera_zoom << '|' << (node.active_camera ? 1 : 0) << '|'
             << node.collision_layer << '|' << node.collision_mask << '|'
             << (node.audio_loop ? 1 : 0) << '|' << node.audio_volume << '|'
             << node.body << '|' << core::encode_hex(node.text) << '\n';
    }
    file.flush();
    const bool ok = static_cast<bool>(file);
    if (ok) {
        path_ = path;
        dirty_ = false;
    }
    return ok;
}

bool Scene::load(const std::wstring& path) {
    std::ifstream file(core::make_path(path), std::ios::binary);
    if (!file) return false;
    std::string header;
    if (!std::getline(file, header)) return false;
    while (!header.empty() && (header.back() == '\r' || header.back() == ' ')) header.pop_back();
    if (header != "RISHA_SCENE_0_5" && header != "RISHA_SCENE_0_4" && header != "RISHA_SCENE_0_3") return false;

    std::vector<SceneNode> loaded;
    int maximum_id = 0;
    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        SceneNode node;
        if (!parse_node(split(line), node)) return false;
        if (node.id <= 0 || node.width <= 0 || node.height <= 0 || node.width > 100000 || node.height > 100000) return false;
        if (node.id == 1) {
            node.name = L"العالم";
            node.type = L"عالم";
            node.parent_id = 0;
        }
        maximum_id = std::max(maximum_id, node.id);
        loaded.push_back(std::move(node));
    }

    if (loaded.empty()) return false;
    std::unordered_set<int> ids;
    for (const SceneNode& node : loaded) {
        if (!ids.insert(node.id).second) return false;
        if (!std::isfinite(node.rotation) || !std::isfinite(node.scale_x) || !std::isfinite(node.scale_y) ||
            !std::isfinite(node.velocity_x) || !std::isfinite(node.velocity_y) || !std::isfinite(node.gravity) ||
            !std::isfinite(node.camera_zoom) || !std::isfinite(node.audio_volume) || !std::isfinite(node.opacity) ||
            node.opacity < 0.0f || node.opacity > 1.0f || node.camera_zoom <= 0.0f) return false;
    }
    const auto root = std::find_if(loaded.begin(), loaded.end(), [](const SceneNode& node) { return node.id == 1 && node.parent_id == 0; });
    if (root == loaded.end()) return false;
    for (const SceneNode& node : loaded) {
        if (node.id == 1) continue;
        if (node.parent_id == 0 || ids.find(node.parent_id) == ids.end()) return false;
        int cursor = node.parent_id;
        std::unordered_set<int> ancestors;
        while (cursor != 0) {
            if (!ancestors.insert(cursor).second) return false;
            if (cursor == node.id) return false;
            const auto parent = std::find_if(loaded.begin(), loaded.end(), [&](const SceneNode& candidate) { return candidate.id == cursor; });
            if (parent == loaded.end()) return false;
            cursor = parent->parent_id;
        }
    }
    std::unordered_set<std::wstring> names;
    for (const SceneNode& node : loaded) if (!names.insert(node.name).second) return false;

    nodes_ = std::move(loaded);
    next_id_ = maximum_id + 1;
    selected_id_ = 1;
    path_ = path;
    dirty_ = false;
    return true;
}

int Scene::selected_id() const noexcept { return selected_id_; }
void Scene::set_selected_id(int id) noexcept { selected_id_ = id; }
const std::wstring& Scene::path() const noexcept { return path_; }
bool Scene::dirty() const noexcept { return dirty_; }
void Scene::set_dirty(bool value) noexcept { dirty_ = value; }

SceneSnapshot Scene::capture() const {
    return SceneSnapshot{nodes_, next_id_, selected_id_};
}

void Scene::restore(const SceneSnapshot& snapshot) {
    nodes_ = snapshot.nodes;
    next_id_ = snapshot.next_id;
    selected_id_ = find(snapshot.selected_id) ? snapshot.selected_id : 1;
    dirty_ = true;
}

int Scene::depth_of(int id) const {
    int depth = 0;
    const SceneNode* node = find(id);
    while (node && node->parent_id != 0 && depth < 64) {
        node = find(node->parent_id);
        ++depth;
    }
    return depth;
}

bool Scene::is_descendant(int id, int ancestor_id) const {
    const SceneNode* node = find(id);
    int guard = 0;
    while (node && node->parent_id != 0 && guard++ < 64) {
        if (node->parent_id == ancestor_id) return true;
        node = find(node->parent_id);
    }
    return false;
}

}
