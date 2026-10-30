#include "risha/platform/Window.hpp"
#include "risha/core/Logger.hpp"
#include "risha/core/Paths.hpp"
#include "risha/core/Text.hpp"
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace risha::platform {
namespace {

char32_t keysym_to_unicode(KeySym keysym) {
    if (keysym >= 0x20 && keysym <= 0x7E) return static_cast<char32_t>(keysym);
    if (keysym >= 0xA0 && keysym <= 0xFF) return static_cast<char32_t>(keysym);
    if ((keysym & 0xFF000000) == 0x01000000) return static_cast<char32_t>(keysym & 0x00FFFFFF);
    if (keysym == 0xFF80) return U' ';
    if (keysym >= 0xFFAA && keysym <= 0xFFB9) {
        static const char32_t pad[] = {U'*', U'+', U',', U'-', U'.', U'/', U'0', U'1', U'2', U'3', U'4', U'5', U'6', U'7', U'8', U'9'};
        return pad[keysym - 0xFFAA];
    }
    if (keysym >= 0x5C1 && keysym <= 0x5EA) return static_cast<char32_t>(0x0621 + (keysym - 0x5C1));
    if (keysym == 0x5E0) return 0x0640;
    if (keysym >= 0x5EB && keysym <= 0x5F2) return static_cast<char32_t>(0x064B + (keysym - 0x5EB));
    if (keysym == 0x5AC) return 0x060C;
    if (keysym == 0x5BB) return 0x061B;
    if (keysym == 0x5BF) return 0x061F;
    return 0;
}

Key key_from_keysym(KeySym keysym) {
    switch (keysym) {
        case XK_Left: return Key::Left;
        case XK_Right: return Key::Right;
        case XK_Up: return Key::Up;
        case XK_Down: return Key::Down;
        case XK_Home: return Key::Home;
        case XK_End: return Key::End;
        case XK_Prior: return Key::PageUp;
        case XK_Next: return Key::PageDown;
        case XK_Delete: return Key::Delete;
        case XK_BackSpace: return Key::Backspace;
        case XK_Return: case XK_KP_Enter: return Key::Enter;
        case XK_Tab: case XK_ISO_Left_Tab: return Key::Tab;
        case XK_Escape: return Key::Escape;
        case XK_space: return Key::Space;
        default: break;
    }
    if (keysym >= XK_F1 && keysym <= XK_F12) return static_cast<Key>(static_cast<int>(Key::F1) + static_cast<int>(keysym - XK_F1));
    if (keysym >= XK_a && keysym <= XK_z) return static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(keysym - XK_a));
    if (keysym >= XK_A && keysym <= XK_Z) return static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(keysym - XK_A));
    if (keysym >= XK_0 && keysym <= XK_9) return static_cast<Key>(static_cast<int>(Key::D0) + static_cast<int>(keysym - XK_0));
    if (keysym >= XK_KP_0 && keysym <= XK_KP_9) return static_cast<Key>(static_cast<int>(Key::D0) + static_cast<int>(keysym - XK_KP_0));
    return Key::Other;
}

}

struct NativeWindow::Impl {
    Display* display{nullptr};
    ::Window window{0};
    Atom delete_atom{0};
    XIM input_method{nullptr};
    XIC input_context{nullptr};
    int screen{0};
    int width{0};
    int height{0};
    bool standard_format{true};
    unsigned long red_mask{0}, green_mask{0}, blue_mask{0};
    int red_shift{0}, green_shift{0}, blue_shift{0};
    GC gc{nullptr};
    std::vector<unsigned char> convert;

    static int shift_of(unsigned long mask) {
        int shift = 0;
        if (!mask) return 0;
        while (!((mask >> shift) & 1)) ++shift;
        return shift;
    }

    Key physical_key(const XKeyEvent& event) const {
        for (int group = 0; group < 4; ++group) {
            const KeySym keysym = XkbKeycodeToKeysym(display, static_cast<KeyCode>(event.keycode), group, 0);
            const Key key = key_from_keysym(keysym);
            if (key != Key::Other && key != Key::Unknown) return key;
        }
        return key_from_keysym(XkbKeycodeToKeysym(display, static_cast<KeyCode>(event.keycode), 0, 0));
    }

    std::wstring text_of(XKeyEvent& event) {
        std::u32string codes;
        KeySym keysym = NoSymbol;
        char buffer[64]{};
        if (input_context) {
            Status status = 0;
            const int length = Xutf8LookupString(input_context, &event, buffer, static_cast<int>(sizeof(buffer) - 1), &keysym, &status);
            if ((status == XLookupChars || status == XLookupBoth) && length > 0) {
                const std::wstring decoded = core::from_utf8(std::string_view(buffer, static_cast<size_t>(length)));
                return decoded;
            }
            if (status == XLookupKeySym || status == XLookupBoth) {
                const char32_t code = keysym_to_unicode(keysym);
                if (code) codes.push_back(code);
                return core::from_u32(codes);
            }
            return {};
        }
        XLookupString(&event, buffer, static_cast<int>(sizeof(buffer) - 1), &keysym, nullptr);
        const char32_t code = keysym_to_unicode(keysym);
        if (code) codes.push_back(code);
        return core::from_u32(codes);
    }
};

NativeWindow::NativeWindow() : impl_(new Impl) {}

NativeWindow::~NativeWindow() { close(); }

bool NativeWindow::valid() const { return impl_ && impl_->display && impl_->window; }
int NativeWindow::width() const { return impl_->width; }
int NativeWindow::height() const { return impl_->height; }
int NativeWindow::connection() const { return impl_->display ? ConnectionNumber(impl_->display) : -1; }

bool NativeWindow::create(const std::wstring& title, int width, int height, int min_width, int min_height) {
    close();
    std::setlocale(LC_ALL, "");
    XSetLocaleModifiers("");
    Impl& s = *impl_;
    s.display = XOpenDisplay(nullptr);
    if (!s.display) {
        core::log_error("تعذر الاتصال بخادم العرض X11؛ تأكد من تشغيل بيئة سطح مكتب رسومية.");
        return false;
    }
    s.screen = DefaultScreen(s.display);
    Visual* visual = DefaultVisual(s.display, s.screen);
    s.red_mask = visual->red_mask;
    s.green_mask = visual->green_mask;
    s.blue_mask = visual->blue_mask;
    s.red_shift = Impl::shift_of(s.red_mask);
    s.green_shift = Impl::shift_of(s.green_mask);
    s.blue_shift = Impl::shift_of(s.blue_mask);
    s.standard_format = s.red_mask == 0xFF0000 && s.green_mask == 0x00FF00 && s.blue_mask == 0x0000FF && ImageByteOrder(s.display) == LSBFirst;
    s.width = width;
    s.height = height;
    s.window = XCreateSimpleWindow(s.display, RootWindow(s.display, s.screen), 60, 60, static_cast<unsigned>(width), static_cast<unsigned>(height), 0,
                                   BlackPixel(s.display, s.screen), BlackPixel(s.display, s.screen));
    if (!s.window) {
        core::log_error("تعذر إنشاء النافذة.");
        XCloseDisplay(s.display);
        s.display = nullptr;
        return false;
    }
    XSelectInput(s.display, s.window, ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                                           StructureNotifyMask | FocusChangeMask | LeaveWindowMask);
    s.delete_atom = XInternAtom(s.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(s.display, s.window, &s.delete_atom, 1);
    XSizeHints hints{};
    hints.flags = PMinSize;
    hints.min_width = min_width;
    hints.min_height = min_height;
    XSetWMNormalHints(s.display, s.window, &hints);
    XClassHint class_hint{};
    class_hint.res_name = const_cast<char*>("risha_editor");
    class_hint.res_class = const_cast<char*>("RishaEngine");
    XSetClassHint(s.display, s.window, &class_hint);
    set_title(title);
    int supported = 0;
    XkbSetDetectableAutoRepeat(s.display, True, &supported);
    s.input_method = XOpenIM(s.display, nullptr, nullptr, nullptr);
    if (s.input_method) {
        s.input_context = XCreateIC(s.input_method, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, s.window, XNFocusWindow, s.window, nullptr);
        if (s.input_context) XSetICFocus(s.input_context);
    }
    s.gc = XCreateGC(s.display, s.window, 0, nullptr);
    XMapWindow(s.display, s.window);
    XFlush(s.display);
    return true;
}

void NativeWindow::close() {
    Impl& s = *impl_;
    if (!s.display) return;
    if (s.input_context) XDestroyIC(s.input_context);
    if (s.input_method) XCloseIM(s.input_method);
    if (s.gc) XFreeGC(s.display, s.gc);
    if (s.window) XDestroyWindow(s.display, s.window);
    XCloseDisplay(s.display);
    s.display = nullptr;
    s.window = 0;
    s.gc = nullptr;
    s.input_context = nullptr;
    s.input_method = nullptr;
}

void NativeWindow::set_title(const std::wstring& title) {
    Impl& s = *impl_;
    if (!s.display) return;
    const std::string utf8 = core::to_utf8(title);
    Xutf8SetWMProperties(s.display, s.window, utf8.c_str(), utf8.c_str(), nullptr, 0, nullptr, nullptr, nullptr);
    const Atom name = XInternAtom(s.display, "_NET_WM_NAME", False);
    const Atom type = XInternAtom(s.display, "UTF8_STRING", False);
    XChangeProperty(s.display, s.window, name, type, 8, PropModeReplace, reinterpret_cast<const unsigned char*>(utf8.c_str()), static_cast<int>(utf8.size()));
}

void NativeWindow::raise() {
    Impl& s = *impl_;
    if (!s.display) return;
    XRaiseWindow(s.display, s.window);
    XSetInputFocus(s.display, s.window, RevertToParent, CurrentTime);
    XFlush(s.display);
}

bool NativeWindow::has_pending() const {
    return impl_->display && XPending(impl_->display) > 0;
}

void NativeWindow::present(const gfx::Canvas& canvas) {
    Impl& s = *impl_;
    if (!s.display || !s.window) return;
    const int w = canvas.width();
    const int h = canvas.height();
    XImage image{};
    image.width = w;
    image.height = h;
    image.format = ZPixmap;
    image.xoffset = 0;
    image.byte_order = ImageByteOrder(s.display);
    image.bitmap_unit = BitmapUnit(s.display);
    image.bitmap_bit_order = BitmapBitOrder(s.display);
    image.bitmap_pad = 32;
    image.depth = DefaultDepth(s.display, s.screen);
    image.bytes_per_line = w * 4;
    image.bits_per_pixel = 32;
    image.red_mask = s.red_mask;
    image.green_mask = s.green_mask;
    image.blue_mask = s.blue_mask;
    if (s.standard_format) {
        image.data = const_cast<char*>(reinterpret_cast<const char*>(canvas.data()));
    } else {
        s.convert.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        uint32_t* out = reinterpret_cast<uint32_t*>(s.convert.data());
        const uint32_t* in = canvas.data();
        const bool swap = image.byte_order == MSBFirst;
        for (size_t i = 0, count = static_cast<size_t>(w) * static_cast<size_t>(h); i < count; ++i) {
            const uint32_t p = in[i];
            uint32_t v = (((p >> 16) & 255) << s.red_shift) | (((p >> 8) & 255) << s.green_shift) | ((p & 255) << s.blue_shift);
            if (swap) v = ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v >> 8) & 0xFF00) | (v >> 24);
            out[i] = v;
        }
        image.data = reinterpret_cast<char*>(s.convert.data());
    }
    if (!XInitImage(&image)) return;
    XPutImage(s.display, s.window, s.gc, &image, 0, 0, 0, 0, static_cast<unsigned>(w), static_cast<unsigned>(h));
    XFlush(s.display);
}

std::vector<Event> NativeWindow::poll() {
    std::vector<Event> events;
    Impl& s = *impl_;
    if (!s.display) return events;
    while (XPending(s.display) > 0) {
        XEvent raw{};
        XNextEvent(s.display, &raw);
        if (XFilterEvent(&raw, None)) continue;
        Event event;
        auto modifiers = [&](unsigned state) {
            event.ctrl = (state & ControlMask) != 0;
            event.shift = (state & ShiftMask) != 0;
            event.alt = (state & Mod1Mask) != 0;
        };
        switch (raw.type) {
            case ClientMessage:
                if (static_cast<Atom>(raw.xclient.data.l[0]) == s.delete_atom) {
                    event.type = EventType::Close;
                    events.push_back(event);
                }
                break;
            case ConfigureNotify:
                if (raw.xconfigure.width != s.width || raw.xconfigure.height != s.height) {
                    s.width = raw.xconfigure.width;
                    s.height = raw.xconfigure.height;
                    event.type = EventType::Resize;
                    event.width = s.width;
                    event.height = s.height;
                    events.push_back(event);
                }
                break;
            case Expose:
                if (raw.xexpose.count == 0) {
                    event.type = EventType::Redraw;
                    events.push_back(event);
                }
                break;
            case FocusIn:
                if (s.input_context) XSetICFocus(s.input_context);
                event.type = EventType::GotFocus;
                events.push_back(event);
                break;
            case FocusOut:
                if (s.input_context) XUnsetICFocus(s.input_context);
                event.type = EventType::LostFocus;
                events.push_back(event);
                break;
            case LeaveNotify:
                event.type = EventType::Leave;
                events.push_back(event);
                break;
            case KeyPress:
            case KeyRelease: {
                modifiers(raw.xkey.state);
                event.key = s.physical_key(raw.xkey);
                event.type = raw.type == KeyPress ? EventType::KeyDown : EventType::KeyUp;
                events.push_back(event);
                if (raw.type == KeyPress && !event.ctrl && !event.alt) {
                    std::wstring text = s.text_of(raw.xkey);
                    std::wstring printable;
                    for (wchar_t c : text) if (c >= 0x20 && c != 0x7F) printable.push_back(c);
                    if (!printable.empty()) {
                        Event text_event = event;
                        text_event.type = EventType::Text;
                        text_event.text = std::move(printable);
                        events.push_back(std::move(text_event));
                    }
                }
                break;
            }
            case ButtonPress:
            case ButtonRelease: {
                modifiers(raw.xbutton.state);
                event.x = raw.xbutton.x;
                event.y = raw.xbutton.y;
                const unsigned button = raw.xbutton.button;
                if (button == Button4 || button == Button5) {
                    if (raw.type == ButtonPress) {
                        event.type = EventType::MouseWheel;
                        event.wheel = button == Button4 ? 1 : -1;
                        events.push_back(event);
                    }
                } else if (button == Button1 || button == Button2 || button == Button3) {
                    event.type = raw.type == ButtonPress ? EventType::MouseDown : EventType::MouseUp;
                    event.button = button == Button1 ? 1 : (button == Button3 ? 2 : 3);
                    events.push_back(event);
                }
                break;
            }
            case MotionNotify:
                modifiers(raw.xmotion.state);
                event.type = EventType::MouseMove;
                event.x = raw.xmotion.x;
                event.y = raw.xmotion.y;
                events.push_back(event);
                break;
            default:
                break;
        }
    }
    return events;
}

void wait_for_events(int timeout_ms, const std::vector<NativeWindow*>& windows) {
    std::vector<pollfd> fds;
    for (NativeWindow* window : windows) {
        if (!window || !window->valid()) continue;
        if (window->has_pending()) return;
        pollfd entry{};
        entry.fd = window->connection();
        entry.events = POLLIN;
        fds.push_back(entry);
    }
    if (fds.empty()) {
        usleep(static_cast<useconds_t>(std::max(0, timeout_ms)) * 1000);
        return;
    }
    ::poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout_ms);
}

std::wstring home_directory() {
    const char* home = std::getenv("HOME");
    return core::from_utf8(home ? home : ".");
}

std::wstring data_directory() {
    const char* data = std::getenv("XDG_DATA_HOME");
    if (data && *data) return core::from_utf8(std::string(data) + "/risha-engine");
    return home_directory() + L"/.local/share/risha-engine";
}

bool open_url(const std::string& url) {
    const pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        setsid();
        const pid_t inner = fork();
        if (inner != 0) _exit(0);
        execlp("xdg-open", "xdg-open", url.c_str(), static_cast<char*>(nullptr));
        _exit(1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
}

std::wstring open_folder_dialog(const std::wstring& title) {
    int pipes[2];
    if (pipe(pipes) != 0) return {};
    const pid_t pid = fork();
    if (pid < 0) { close(pipes[0]); close(pipes[1]); return {}; }
    if (pid == 0) {
        close(pipes[0]);
        dup2(pipes[1], STDOUT_FILENO);
        close(pipes[1]);
        const int null_fd = ::open("/dev/null", O_WRONLY);
        if (null_fd >= 0) dup2(null_fd, STDERR_FILENO);
        const std::string label = "--title=" + core::to_utf8(title);
        execlp("zenity", "zenity", "--file-selection", "--directory", label.c_str(), static_cast<char*>(nullptr));
        execlp("kdialog", "kdialog", "--getexistingdirectory", ".", static_cast<char*>(nullptr));
        _exit(127);
    }
    close(pipes[1]);
    std::string result;
    char buffer[512];
    ssize_t count = 0;
    while ((count = read(pipes[0], buffer, sizeof(buffer))) > 0) result.append(buffer, static_cast<size_t>(count));
    close(pipes[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) return {};
    return core::from_utf8(result);
}

}
