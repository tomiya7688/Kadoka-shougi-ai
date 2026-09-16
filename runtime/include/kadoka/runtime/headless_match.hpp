#pragma once

#include "kadoka/runtime/game_outcome.hpp"
#include "kadoka/runtime/turn_runner.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace kadoka::shogi::runtime {

struct MatchLimits {
    SearchLimits black_search{};
    SearchLimits white_search{};
    // Total Engine::search calls allowed for one side on one ply. A value of 1
    // means an illegal output is not retried. A value of 0 stops immediately.
    std::uint32_t max_engine_attempts_per_turn{3};
    // Safety guard for malformed engines/games that do not reach another
    // adjudicated result. Repetition is checked before this guard is hit.
    std::uint32_t max_plies{512};
};

struct MatchResult {
    GameOutcome outcome{};
    Position final_position{};
    std::vector<Move> accepted_moves{};
    std::uint32_t black_illegal_outputs{0};
    std::uint32_t white_illegal_outputs{0};
    // Runtime diagnostic only. Set when execution stops because the side to
    // move could not continue, without implying an official game loss.
    std::optional<Color> stopped_side{};
};

// Runs a GUI-free match using the same validated runtime path for both engines.
// Only accepted legal moves enter accepted_moves and advance final_position.
[[nodiscard]] MatchResult run_headless_match(
    Engine& black_engine,
    Engine& white_engine,
    const Position& initial_position,
    const MatchLimits& limits = {}
);

} // namespace kadoka::shogi::runtime
