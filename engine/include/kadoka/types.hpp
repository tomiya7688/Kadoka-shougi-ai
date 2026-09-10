#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace kadoka::shogi {

enum class Color : std::uint8_t {
    Black = 0,
    White = 1,
};

constexpr Color opposite(Color color) noexcept {
    return color == Color::Black ? Color::White : Color::Black;
}

enum class PieceType : std::uint8_t {
    None = 0,
    Pawn,
    Lance,
    Knight,
    Silver,
    Gold,
    Bishop,
    Rook,
    King,
    ProPawn,
    ProLance,
    ProKnight,
    ProSilver,
    Horse,
    Dragon,
};

struct Piece {
    PieceType type{PieceType::None};
    Color color{Color::Black};

    [[nodiscard]] constexpr bool empty() const noexcept {
        return type == PieceType::None;
    }
};

struct Square {
    std::uint8_t file{0}; // 1..9
    std::uint8_t rank{0}; // 1..9

    [[nodiscard]] constexpr bool valid() const noexcept {
        return file >= 1 && file <= 9 && rank >= 1 && rank <= 9;
    }

    friend constexpr bool operator==(Square, Square) = default;
};

struct Move {
    std::optional<Square> from{}; // empty for drops
    Square to{};
    PieceType drop_piece{PieceType::None};
    bool promote{false};

    [[nodiscard]] constexpr bool is_drop() const noexcept {
        return !from.has_value();
    }
};

using Board = std::array<Piece, 81>;

} // namespace kadoka::shogi
