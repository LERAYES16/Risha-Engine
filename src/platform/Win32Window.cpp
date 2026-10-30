#include "risha/platform/Window.hpp"
#include "risha/core/Logger.hpp"
#include "risha/core/Text.hpp"
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <deque>

namespace risha::platform {
namespace {

const wchar_t* kWindowClass = L"RishaEngineWindow";
int g_window_count = 0;

Key key_from_virtual(WPARAM code) {
    switch (code) {
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_HOME: return Key::Home;
        case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp;
        case VK_NEXT: return Key::PageDown;
        case VK_DELETE: return Key::Delete;
        case VK_BACK: return Key::Backspace;
        case VK_RETURN: return Key::Enter;
        case VK_TAB: return Key::Tab;
        case VK_ESCAPE: return Key::Escape;
        case VK_SPACE: return Key::Space;
        default: break;
    }
    if (code >= VK_F1 && code <= VK_F12) return static_cast<Key>(static_cast<int>(Key::F1) + static_cast<int>(code - VK_F1));
    if (code >= 'A' && code <= 'Z') return static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(code - 'A'));
    if (code >= '0' && code <= '9') return static_cast<Key>(static_cast<int>(Key::D0) + static_cast<int>(code - '0'));
    if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9) return static_cast<Key>(static_cast<int>(Key::D0) + static_cast<int>(code - VK_NUMPAD0));
    return Key::Other;
}


}

struct NativeWindow::Impl {
    HWND window{nullptr};
    int width{0};
    int height{0};
    int min_width{0};
    int min_height{0};
    std::deque<Event> events;
    std::vector<uint32_t> last;
    int last_width{0};
    int last_height{0};
    bool tracking{false};

    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    void paint_last(HDC dc) {
        if (last.empty() || last_width <= 0 || last_height <= 0) return;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = last_width;
        info.bmiHeader.biHeight = -last_height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        SetDIBitsToDevice(dc, 0, 0, static_cast<DWORD>(last_width), static_cast<DWORD>(last_height), 0, 0, 0, static_cast<UINT>(last_height), last.data(), &info, DIB_RGB_COLORS);
    }
};

LRESULT CALLBACK NativeWindow::Impl::procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    Impl* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
    if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);
    auto modifiers = [](Event& event) {
        event.ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        event.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        event.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    };
    switch (message) {
        case WM_CLOSE: {
            Event event;
            event.type = EventType::Close;
            self->events.push_back(event);
            return 0;
        }
        case WM_SIZE: {
            if (wparam == SIZE_MINIMIZED) return 0;
            Event event;
            event.type = EventType::Resize;
            self->width = LOWORD(lparam);
            self->height = HIWORD(lparam);
            event.width = self->width;
            event.height = self->height;
            self->events.push_back(event);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lparam);
            RECT rect{0, 0, self->min_width, self->min_height};
            AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
            info->ptMinTrackSize.x = rect.right - rect.left;
            info->ptMinTrackSize.y = rect.bottom - rect.top;
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            self->paint_last(dc);
            EndPaint(hwnd, &paint);
            Event event;
            event.type = EventType::Redraw;
            self->events.push_back(event);
            return 0;
        }
        case WM_SETFOCUS: {
            Event event;
            event.type = EventType::GotFocus;
            self->events.push_back(event);
            return 0;
        }
        case WM_KILLFOCUS: {
            Event event;
            event.type = EventType::LostFocus;
            self->events.push_back(event);
            return 0;
        }
        case WM_MOUSELEAVE: {
            self->tracking = false;
            Event event;
            event.type = EventType::Leave;
            self->events.push_back(event);
            return 0;
        }
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP: {
            Event event;
            event.type = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) ? EventType::KeyDown : EventType::KeyUp;
            event.key = key_from_virtual(wparam);
            event.repeat = (lparam & (1 << 30)) != 0 && event.type == EventType::KeyDown;
            modifiers(event);
            self->events.push_back(event);
            if (message == WM_SYSKEYDOWN && wparam != VK_F10) return 0;
            break;
        }
        case WM_CHAR: {
            Event event;
            event.type = EventType::Text;
            modifiers(event);
            const wchar_t c = static_cast<wchar_t>(wparam);
            if (c >= 0x20 && c != 0x7F && !event.ctrl && !event.alt) {
                event.text.push_back(c);
                self->events.push_back(event);
            } else if (c >= 0x20 && c != 0x7F && event.ctrl && event.alt) {
                event.ctrl = false;
                event.alt = false;
                event.text.push_back(c);
                self->events.push_back(event);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!self->tracking) {
                TRACKMOUSEEVENT track{};
                track.cbSize = sizeof(track);
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = hwnd;
                TrackMouseEvent(&track);
                self->tracking = true;
            }
            Event event;
            event.type = EventType::MouseMove;
            event.x = GET_X_LPARAM(lparam);
            event.y = GET_Y_LPARAM(lparam);
            modifiers(event);
            self->events.push_back(event);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            Event event;
            const bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN;
            event.type = down ? EventType::MouseDown : EventType::MouseUp;
            event.button = (message == WM_LBUTTONDOWN || message == WM_LBUTTONUP) ? 1 : ((message == WM_RBUTTONDOWN || message == WM_RBUTTONUP) ? 2 : 3);
            event.x = GET_X_LPARAM(lparam);
            event.y = GET_Y_LPARAM(lparam);
            modifiers(event);
            if (down) SetCapture(hwnd);
            else ReleaseCapture();
            self->events.push_back(event);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            Event event;
            event.type = EventType::MouseWheel;
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &point);
            event.x = point.x;
            event.y = point.y;
            event.wheel = GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1 : -1;
            modifiers(event);
            self->events.push_back(event);
            return 0;
        }
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

NativeWindow::NativeWindow() : impl_(new Impl) {}
NativeWindow::~NativeWindow() { close(); }

bool NativeWindow::valid() const { return impl_ && impl_->window != nullptr; }
int NativeWindow::width() const { return impl_->width; }
int NativeWindow::height() const { return impl_->height; }
int NativeWindow::connection() const { return -1; }
bool NativeWindow::has_pending() const { return !impl_->events.empty(); }

bool NativeWindow::create(const std::wstring& title, int width, int height, int min_width, int min_height) {
    close();
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (g_window_count == 0) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = Impl::procedure;
        window_class.hInstance = instance;
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.lpszClassName = kWindowClass;
        if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            core::log_error("تعذر تسجيل صنف النافذة.");
            return false;
        }
    }
    Impl& s = *impl_;
    s.min_width = min_width;
    s.min_height = min_height;
    RECT rect{0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    s.window = CreateWindowExW(0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                               nullptr, nullptr, instance, &s);
    if (!s.window) {
        core::log_error("تعذر إنشاء النافذة.");
        return false;
    }
    ++g_window_count;
    RECT client{};
    GetClientRect(s.window, &client);
    s.width = client.right - client.left;
    s.height = client.bottom - client.top;
    ShowWindow(s.window, SW_SHOW);
    UpdateWindow(s.window);
    return true;
}

void NativeWindow::close() {
    Impl& s = *impl_;
    if (!s.window) return;
    SetWindowLongPtrW(s.window, GWLP_USERDATA, 0);
    DestroyWindow(s.window);
    s.window = nullptr;
    s.events.clear();
    --g_window_count;
}

void NativeWindow::set_title(const std::wstring& title) {
    if (impl_->window) SetWindowTextW(impl_->window, title.c_str());
}

void NativeWindow::raise() {
    if (!impl_->window) return;
    SetForegroundWindow(impl_->window);
    SetFocus(impl_->window);
}

void NativeWindow::present(const gfx::Canvas& canvas) {
    Impl& s = *impl_;
    if (!s.window) return;
    s.last.assign(canvas.data(), canvas.data() + static_cast<size_t>(canvas.width()) * static_cast<size_t>(canvas.height()));
    s.last_width = canvas.width();
    s.last_height = canvas.height();
    HDC dc = GetDC(s.window);
    s.paint_last(dc);
    ReleaseDC(s.window, dc);
}

std::vector<Event> NativeWindow::poll() {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    std::vector<Event> result(impl_->events.begin(), impl_->events.end());
    impl_->events.clear();
    return result;
}

void wait_for_events(int timeout_ms, const std::vector<NativeWindow*>& windows) {
    for (NativeWindow* window : windows) if (window && window->has_pending()) return;
    MsgWaitForMultipleObjects(0, nullptr, FALSE, static_cast<DWORD>(std::max(0, timeout_ms)), QS_ALLINPUT);
}

std::wstring home_directory() {
    wchar_t buffer[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, buffer))) return buffer;
    return L".";
}

std::wstring data_directory() {
    wchar_t buffer[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buffer))) return std::wstring(buffer) + L"\\RishaEngine";
    return L".\\data";
}

bool open_url(const std::string& url) {
    const bool ok = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", core::from_utf8(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    return ok;
}

std::wstring open_folder_dialog(const std::wstring& title) {
    BROWSEINFOW info{};
    info.lpszTitle = title.c_str();
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&info);
    if (!item) return {};
    wchar_t buffer[MAX_PATH]{};
    std::wstring result;
    if (SHGetPathFromIDListW(item, buffer)) result = buffer;
    CoTaskMemFree(item);
    return result;
}

}
