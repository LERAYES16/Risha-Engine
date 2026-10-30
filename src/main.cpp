#include "app/App.hpp"
#include <cstring>

int main(int argc, char** argv) {
    risha::app::App app;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--demo") == 0) app.autostart(false);
        else if (std::strcmp(argv[i], "--demo-play") == 0) app.autostart(true);
    }
    app.run();
    return 0;
}
