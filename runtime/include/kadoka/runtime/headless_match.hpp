#pragma once

#include "kadoka/runtime/turn_runner.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace kadoka::shogi::runtime {

enum class MatchEndReason : std::uint8_t {
    NoLegalMoves,
    EngineAttemptLimit,
    PlyLimit,
};

struct MatchLimits {
    SearchLimits black_search{};
    SearchLimits white_search{};
    // Total Engine::search calls allowed for one side on one ply. A value of 1
    // means an illegal output is not retried. A value of 0 stops immediately.
    std::uint32_t max_engine_attempts_per_turn{3};
    // Safety guard until repetition/perpetual-check adjudication is implemented.
    std::uint32_t max_plies{512};
};

struct MatchResult {
    MatchEndReason end_reason{MatchEndReason::PlyLimit};
    Position final_position{};
    std::vector<Move> accepted_moves{};
    std::uint32_t black_illegal_outputs{0};
    std::uint32_t white_illegal_outputs{0};
    // Side whose turn could not continue for NoLegalMoves/EngineAttemptLimit.
    // Empty for neutral safeguards such as PlyLimit.
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
