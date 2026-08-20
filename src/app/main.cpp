#include "platform/win32_app.hpp"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int show_command) {
    Win32App app;
    return app.run(instance, show_command);
}
