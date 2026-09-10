#pragma once

#include "kadoka/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kadoka::shogi {

struct Hands {
    // Pawn, Lance, Knight, Silver, Gold, Bishop, Rook
    std::array<std::uint8_t, 7> black{};
    std::array<std::uint8_t, 7> white{};
};

class Position {
public:
    Position();

    [[nodiscard]] static Position startpos();

    [[nodiscard]] const Board& board() const noexcept { return board_; }
    [[nodiscard]] const Hands& hands() const noexcept { return hands_; }
    [[nodiscard]] Color side_to_move() const noexcept { return side_to_move_; }
    [[nodiscard]] std::uint32_t ply() const noexcept { return ply_; }

    [[nodiscard]] const Piece& at(Square square) const;
    [[nodiscard]] std::string to_sfen() const;

    // Intentionally conservative bootstrap API. Legal move generation and
    // make/unmake are added next, after representation tests are locked down.

private:
    [[nodiscard]] static std::size_t index_of(Square square);

    Board board_{};
    Hands hands_{};
    Color side_to_move_{Color::Black};
    std::uint32_t ply_{1};
};

} // namespace kadoka::shogi
