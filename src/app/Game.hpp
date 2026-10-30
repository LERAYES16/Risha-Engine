#pragma once
#include "risha/core/Project.hpp"
#include "risha/gfx/Canvas.hpp"
#include "risha/platform/Window.hpp"
#include "risha/runtime/Runtime.hpp"
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

namespace risha::app {

struct OutputLine {
    std::wstring text;
    bool error{false};
};

std::wstring key_name(platform::Key key);

class Game {
public:
    bool start(const editor::Scene& scene, const core::ProjectSettings& settings, const std::filesystem::path& directory, std::vector<OutputLine>& sink);
    void stop();
    bool open() const { return window_.valid(); }
    platform::NativeWindow& window() { return window_; }
    void handle(const std::vector<platform::Event>& events);
    void tick(double dt, std::vector<OutputLine>& sink);
    void render();

private:
    void release_all();
    platform::NativeWindow window_;
    runtime::Runtime runtime_;
    core::ProjectSettings settings_;
    gfx::Canvas canvas_;
    std::deque<OutputLine> hud_;
    double accumulator_{0.0};
    double clock_{0.0};
    bool dirty_{true};
};

}
