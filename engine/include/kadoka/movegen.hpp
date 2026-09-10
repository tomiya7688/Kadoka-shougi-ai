#pragma once

#include "kadoka/position.hpp"

#include <vector>

namespace kadoka::shogi {

// Generates moves that satisfy piece movement, occupancy, promotion, and
// basic drop restrictions. It intentionally does NOT reject moves that leave
// the moving side's king in check and does NOT implement pawn-drop mate.
[[nodiscard]] std::vector<Move> generate_pseudo_legal_moves(const Position& position);

} // namespace kadoka::shogi
