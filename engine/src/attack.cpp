#include "kadoka/attack.hpp"

#include <cstdlib>

namespace kadoka::shogi {
namespace {

constexpr int sign(int x) noexcept { return (x > 0) - (x < 0); }

bool clear_ray(const Position& p, Square from, Square to) {
    int f = static_cast<int>(from.file);
    int r = static_cast<int>(from.rank);
    const int df = sign(static_cast<int>(to.file) - f);
    const int dr = sign(static_cast<int>(to.rank) - r);
    f += df;
    r += dr;
    while (f != to.file || r != to.rank) {
        if (!p.at(Square{static_cast<std::uint8_t>(f), static_cast<std::uint8_t>(r)}).empty()) return false;
        f += df;
        r += dr;
    }
    return true;
}

bool attacks(const Position& p, Square from, Piece piece, Square to) {
    const int raw_df = static_cast<int>(to.file) - static_cast<int>(from.file);
    const int raw_dr = static_cast<int>(to.rank) - static_cast<int>(from.rank);
    const int forward = piece.color == Color::Black ? -1 : 1;
    const int df = raw_df;
    const int dr = raw_dr * forward;
    const int adf = std::abs(raw_df);
    const int adr = std::abs(raw_dr);

    auto gold = [&] {
        return (dr == 1 && adf <= 1) || (dr == 0 && adf == 1) || (dr == -1 && df == 0);
    };

    switch (piece.type) {
    case PieceType::Pawn: return df == 0 && dr == 1;
    case PieceType::Lance:
        return df == 0 && dr > 0 && clear_ray(p, from, to);
    case PieceType::Knight: return adf == 1 && dr == 2;
    case PieceType::Silver: return (dr == 1 && adf <= 1) || (dr == -1 && adf == 1);
    case PieceType::Gold:
    case PieceType::ProPawn:
    case PieceType::ProLance:
    case PieceType::ProKnight:
    case PieceType::ProSilver:
        return gold();
    case PieceType::Bishop:
        return adf == adr && adf > 0 && clear_ray(p, from, to);
    case PieceType::Rook:
        return ((raw_df == 0) != (raw_dr == 0)) && clear_ray(p, from, to);
    case PieceType::King:
        return adf <= 1 && adr <= 1 && (adf != 0 || adr != 0);
    case PieceType::Horse:
        return (adf == adr && adf > 0 && clear_ray(p, from, to))
            || ((adf + adr) == 1);
    case PieceType::Dragon:
        return ((((raw_df == 0) != (raw_dr == 0))) && clear_ray(p, from, to))
            || (adf == 1 && adr == 1);
    case PieceType::None:
        return false;
    }
    return false;
}

} // namespace

bool is_square_attacked(const Position& position, Square square, Color by_color) {
    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        for (std::uint8_t file = 1; file <= 9; ++file) {
            const Square from{file, rank};
            const Piece piece = position.at(from);
            if (!piece.empty() && piece.color == by_color && attacks(position, from, piece, square)) return true;
        }
    }
    return false;
}

bool is_in_check(const Position& position, Color color) {
    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        for (std::uint8_t file = 1; file <= 9; ++file) {
            const Square sq{file, rank};
            const Piece piece = position.at(sq);
            if (!piece.empty() && piece.color == color && piece.type == PieceType::King) {
                return is_square_attacked(position, sq, opposite(color));
            }
        }
    }
    return false;
}

} // namespace kadoka::shogi
