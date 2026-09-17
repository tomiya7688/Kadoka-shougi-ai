#pragma once

#include "kadoka/engine.hpp"
#include "kadoka/runtime/ai_backend.hpp"

#include <chrono>
#include <cstdint>
#include <optional>

namespace kadoka::shogi::runtime {

enum class TurnStatus : std::uint8_t {
    MoveApplied,
    IllegalMove,
    Resigned,
    EnteringKingDeclaration,
    NoLegalMoves,
};

struct TurnResult {
    TurnStatus status{TurnStatus::NoLegalMoves};
    std::optional<SearchResult> search_result{};
    std::optional<Position> next_position{};
    // Wall-clock time spent inside AIBackend::decide(). Core move generation
    // and validation are intentionally excluded from the player's clock.
    std::chrono::nanoseconds decision_time{0};
};

// Generic Runtime path for native, process, script, dynamic-library or network
// adapters. The backend only proposes a result; this function owns legal-move
// validation and authoritative state transition.
[[nodiscard]] TurnResult run_ai_turn(
    AIBackend& backend,
    const Position& position,
    const SearchLimits& limits = {}
);

// Backwards-compatible native Engine entry point. It is normalized through the
// same AIBackend/TurnRunner path so legality rules do not fork by backend type.
[[nodiscard]] TurnResult run_engine_turn(
    Engine& engine,
    const Position& position,
    const SearchLimits& limits = {}
);

} // namespace kadoka::shogi::runtime
