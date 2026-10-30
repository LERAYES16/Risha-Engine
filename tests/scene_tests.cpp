#include "risha/editor/Scene.hpp"
#include <cstdio>
#include <cstring>
#include <iostream>

int main() {
    risha::editor::Scene scene;
    const int auto_named_one = scene.add_node(1, L"", L"Sprite");
    const int auto_named_two = scene.add_node(1, L"", L"Sprite");
    if (scene.find(auto_named_one)->name != L"Sprite") return 27;
    if (scene.find(auto_named_two)->name != L"Sprite 2") return 28;
    const int parent = scene.add_node(1, L"لاعب|رئيسي", L"كائن|2D");
    const int child = scene.add_node(parent, L"ابن", L"كائن");
    auto* parent_node = scene.find(parent);
    if (!parent_node) return 1;
    parent_node->x = 55;
    parent_node->y = 75;
    parent_node->width = 200;
    parent_node->height = 100;
    parent_node->visible = false;
    parent_node->locked = true;
    parent_node->opacity = 0.65f;
    parent_node->rotation = 25.0f;
    parent_node->scale_x = 1.5f;
    parent_node->scale_y = 0.75f;
    parent_node->z = 7;
    parent_node->color = 0xFF112233u;
    parent_node->flip_x = true;
    parent_node->resource = L"assets/لاعب.png";
    parent_node->script = L"scripts/لاعب.rishascript";
    parent_node->velocity_x = 12.5f;
    parent_node->velocity_y = -3.5f;
    parent_node->gravity = 9.8f;
    parent_node->camera_zoom = 1.25f;
    parent_node->active_camera = true;
    parent_node->collision_layer = 2;
    parent_node->collision_mask = 3;
    parent_node->audio_loop = true;
    parent_node->audio_volume = 0.5f;
    const std::wstring path = L"risha_scene_test.risha";
    if (!scene.save(path)) return 2;

    risha::editor::Scene loaded;
    if (!loaded.load(path)) return 3;
    const auto* loaded_parent = loaded.find(parent);
    const auto* loaded_child = loaded.find(child);
    if (!loaded_parent || !loaded_child) return 4;
    if (loaded_parent->name != L"لاعب|رئيسي") return 5;
    if (loaded_parent->type != L"كائن|2D") return 6;
    if (loaded_parent->x != 55 || loaded_parent->y != 75) return 7;
    if (loaded_parent->width != 200 || loaded_parent->height != 100) return 8;
    if (loaded_parent->visible || !loaded_parent->locked || loaded_parent->opacity != 0.65f) return 9;
    if (loaded_parent->rotation != 25.0f || loaded_parent->scale_x != 1.5f || loaded_parent->scale_y != 0.75f) return 13;
    if (loaded_parent->z != 7 || loaded_parent->color != 0xFF112233u || !loaded_parent->flip_x) return 14;
    if (loaded_parent->resource != L"assets/لاعب.png" || loaded_parent->script != L"scripts/لاعب.rishascript") return 15;
    if (loaded_parent->velocity_x != 12.5f || loaded_parent->velocity_y != -3.5f || loaded_parent->gravity != 9.8f) return 16;
    if (loaded_parent->camera_zoom != 1.25f || !loaded_parent->active_camera || loaded_parent->collision_layer != 2 || loaded_parent->collision_mask != 3) return 17;
    if (!loaded_parent->audio_loop || loaded_parent->audio_volume != 0.5f) return 18;
    if (!loaded.rename_node(child, L"طفل جديد")) return 19;
    if (loaded.find(child)->name != L"طفل جديد") return 20;
    if (loaded.duplicate_node(child) == 0) return 21;
    const int duplicate = loaded.selected_id();
    if (!loaded.reparent_node(duplicate, parent)) return 22;
    if (loaded.find(duplicate)->parent_id != parent) return 23;
    if (loaded.find(child)->parent_id != parent) return 10;
    if (!loaded.remove_node(parent)) return 11;
    if (loaded.find(parent) || loaded.find(child)) return 12;
    std::remove("risha_scene_test.risha");
    {
        FILE* legacy = std::fopen("risha_scene_legacy.risha", "wb");
        if (!legacy) return 24;
        const char* data = "RISHA_SCENE_0_3\n1|0|d8;a7;d9;84;d8;b9;d8;a7;d9;84;d9;85;|d8;b9;d8;a7;d9;84;d9;85;|0|0|640|420|1\n";
        std::fwrite(data, 1, std::strlen(data), legacy);
        std::fclose(legacy);
    }
    risha::editor::Scene legacy_scene;
    if (!legacy_scene.load(L"risha_scene_legacy.risha")) return 25;
    if (legacy_scene.find(1)->name != L"العالم") return 26;
    std::remove("risha_scene_legacy.risha");
    std::cout << "scene tests passed\n";
    return 0;
}
