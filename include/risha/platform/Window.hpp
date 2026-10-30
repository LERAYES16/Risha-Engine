#pragma once
#include "risha/gfx/Canvas.hpp"
#include <memory>
#include <string>
#include <vector>

namespace risha::platform {

enum class Key {
    Unknown,
    Left, Right, Up, Down, Home, End, PageUp, PageDown, Delete, Backspace, Enter, Tab, Escape, Space,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
    Other
};

enum class EventType { Close, Resize, Redraw, KeyDown, KeyUp, Text, MouseMove, MouseDown, MouseUp, MouseWheel, GotFocus, LostFocus, Leave };

struct Event {
    EventType type{EventType::Redraw};
    Key key{Key::Unknown};
    std::wstring text;
    bool ctrl{false};
    bool shift{false};
    bool alt{false};
    bool repeat{false};
    int x{0};
    int y{0};
    int button{0};
    int wheel{0};
    int width{0};
    int height{0};
};

class NativeWindow {
public:
    NativeWindow();
    ~NativeWindow();
    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;

    bool create(const std::wstring& title, int width, int height, int min_width, int min_height);
    void close();
    bool valid() const;
    void set_title(const std::wstring& title);
    void raise();
    void present(const gfx::Canvas& canvas);
    std::vector<Event> poll();
    bool has_pending() const;
    int width() const;
    int height() const;
    int connection() const;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

void wait_for_events(int timeout_ms, const std::vector<NativeWindow*>& windows);
std::wstring open_folder_dialog(const std::wstring& title);
bool open_url(const std::string& url);
std::wstring home_directory();
std::wstring data_directory();

}
