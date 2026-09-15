#pragma once

#include "kadoka/engine.hpp"

#include <cstdint>
#include <optional>

namespace kadoka::shogi::runtime {

enum class TurnStatus : std::uint8_t {
    MoveApplied,
    IllegalMove,
    NoLegalMoves,
};

struct TurnResult {
    TurnStatus status{TurnStatus::NoLegalMoves};
    std::optional<SearchResult> search_result{};
    std::optional<Position> next_position{};
};

// Runs exactly one engine decision against the authoritative legal-move list.
// Illegal engine output never mutates the canonical position; callers may retry
// the same engine (or another adapter) with the same Position.
[[nodiscard]] TurnResult run_engine_turn(
    Engine& engine,
    const Position& position,
    const SearchLimits& limits = {}
);

} // namespace kadoka::shogi::runtime
