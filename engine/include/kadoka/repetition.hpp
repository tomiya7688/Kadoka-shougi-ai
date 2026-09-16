#pragma once

#include "kadoka/position.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace kadoka::shogi {

enum class RepetitionStatus : std::uint8_t {
    None,
    Draw,
    BlackLosesPerpetualCheck,
    WhiteLosesPerpetualCheck,
};

struct RepetitionResult {
    RepetitionStatus status{RepetitionStatus::None};
    // Indices in the supplied history that bound the four-occurrence sequence.
    std::size_t first_occurrence{0};
    std::size_t fourth_occurrence{0};
};

// `history` must contain the initial position followed by every accepted legal
// post-move position in chronological order. The latest position is adjudicated.
// Repetition identity is board + both hands + side to move; ply is ignored.
[[nodiscard]] RepetitionResult adjudicate_repetition(std::span<const Position> history);

} // namespace kadoka::shogi
