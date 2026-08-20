#include "game/game.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr float kEpsilon = 0.001f;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_near(float actual, float expected, const std::string& message) {
    require(std::fabs(actual - expected) <= kEpsilon, message);
}

void start(Game& game) {
    InputState input{};
    input.start = true;
    game.update(0.0f, input);
    require(game.state() == RunState::Playing, "start input should begin a run");
}

void player_is_clamped_inside_the_arena() {
    Game game;
    start(game);

    InputState input{};
    input.left = true;
    input.up = true;
    for (int step = 0; step < 100; ++step) {
        game.update(0.1f, input);
    }

    const Vec2 position = game.player_position();
    require(position.x >= game.player_radius(), "player must remain inside the left arena edge");
    require(position.y >= game.player_radius(), "player must remain inside the top arena edge");
}

void enemy_collision_ends_the_run() {
    GameConfig config{};
    config.initial_enemy_positions.push_back(Vec2{400.0f, 300.0f});
    Game game(config);

    InputState input{};
    input.start = true;
    game.update(0.016f, input);

    require(game.state() == RunState::GameOver, "an enemy overlapping the player should end the run");
}

void enemy_positions_expose_current_enemies() {
    GameConfig config{};
    config.initial_enemy_positions.push_back(Vec2{100.0f, 150.0f});
    config.initial_enemy_positions.push_back(Vec2{700.0f, 450.0f});
    Game game(config);

    const std::vector<Vec2>& enemies = game.enemy_positions();
    require(enemies.size() == 2, "current enemy positions should include every configured enemy");
    require_near(enemies[0].x, 100.0f, "first current enemy x position should be exposed");
    require_near(enemies[0].y, 150.0f, "first current enemy y position should be exposed");
    require_near(enemies[1].x, 700.0f, "second current enemy x position should be exposed");
    require_near(enemies[1].y, 450.0f, "second current enemy y position should be exposed");
}

void enemy_spawns_on_an_arena_edge() {
    Game game;
    start(game);
    game.update(0.25f, InputState{});

    const std::vector<Vec2>& enemies = game.enemy_positions();
    require(!enemies.empty(), "a running game should spawn an enemy over time");

    const Vec2 spawned_enemy = enemies.front();
    const Arena& arena = game.arena();
    const bool on_horizontal_edge = spawned_enemy.x == 0.0f || spawned_enemy.x == arena.width;
    const bool on_vertical_edge = spawned_enemy.y == 0.0f || spawned_enemy.y == arena.height;
    require(on_horizontal_edge || on_vertical_edge, "spawned enemy should begin on an arena edge");
}

void diagonal_input_does_not_exceed_player_speed() {
    Game game;
    start(game);

    const Vec2 start_position = game.player_position();
    InputState input{};
    input.right = true;
    input.down = true;
    constexpr float elapsed = 0.1f;
    game.update(elapsed, input);

    const float distance_moved = length(game.player_position() - start_position);
    require(distance_moved > kEpsilon, "diagonal input should move the player");
    require(distance_moved <= game.config().player_speed * elapsed + kEpsilon,
            "diagonal movement must not exceed configured player speed");
    require_near(distance_moved, game.config().player_speed * elapsed,
                 "diagonal movement should use the configured player speed");
}

void timed_waves_spawn_at_a_repeatable_cadence() {
    Game game;
    start(game);

    game.update(0.25f, InputState{});
    game.update(0.25f, InputState{});
    game.update(0.25f, InputState{});

    require(game.enemy_positions().size() == 3,
            "each elapsed spawn interval should add one enemy to the current wave");
}

void spawned_enemies_pursue_the_player() {
    Game game;
    start(game);
    game.update(0.25f, InputState{});

    require(!game.enemy_positions().empty(), "a running game should spawn an enemy before it can pursue");
    const Vec2 player_position = game.player_position();
    const Vec2 enemy_before = game.enemy_positions().front();
    const float distance_before = length(enemy_before - player_position);

    game.update(0.1f, InputState{});

    const Vec2 enemy_after = game.enemy_positions().front();
    const float distance_after = length(enemy_after - player_position);
    const float distance_moved = length(enemy_after - enemy_before);
    require(distance_moved > kEpsilon, "enemies should move while pursuing the player");
    require(distance_moved <= game.config().enemy_speed * 0.1f + kEpsilon,
            "enemy pursuit must not exceed configured enemy speed");
    require_near(distance_moved, game.config().enemy_speed * 0.1f,
                 "enemy pursuit should use configured enemy speed");
    require(distance_after < distance_before, "enemies should move toward the player between spawn waves");
}

void capped_update_discards_overdue_wave_backlog() {
    GameConfig config{};
    config.enemy_spawn_interval = 0.014f;
    Game game(config);
    start(game);

    game.update(10.0f, InputState{});
    const std::size_t enemies_after_capped_update = game.enemy_positions().size();
    require(enemies_after_capped_update == 16,
            "a capped update should spawn no more than the wave cap");

    game.update(0.0f, InputState{});
    game.update(std::numeric_limits<float>::infinity(), InputState{});
    require(game.enemy_positions().size() == enemies_after_capped_update,
            "zero or invalid dt must not release overdue full waves");

    game.update(0.005f, InputState{});
    require(game.enemy_positions().size() == enemies_after_capped_update + 1,
            "the capped update should retain only its fractional spawn remainder");
}

int arena_edge_index(const Vec2& position, const Arena& arena) {
    if (position.y == 0.0f) {
        return 0;
    }
    if (position.x == arena.width) {
        return 1;
    }
    if (position.y == arena.height) {
        return 2;
    }
    if (position.x == 0.0f) {
        return 3;
    }
    throw std::runtime_error("enemy position is not on an arena edge");
}

void deterministic_edge_selection_avoids_the_low_bit_cycle() {
    GameConfig config{};
    config.random_seed = 2u;
    config.enemy_speed = 0.0f;
    Game game(config);
    start(game);

    constexpr std::size_t spawn_count = 8;
    for (std::size_t spawn = 0; spawn < spawn_count; ++spawn) {
        game.update(0.25f, InputState{});
    }

    const std::vector<Vec2> first_run = game.enemy_positions();
    require(first_run.size() == spawn_count, "test setup should produce one enemy per timed wave");

    constexpr int kOldLowBitCycle[] = {3, 0, 1, 2};
    bool repeats_old_cycle = true;
    for (std::size_t index = 0; index < first_run.size(); ++index) {
        if (arena_edge_index(first_run[index], game.arena()) != kOldLowBitCycle[index % 4]) {
            repeats_old_cycle = false;
            break;
        }
    }
    require(!repeats_old_cycle, "edge selection must not use the LCG low-bit repeating cycle");

    InputState restart{};
    restart.restart = true;
    game.update(0.0f, restart);
    start(game);
    for (std::size_t spawn = 0; spawn < spawn_count; ++spawn) {
        game.update(0.25f, InputState{});
    }

    const std::vector<Vec2>& restarted_run = game.enemy_positions();
    require(restarted_run.size() == first_run.size(), "restart should reproduce the same number of spawns");
    for (std::size_t index = 0; index < first_run.size(); ++index) {
        require_near(restarted_run[index].x, first_run[index].x,
                     "restart with the same seed should reproduce enemy spawn x coordinates");
        require_near(restarted_run[index].y, first_run[index].y,
                     "restart with the same seed should reproduce enemy spawn y coordinates");
    }
}

void invalid_delta_does_not_advance_score_or_waves() {
    Game game;
    start(game);
    game.update(0.1f, InputState{});
    const float score_before_invalid_delta = game.score();
    const std::size_t enemies_before_invalid_delta = game.enemy_positions().size();

    game.update(std::numeric_limits<float>::infinity(), InputState{});

    require_near(game.score(), score_before_invalid_delta, "invalid dt must not advance survival score");
    require(game.enemy_positions().size() == enemies_before_invalid_delta,
            "invalid dt must not advance timed wave spawning");
}

void dash_respects_its_cooldown() {
    Game game;
    start(game);

    InputState dash{};
    dash.dash = true;
    game.update(0.0f, dash);
    require_near(game.dash_cooldown_remaining(), game.config().dash_cooldown,
                 "first dash should start its cooldown");

    game.update(0.1f, dash);
    require_near(game.dash_cooldown_remaining(), game.config().dash_cooldown - 0.1f,
                 "holding dash during cooldown must not trigger a second dash");
}

void dash_uses_normal_speed_after_its_duration_expires() {
    Game game;
    start(game);

    InputState dash{};
    dash.dash = true;
    game.update(0.0f, dash);

    InputState move_right{};
    move_right.right = true;
    game.update(0.20f, move_right);

    const float expected_distance = game.config().dash_speed * game.config().dash_duration +
                                    game.config().player_speed * (0.20f - game.config().dash_duration);
    require_near(game.player_position().x, game.arena().width * 0.5f + expected_distance,
                 "player should only dash for its remaining duration");
}

void paused_game_does_not_advance_score() {
    Game game;
    start(game);
    game.update(1.0f, InputState{});
    const float score_before_pause = game.score();

    InputState pause{};
    pause.pause = true;
    game.update(0.0f, pause);
    require(game.state() == RunState::Paused, "pause input should pause a running game");
    game.update(5.0f, InputState{});

    require_near(game.score(), score_before_pause, "paused simulation must not advance score");
}

void restart_restores_a_fresh_run() {
    Game game;
    start(game);

    InputState move_and_dash{};
    move_and_dash.right = true;
    move_and_dash.dash = true;
    game.update(0.5f, move_and_dash);
    require(game.score() > 0.0f, "running game should accrue score before restart");

    InputState restart{};
    restart.restart = true;
    game.update(0.0f, restart);

    require(game.state() == RunState::Start, "restart should restore the start state");
    require_near(game.score(), 0.0f, "restart should clear score");
    require_near(game.dash_cooldown_remaining(), 0.0f, "restart should clear dash cooldown");
    require_near(game.player_position().x, game.arena().width * 0.5f,
                 "restart should restore the centered player x coordinate");
    require_near(game.player_position().y, game.arena().height * 0.5f,
                 "restart should restore the centered player y coordinate");
}

void restart_from_game_over_starts_a_fresh_active_run() {
    GameConfig config{};
    config.enemy_spawn_interval = 100.0f;
    config.initial_enemy_positions.push_back(Vec2{500.0f, 300.0f});
    Game game(config);
    start(game);

    game.update(0.1f, InputState{});
    require(game.score() > 0.0f, "the run should accrue score before game over");

    InputState move_right{};
    move_right.right = true;
    game.update(0.25f, move_right);
    require(game.state() == RunState::GameOver, "test setup should drive the run to game over");

    InputState restart{};
    restart.restart = true;
    game.update(0.0f, restart);

    require(game.state() == RunState::Playing, "restart from game over should immediately begin a new run");
    require_near(game.score(), 0.0f, "restart from game over should clear survival score");
    require(game.enemy_positions().size() == 1,
            "restart from game over should restore the configured initial enemies");
    require_near(game.enemy_positions().front().x, 500.0f,
                 "restart from game over should restore initial enemy x position");
    require_near(game.enemy_positions().front().y, 300.0f,
                 "restart from game over should restore initial enemy y position");
}

struct TestCase {
    const char* name;
    std::function<void()> run;
};

}  // namespace

int main() {
    const std::vector<TestCase> tests{
        {"player_is_clamped_inside_the_arena", player_is_clamped_inside_the_arena},
        {"enemy_collision_ends_the_run", enemy_collision_ends_the_run},
        {"enemy_positions_expose_current_enemies", enemy_positions_expose_current_enemies},
        {"enemy_spawns_on_an_arena_edge", enemy_spawns_on_an_arena_edge},
        {"diagonal_input_does_not_exceed_player_speed", diagonal_input_does_not_exceed_player_speed},
        {"timed_waves_spawn_at_a_repeatable_cadence", timed_waves_spawn_at_a_repeatable_cadence},
        {"spawned_enemies_pursue_the_player", spawned_enemies_pursue_the_player},
        {"capped_update_discards_overdue_wave_backlog", capped_update_discards_overdue_wave_backlog},
        {"deterministic_edge_selection_avoids_the_low_bit_cycle",
         deterministic_edge_selection_avoids_the_low_bit_cycle},
        {"invalid_delta_does_not_advance_score_or_waves", invalid_delta_does_not_advance_score_or_waves},
        {"dash_respects_its_cooldown", dash_respects_its_cooldown},
        {"dash_uses_normal_speed_after_its_duration_expires", dash_uses_normal_speed_after_its_duration_expires},
        {"paused_game_does_not_advance_score", paused_game_does_not_advance_score},
        {"restart_restores_a_fresh_run", restart_restores_a_fresh_run},
        {"restart_from_game_over_starts_a_fresh_active_run", restart_from_game_over_starts_a_fresh_active_run},
    };

    int failed = 0;
    for (const TestCase& test : tests) {
        try {
            test.run();
            std::cout << "PASS " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << test.name << ": " << error.what() << '\n';
        }
    }

    const int passed = static_cast<int>(tests.size()) - failed;
    std::cout << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
