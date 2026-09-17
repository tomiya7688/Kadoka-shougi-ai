#pragma once

#include "kadoka/impasse.hpp"
#include "kadoka/runtime/game_outcome.hpp"
#include "kadoka/runtime/match_clock.hpp"
#include "kadoka/runtime/turn_runner.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace kadoka::shogi::runtime {

enum class AutomaticImpasseRule : std::uint8_t {
    Disabled,
    Jsa500Moves,
};

struct MatchLimits {
    SearchLimits black_search{};
    SearchLimits white_search{};
    // Empty means no official match clock for that side. SearchLimits may
    // still contain an advisory per-search time budget independently.
    std::optional<PlayerTimeControl> black_time_control{};
    std::optional<PlayerTimeControl> white_time_control{};
    // Total Engine::search calls allowed for one side on one ply. A value of 1
    // means an illegal output is not retried. A value of 0 stops immediately.
    std::uint32_t max_engine_attempts_per_turn{3};
    // Safety guard for malformed engines/games that do not reach another
    // adjudicated result. Repetition is checked before this guard is hit.
    std::uint32_t max_plies{512};
    // Default project rules follow current JSA official rules. Tournament or
    // engine-specific environments may disable this and apply their own
    // maximum-move / impasse policy above the runtime.
    AutomaticImpasseRule automatic_impasse_rule{AutomaticImpasseRule::Jsa500Moves};
    // Used only after an explicit offer is accepted by the opponent.
    MutualImpassePolicy mutual_impasse_policy{MutualImpassePolicy::Jsa24Point};
};

struct MatchResult {
    GameOutcome outcome{};
    Position final_position{};
    std::vector<Move> accepted_moves{};
    std::uint32_t black_illegal_outputs{0};
    std::uint32_t white_illegal_outputs{0};
    MatchClockSnapshot clock{};
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
