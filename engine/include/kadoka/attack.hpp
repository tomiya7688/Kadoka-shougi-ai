#pragma once

#include "kadoka/position.hpp"

namespace kadoka::shogi {

[[nodiscard]] bool is_square_attacked(const Position& position, Square square, Color by_color);
[[nodiscard]] bool is_in_check(const Position& position, Color color);

} // namespace kadoka::shogi
