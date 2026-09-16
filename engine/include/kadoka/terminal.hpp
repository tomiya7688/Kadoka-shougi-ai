#pragma once

#include "kadoka/position.hpp"

#include <cstdint>
#include <optional>

namespace kadoka::shogi {

enum class TerminalPositionStatus : std::uint8_t {
    Ongoing,
    Checkmate,
    NoLegalMoves,
};

struct TerminalPositionResult {
    TerminalPositionStatus status{TerminalPositionStatus::Ongoing};
    std::optional<Color> winner{};
    std::optional<Color> loser{};
};

// Classifies terminal facts that are derivable from one canonical position.
// History-dependent rules such as repetition are adjudicated separately.
[[nodiscard]] TerminalPositionResult adjudicate_terminal_position(const Position& position);

} // namespace kadoka::shogi
