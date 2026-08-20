#include "game/game.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr float kMaximumFrameDelta = 0.25f;
constexpr unsigned int kMaximumWavesPerUpdate = 16;

float clamped_delta(float dt) noexcept {
    if (!std::isfinite(dt) || dt <= 0.0f) {
        return 0.0f;
    }

    return std::min(dt, kMaximumFrameDelta);
}

float clamp(float value, float minimum, float maximum) noexcept {
    return std::max(minimum, std::min(value, maximum));
}

}  // namespace

Game::Game(GameConfig config) : config_(std::move(config)) {
    reset();
}

void Game::update(float dt, InputState input) {
    if (input.restart) {
        const bool restarting_after_game_over = state_ == RunState::GameOver;
        reset();
        if (restarting_after_game_over) {
            state_ = RunState::Playing;
        }
        return;
    }

    if (state_ == RunState::Start) {
        if (input.start) {
            state_ = RunState::Playing;
        } else {
            return;
        }
    }

    if (state_ == RunState::Paused) {
        if (input.pause) {
            state_ = RunState::Playing;
        }
        return;
    }

    if (state_ == RunState::GameOver) {
        return;
    }

    if (input.pause) {
        state_ = RunState::Paused;
        return;
    }

    const float elapsed = clamped_delta(dt);
    dash_cooldown_remaining_ = std::max(0.0f, dash_cooldown_remaining_ - elapsed);
    if (input.dash && dash_cooldown_remaining_ == 0.0f) {
        dash_cooldown_remaining_ = config_.dash_cooldown;
        dash_time_remaining_ = config_.dash_duration;
    }

    const float dash_elapsed = std::min(elapsed, dash_time_remaining_);
    update_player(dash_elapsed, input);
    dash_time_remaining_ -= dash_elapsed;
    update_player(elapsed - dash_elapsed, input);
    update_enemies(elapsed);
    spawn_enemies(elapsed);
    resolve_collisions();

    if (state_ == RunState::Playing) {
        score_ += elapsed * config_.score_per_second;
    }
}

RunState Game::state() const noexcept {
    return state_;
}

float Game::score() const noexcept {
    return score_;
}

const Vec2& Game::player_position() const noexcept {
    return player_position_;
}

const std::vector<Vec2>& Game::enemy_positions() const noexcept {
    return enemy_positions_;
}

float Game::player_radius() const noexcept {
    return config_.player_radius;
}

float Game::dash_cooldown_remaining() const noexcept {
    return dash_cooldown_remaining_;
}

const Arena& Game::arena() const noexcept {
    return config_.arena;
}

const GameConfig& Game::config() const noexcept {
    return config_;
}

void Game::reset() {
    state_ = RunState::Start;
    player_position_ = Vec2{config_.arena.width * 0.5f, config_.arena.height * 0.5f};
    clamp_player_to_arena();
    enemy_positions_ = config_.initial_enemy_positions;
    score_ = 0.0f;
    dash_cooldown_remaining_ = 0.0f;
    dash_time_remaining_ = 0.0f;
    spawn_time_accumulator_ = 0.0f;
    random_state_ = config_.random_seed;
}

void Game::update_player(float dt, const InputState& input) {
    Vec2 direction{};
    direction.x = (input.right ? 1.0f : 0.0f) - (input.left ? 1.0f : 0.0f);
    direction.y = (input.down ? 1.0f : 0.0f) - (input.up ? 1.0f : 0.0f);

    const float speed = dash_time_remaining_ > 0.0f ? config_.dash_speed : config_.player_speed;
    player_position_ += normalized(direction) * (speed * dt);
    clamp_player_to_arena();
}

void Game::update_enemies(float dt) {
    for (Vec2& enemy_position : enemy_positions_) {
        const Vec2 direction = normalized(player_position_ - enemy_position);
        enemy_position += direction * (config_.enemy_speed * dt);
    }
}

void Game::spawn_enemies(float dt) {
    if (!std::isfinite(config_.enemy_spawn_interval) || config_.enemy_spawn_interval <= 0.0f ||
        config_.enemies_per_wave == 0) {
        return;
    }

    spawn_time_accumulator_ += dt;
    unsigned int spawned_waves = 0;
    while (spawn_time_accumulator_ >= config_.enemy_spawn_interval &&
           spawned_waves < kMaximumWavesPerUpdate) {
        spawn_time_accumulator_ -= config_.enemy_spawn_interval;
        for (unsigned int enemy = 0; enemy < config_.enemies_per_wave; ++enemy) {
            spawn_enemy_on_edge();
        }
        ++spawned_waves;
    }

    if (spawned_waves == kMaximumWavesPerUpdate) {
        spawn_time_accumulator_ = std::fmod(spawn_time_accumulator_, config_.enemy_spawn_interval);
    }
}

void Game::spawn_enemy_on_edge() {
    constexpr float kRandomUnitScale = 1.0f / 16777216.0f;
    const float along_width = static_cast<float>(next_random() >> 8) * kRandomUnitScale * config_.arena.width;
    const float along_height = static_cast<float>(next_random() >> 8) * kRandomUnitScale * config_.arena.height;

    switch ((next_random() >> 30u) & 3u) {
        case 0u:
            enemy_positions_.push_back(Vec2{along_width, 0.0f});
            break;
        case 1u:
            enemy_positions_.push_back(Vec2{config_.arena.width, along_height});
            break;
        case 2u:
            enemy_positions_.push_back(Vec2{along_width, config_.arena.height});
            break;
        default:
            enemy_positions_.push_back(Vec2{0.0f, along_height});
            break;
    }
}

void Game::resolve_collisions() {
    const float collision_distance = config_.player_radius + config_.enemy_radius;
    const float collision_distance_squared = collision_distance * collision_distance;

    for (const Vec2& enemy_position : enemy_positions_) {
        if (length_squared(enemy_position - player_position_) <= collision_distance_squared) {
            state_ = RunState::GameOver;
            return;
        }
    }
}

void Game::clamp_player_to_arena() {
    const float minimum_x = config_.player_radius;
    const float minimum_y = config_.player_radius;
    const float maximum_x = std::max(minimum_x, config_.arena.width - config_.player_radius);
    const float maximum_y = std::max(minimum_y, config_.arena.height - config_.player_radius);

    player_position_.x = clamp(player_position_.x, minimum_x, maximum_x);
    player_position_.y = clamp(player_position_.y, minimum_y, maximum_y);
}

std::uint32_t Game::next_random() noexcept {
    random_state_ = random_state_ * 1664525u + 1013904223u;
    return random_state_;
}
