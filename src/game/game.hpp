#pragma once

#include "core/vec2.hpp"

#include <cstdint>
#include <vector>

enum class RunState {
    Start,
    Playing,
    Paused,
    GameOver,
};

struct InputState {
    bool up{false};
    bool down{false};
    bool left{false};
    bool right{false};
    bool dash{false};
    bool start{false};
    bool pause{false};
    bool restart{false};
};

struct Arena {
    float width{800.0f};
    float height{600.0f};
};

struct GameConfig {
    Arena arena{};
    float player_radius{16.0f};
    float player_speed{240.0f};
    float dash_speed{650.0f};
    float dash_duration{0.12f};
    float dash_cooldown{1.0f};
    float enemy_radius{16.0f};
    float enemy_speed{90.0f};
    float enemy_spawn_interval{0.25f};
    unsigned int enemies_per_wave{1};
    std::uint32_t random_seed{0x4e454f4eu};
    float score_per_second{10.0f};
    std::vector<Vec2> initial_enemy_positions{};
};

class Game {
public:
    explicit Game(GameConfig config = {});

    void update(float dt, InputState input);

    RunState state() const noexcept;
    float score() const noexcept;
    const Vec2& player_position() const noexcept;
    const std::vector<Vec2>& enemy_positions() const noexcept;
    float player_radius() const noexcept;
    float dash_cooldown_remaining() const noexcept;
    const Arena& arena() const noexcept;
    const GameConfig& config() const noexcept;

private:
    void reset();
    void update_player(float dt, const InputState& input);
    void update_enemies(float dt);
    void spawn_enemies(float dt);
    void spawn_enemy_on_edge();
    void resolve_collisions();
    void clamp_player_to_arena();
    std::uint32_t next_random() noexcept;

    GameConfig config_;
    RunState state_{RunState::Start};
    Vec2 player_position_{};
    std::vector<Vec2> enemy_positions_{};
    float score_{0.0f};
    float dash_cooldown_remaining_{0.0f};
    float dash_time_remaining_{0.0f};
    float spawn_time_accumulator_{0.0f};
    std::uint32_t random_state_{0u};
};
