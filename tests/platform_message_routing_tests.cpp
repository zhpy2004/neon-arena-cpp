#include "platform/win32_app.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void normal_key_messages_are_consumed() {
    require(keyboard_message_routing(WM_KEYDOWN) == KeyboardMessageRouting::Consume,
            "WM_KEYDOWN should be consumed for gameplay input");
    require(keyboard_message_routing(WM_KEYUP) == KeyboardMessageRouting::Consume,
            "WM_KEYUP should be consumed for gameplay input");
}

void system_key_messages_are_deferred() {
    require(keyboard_message_routing(WM_SYSKEYDOWN) == KeyboardMessageRouting::DeferToDefault,
            "WM_SYSKEYDOWN should reach DefWindowProcW");
    require(keyboard_message_routing(WM_SYSKEYUP) == KeyboardMessageRouting::DeferToDefault,
            "WM_SYSKEYUP should reach DefWindowProcW");
}

void next_wait_is_bounded_by_the_remaining_fixed_step() {
    constexpr float fixed_step = 1.0f / 60.0f;
    require(next_simulation_wait_milliseconds(0.0f, fixed_step) == 17u,
            "an empty accumulator should wait for the next 60 Hz step");
    require(next_simulation_wait_milliseconds(0.010f, fixed_step) == 7u,
            "a partial accumulator should wait only for its remaining step time");
    require(next_simulation_wait_milliseconds(fixed_step, fixed_step) == 0u,
            "a due simulation step should not wait");
    require(next_simulation_wait_milliseconds(0.0f, 1.0f) == 50u,
            "large fixed steps should use the bounded wait interval");
}

}  // namespace

int main() {
    int failed = 0;
    const struct {
        const char* name;
        void (*run)();
    } tests[] = {
        {"normal_key_messages_are_consumed", normal_key_messages_are_consumed},
        {"system_key_messages_are_deferred", system_key_messages_are_deferred},
        {"next_wait_is_bounded_by_the_remaining_fixed_step", next_wait_is_bounded_by_the_remaining_fixed_step},
    };

    for (const auto& test : tests) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << (static_cast<int>(sizeof(tests) / sizeof(tests[0])) - failed) << " passed, " << failed
              << " failed\n";
    return failed == 0 ? 0 : 1;
}
