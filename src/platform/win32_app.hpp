#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "game/game.hpp"

#include <array>

enum class KeyboardMessageRouting {
    Consume,
    DeferToDefault,
};

KeyboardMessageRouting keyboard_message_routing(UINT message) noexcept;
unsigned int next_simulation_wait_milliseconds(float accumulator, float fixed_step) noexcept;

class Win32App {
public:
    int run(HINSTANCE instance, int show_command);

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);

    void on_key_down(WPARAM virtual_key, LPARAM l_param);
    void on_key_up(WPARAM virtual_key);
    InputState read_input(const Game& game);
    void render(const Game& game) const;

    HINSTANCE instance_{nullptr};
    HWND window_{nullptr};
    bool running_{false};
    std::array<bool, 256> key_down_{};
    std::array<bool, 256> key_pressed_{};
};
