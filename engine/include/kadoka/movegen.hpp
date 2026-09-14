#pragma once

#include "kadoka/position.hpp"

#include <vector>

namespace kadoka::shogi {

// Piece movement, occupancy, promotion and basic drop restrictions only.
[[nodiscard]] std::vector<Move> generate_pseudo_legal_moves(const Position& position);

// Filters pseudo-legal moves that leave the moving side's king in check and
// rejects pawn-drop mate (uchi-fuzume).
[[nodiscard]] std::vector<Move> generate_legal_moves(const Position& position);

} // namespace kadoka::shogi
