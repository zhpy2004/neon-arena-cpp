#include "platform/win32_app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>

namespace {

constexpr wchar_t kWindowClassName[] = L"NeonArenaWindow";
constexpr wchar_t kWindowTitle[] = L"Neon Arena";
constexpr float kFixedStep = 1.0f / 60.0f;
constexpr float kMaximumFrameDelta = 0.25f;
constexpr int kPadding = 34;

COLORREF color(unsigned char red, unsigned char green, unsigned char blue) noexcept {
    return RGB(red, green, blue);
}

int rounded(float value) noexcept {
    return static_cast<int>(std::lround(value));
}

void draw_text(HDC device_context, int x, int y, int height, int weight, COLORREF text_color,
               const wchar_t* text) {
    const HFONT font = CreateFontW(
        height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    if (font == nullptr) {
        return;
    }

    const HGDIOBJ old_font = SelectObject(device_context, font);
    const int old_background_mode = SetBkMode(device_context, TRANSPARENT);
    const COLORREF old_text_color = SetTextColor(device_context, text_color);
    TextOutW(device_context, x, y, text, static_cast<int>(std::wcslen(text)));
    SetTextColor(device_context, old_text_color);
    SetBkMode(device_context, old_background_mode);
    SelectObject(device_context, old_font);
    DeleteObject(font);
}

void fill_rect(HDC device_context, const RECT& rectangle, COLORREF fill_color) {
    const HBRUSH brush = CreateSolidBrush(fill_color);
    if (brush != nullptr) {
        FillRect(device_context, &rectangle, brush);
        DeleteObject(brush);
    }
}

void draw_outline(HDC device_context, const RECT& rectangle, int width, COLORREF outline_color) {
    const HPEN pen = CreatePen(PS_SOLID, width, outline_color);
    const HGDIOBJ old_pen = SelectObject(device_context, pen);
    const HGDIOBJ old_brush = SelectObject(device_context, GetStockObject(NULL_BRUSH));
    Rectangle(device_context, rectangle.left, rectangle.top, rectangle.right, rectangle.bottom);
    SelectObject(device_context, old_brush);
    SelectObject(device_context, old_pen);
    DeleteObject(pen);
}

void draw_circle(HDC device_context, int center_x, int center_y, int radius, COLORREF fill_color) {
    const HBRUSH brush = CreateSolidBrush(fill_color);
    const HGDIOBJ old_brush = SelectObject(device_context, brush);
    const HGDIOBJ old_pen = SelectObject(device_context, GetStockObject(NULL_PEN));
    Ellipse(device_context, center_x - radius, center_y - radius, center_x + radius, center_y + radius);
    SelectObject(device_context, old_pen);
    SelectObject(device_context, old_brush);
    DeleteObject(brush);
}

void draw_enemy(HDC device_context, int center_x, int center_y, int radius) {
    const POINT glow[] = {
        {center_x, center_y - radius}, {center_x + radius, center_y},
        {center_x, center_y + radius}, {center_x - radius, center_y},
    };
    const HBRUSH brush = CreateSolidBrush(color(255, 45, 150));
    const HGDIOBJ old_brush = SelectObject(device_context, brush);
    const HGDIOBJ old_pen = SelectObject(device_context, GetStockObject(NULL_PEN));
    Polygon(device_context, glow, 4);
    SelectObject(device_context, old_pen);
    SelectObject(device_context, old_brush);
    DeleteObject(brush);

    const int inner_radius = std::max(2, radius / 2);
    const POINT core[] = {
        {center_x, center_y - inner_radius}, {center_x + inner_radius, center_y},
        {center_x, center_y + inner_radius}, {center_x - inner_radius, center_y},
    };
    const HBRUSH core_brush = CreateSolidBrush(color(64, 15, 90));
    const HGDIOBJ old_core_brush = SelectObject(device_context, core_brush);
    Polygon(device_context, core, 4);
    SelectObject(device_context, old_core_brush);
    DeleteObject(core_brush);
}

}  // namespace

KeyboardMessageRouting keyboard_message_routing(UINT message) noexcept {
    switch (message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            return KeyboardMessageRouting::Consume;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        default:
            return KeyboardMessageRouting::DeferToDefault;
    }
}

unsigned int next_simulation_wait_milliseconds(float accumulator, float fixed_step) noexcept {
    constexpr unsigned int kMaximumWaitMilliseconds = 50u;
    if (!std::isfinite(accumulator) || !std::isfinite(fixed_step) || fixed_step <= 0.0f) {
        return 0u;
    }

    const float remaining = fixed_step - accumulator;
    if (remaining <= 0.0f) {
        return 0u;
    }

    const float requested_milliseconds = std::ceil(remaining * 1000.0f);
    if (requested_milliseconds >= static_cast<float>(kMaximumWaitMilliseconds)) {
        return kMaximumWaitMilliseconds;
    }

    return requested_milliseconds < 1.0f ? 1u : static_cast<unsigned int>(requested_milliseconds);
}

int Win32App::run(HINSTANCE instance, int show_command) {
    instance_ = instance;

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpfnWndProc = Win32App::window_proc;
    window_class.lpszClassName = kWindowClassName;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (RegisterClassExW(&window_class) == 0) {
        return 1;
    }

    constexpr DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT desired_client_area{0, 0, 1080, 760};
    AdjustWindowRect(&desired_client_area, window_style, FALSE);
    window_ = CreateWindowExW(
        0, kWindowClassName, kWindowTitle, window_style, CW_USEDEFAULT, CW_USEDEFAULT,
        desired_client_area.right - desired_client_area.left,
        desired_client_area.bottom - desired_client_area.top, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) {
        UnregisterClassW(kWindowClassName, instance_);
        return 1;
    }

    ShowWindow(window_, show_command);
    UpdateWindow(window_);
    running_ = true;

    Game game;
    using clock = std::chrono::steady_clock;
    auto previous_frame = clock::now();
    float accumulator = 0.0f;

    while (running_) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        const auto current_frame = clock::now();
        float frame_delta = std::chrono::duration<float>(current_frame - previous_frame).count();
        previous_frame = current_frame;
        frame_delta = std::max(0.0f, std::min(frame_delta, kMaximumFrameDelta));
        accumulator += frame_delta;

        while (running_ && accumulator >= kFixedStep) {
            game.update(kFixedStep, read_input(game));
            accumulator -= kFixedStep;
        }

        if (running_ && !IsIconic(window_)) {
            render(game);
        }

        const unsigned int wait_milliseconds = next_simulation_wait_milliseconds(accumulator, kFixedStep);
        if (running_ && wait_milliseconds > 0u) {
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(wait_milliseconds), QS_ALLINPUT,
                                        MWMO_INPUTAVAILABLE);
        }
    }

    UnregisterClassW(kWindowClassName, instance_);
    return 0;
}

LRESULT CALLBACK Win32App::window_proc(HWND window, UINT message, WPARAM w_param, LPARAM l_param) {
    Win32App* app = reinterpret_cast<Win32App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        app = static_cast<Win32App*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    switch (message) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (keyboard_message_routing(message) == KeyboardMessageRouting::DeferToDefault) {
                return DefWindowProcW(window, message, w_param, l_param);
            }
            if (app != nullptr) {
                app->on_key_down(w_param, l_param);
            }
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (keyboard_message_routing(message) == KeyboardMessageRouting::DeferToDefault) {
                return DefWindowProcW(window, message, w_param, l_param);
            }
            if (app != nullptr) {
                app->on_key_up(w_param);
            }
            return 0;
        case WM_ACTIVATEAPP:
            if (app != nullptr && w_param == FALSE) {
                app->key_down_.fill(false);
                app->key_pressed_.fill(false);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{};
            BeginPaint(window, &paint);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_DESTROY:
            if (app != nullptr) {
                app->running_ = false;
            }
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(window, message, w_param, l_param);
    }
}

void Win32App::on_key_down(WPARAM virtual_key, LPARAM l_param) {
    if (virtual_key >= key_down_.size()) {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(virtual_key);
    const bool is_auto_repeat = (l_param & (static_cast<LPARAM>(1) << 30)) != 0;
    if (!key_down_[index] && !is_auto_repeat) {
        key_pressed_[index] = true;
    }
    key_down_[index] = true;
}

void Win32App::on_key_up(WPARAM virtual_key) {
    if (virtual_key < key_down_.size()) {
        key_down_[static_cast<std::size_t>(virtual_key)] = false;
    }
}

InputState Win32App::read_input(const Game& game) {
    const auto held = [this](int virtual_key) {
        return key_down_[static_cast<std::size_t>(virtual_key)];
    };
    const auto consume_press = [this](int virtual_key) {
        const std::size_t index = static_cast<std::size_t>(virtual_key);
        const bool pressed = key_pressed_[index];
        key_pressed_[index] = false;
        return pressed;
    };

    const bool enter_pressed = consume_press(VK_RETURN);
    const bool pause_pressed = consume_press('P');
    InputState input{};
    input.up = held('W') || held(VK_UP);
    input.down = held('S') || held(VK_DOWN);
    input.left = held('A') || held(VK_LEFT);
    input.right = held('D') || held(VK_RIGHT);
    input.dash = consume_press(VK_SPACE);
    input.start = game.state() == RunState::Start && enter_pressed;
    input.pause = pause_pressed;
    input.restart = game.state() == RunState::GameOver && enter_pressed;
    return input;
}

void Win32App::render(const Game& game) const {
    RECT client{};
    GetClientRect(window_, &client);
    const int client_width = client.right - client.left;
    const int client_height = client.bottom - client.top;
    if (client_width <= 0 || client_height <= 0) {
        return;
    }

    const HDC screen = GetDC(window_);
    const HDC buffer = CreateCompatibleDC(screen);
    const HBITMAP bitmap = CreateCompatibleBitmap(screen, client_width, client_height);
    if (screen == nullptr || buffer == nullptr || bitmap == nullptr) {
        if (bitmap != nullptr) {
            DeleteObject(bitmap);
        }
        if (buffer != nullptr) {
            DeleteDC(buffer);
        }
        if (screen != nullptr) {
            ReleaseDC(window_, screen);
        }
        return;
    }

    const HGDIOBJ old_bitmap = SelectObject(buffer, bitmap);
    const RECT background{0, 0, client_width, client_height};
    fill_rect(buffer, background, color(5, 5, 18));

    const Arena& arena = game.arena();
    const float available_width = static_cast<float>(std::max(1, client_width - 2 * kPadding));
    const float available_height = static_cast<float>(std::max(1, client_height - 155));
    const float scale = std::max(0.1f, std::min(available_width / arena.width, available_height / arena.height));
    const int arena_width = rounded(arena.width * scale);
    const int arena_height = rounded(arena.height * scale);
    const int arena_left = (client_width - arena_width) / 2;
    const int arena_top = 94 + std::max(0, (client_height - 130 - arena_height) / 2);
    const RECT arena_rect{arena_left, arena_top, arena_left + arena_width, arena_top + arena_height};

    fill_rect(buffer, arena_rect, color(11, 12, 36));
    const HPEN grid_pen = CreatePen(PS_SOLID, 1, color(21, 38, 76));
    const HGDIOBJ old_grid_pen = SelectObject(buffer, grid_pen);
    for (int x = arena_left + rounded(40.0f * scale); x < arena_rect.right; x += rounded(40.0f * scale)) {
        MoveToEx(buffer, x, arena_top, nullptr);
        LineTo(buffer, x, arena_rect.bottom);
    }
    for (int y = arena_top + rounded(40.0f * scale); y < arena_rect.bottom; y += rounded(40.0f * scale)) {
        MoveToEx(buffer, arena_left, y, nullptr);
        LineTo(buffer, arena_rect.right, y);
    }
    SelectObject(buffer, old_grid_pen);
    DeleteObject(grid_pen);

    draw_outline(buffer, arena_rect, 6, color(18, 65, 132));
    draw_outline(buffer, arena_rect, 2, color(58, 240, 255));

    const auto project_x = [arena_left, scale](float x) { return arena_left + rounded(x * scale); };
    const auto project_y = [arena_top, scale](float y) { return arena_top + rounded(y * scale); };
    const int enemy_radius = std::max(5, rounded(game.config().enemy_radius * scale));
    for (const Vec2& enemy : game.enemy_positions()) {
        draw_enemy(buffer, project_x(enemy.x), project_y(enemy.y), enemy_radius);
    }

    const Vec2& player = game.player_position();
    const int player_radius = std::max(6, rounded(game.player_radius() * scale));
    draw_circle(buffer, project_x(player.x), project_y(player.y), player_radius + 7, color(16, 72, 110));
    draw_circle(buffer, project_x(player.x), project_y(player.y), player_radius, color(68, 250, 255));
    draw_circle(buffer, project_x(player.x), project_y(player.y), std::max(3, player_radius / 3), color(240, 255, 255));

    wchar_t score_text[64]{};
    std::swprintf(score_text, sizeof(score_text) / sizeof(score_text[0]), L"SCORE  %06d",
                  std::max(0, rounded(game.score())));
    draw_text(buffer, 30, 22, 28, FW_BOLD, color(66, 242, 255), L"NEON ARENA");
    draw_text(buffer, client_width - 225, 28, 20, FW_BOLD, color(255, 95, 190), score_text);

    wchar_t dash_text[64]{};
    std::swprintf(dash_text, sizeof(dash_text) / sizeof(dash_text[0]), L"DASH  %.1fs",
                  game.dash_cooldown_remaining());
    draw_text(buffer, 32, client_height - 36, 16, FW_NORMAL, color(154, 180, 224),
              L"WASD / ARROWS  MOVE     SPACE  DASH     P  PAUSE");
    draw_text(buffer, client_width - 158, client_height - 36, 16, FW_BOLD, color(118, 230, 255), dash_text);

    const RunState state = game.state();
    if (state != RunState::Playing) {
        const RECT panel{client_width / 2 - 205, client_height / 2 - 76,
                         client_width / 2 + 205, client_height / 2 + 76};
        fill_rect(buffer, panel, color(14, 12, 42));
        draw_outline(buffer, panel, 2, state == RunState::GameOver ? color(255, 66, 154) : color(83, 239, 255));
        if (state == RunState::Start) {
            draw_text(buffer, panel.left + 85, panel.top + 25, 30, FW_BOLD, color(85, 245, 255), L"SURVIVE");
            draw_text(buffer, panel.left + 59, panel.top + 72, 18, FW_NORMAL, color(224, 236, 255),
                      L"PRESS ENTER TO START");
        } else if (state == RunState::Paused) {
            draw_text(buffer, panel.left + 106, panel.top + 25, 30, FW_BOLD, color(255, 224, 95), L"PAUSED");
            draw_text(buffer, panel.left + 72, panel.top + 72, 18, FW_NORMAL, color(224, 236, 255),
                      L"PRESS P TO RESUME");
        } else {
            draw_text(buffer, panel.left + 74, panel.top + 25, 30, FW_BOLD, color(255, 72, 165), L"GAME OVER");
            draw_text(buffer, panel.left + 49, panel.top + 72, 18, FW_NORMAL, color(224, 236, 255),
                      L"PRESS ENTER TO RESTART");
        }
    }

    BitBlt(screen, 0, 0, client_width, client_height, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    ReleaseDC(window_, screen);
}
