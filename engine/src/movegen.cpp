#include "kadoka/movegen.hpp"

#include <array>
#include <cstdint>

namespace kadoka::shogi {
namespace {

constexpr bool inside(int file, int rank) noexcept {
    return file >= 1 && file <= 9 && rank >= 1 && rank <= 9;
}

constexpr bool in_promotion_zone(Color color, int rank) noexcept {
    return color == Color::Black ? rank <= 3 : rank >= 7;
}

constexpr bool promotable(PieceType type) noexcept {
    switch (type) {
    case PieceType::Pawn:
    case PieceType::Lance:
    case PieceType::Knight:
    case PieceType::Silver:
    case PieceType::Bishop:
    case PieceType::Rook:
        return true;
    default:
        return false;
    }
}

constexpr bool mandatory_promotion(PieceType type, Color color, int to_rank) noexcept {
    const int last = color == Color::Black ? 1 : 9;
    const int second_last = color == Color::Black ? 2 : 8;
    if ((type == PieceType::Pawn || type == PieceType::Lance) && to_rank == last) {
        return true;
    }
    return type == PieceType::Knight && (to_rank == last || to_rank == second_last);
}

void add_board_move(
    const Position& position,
    std::vector<Move>& moves,
    Square from,
    Square to,
    PieceType type,
    Color color) {
    const Piece& target = position.at(to);
    if (!target.empty() && target.color == color) {
        return;
    }

    const bool can_promote = promotable(type)
        && (in_promotion_zone(color, from.rank) || in_promotion_zone(color, to.rank));
    const bool must_promote = mandatory_promotion(type, color, to.rank);

    if (!must_promote) {
        moves.push_back(Move{from, to, PieceType::None, false});
    }
    if (can_promote) {
        moves.push_back(Move{from, to, PieceType::None, true});
    }
}

void add_step(
    const Position& position,
    std::vector<Move>& moves,
    Square from,
    PieceType type,
    Color color,
    int df,
    int dr) {
    const int forward = color == Color::Black ? -1 : 1;
    const int file = static_cast<int>(from.file) + df;
    const int rank = static_cast<int>(from.rank) + dr * forward;
    if (!inside(file, rank)) {
        return;
    }
    add_board_move(
        position,
        moves,
        from,
        Square{static_cast<std::uint8_t>(file), static_cast<std::uint8_t>(rank)},
        type,
        color);
}

void add_slide(
    const Position& position,
    std::vector<Move>& moves,
    Square from,
    PieceType type,
    Color color,
    int df,
    int dr,
    bool relative_forward) {
    const int forward = color == Color::Black ? -1 : 1;
    const int rank_step = relative_forward ? dr * forward : dr;
    int file = static_cast<int>(from.file) + df;
    int rank = static_cast<int>(from.rank) + rank_step;

    while (inside(file, rank)) {
        const Square to{static_cast<std::uint8_t>(file), static_cast<std::uint8_t>(rank)};
        const Piece& target = position.at(to);
        if (!target.empty() && target.color == color) {
            break;
        }

        add_board_move(position, moves, from, to, type, color);
        if (!target.empty()) {
            break;
        }

        file += df;
        rank += rank_step;
    }
}

void add_gold_moves(const Position& position, std::vector<Move>& moves, Square from, PieceType type, Color color) {
    constexpr std::array<std::array<int, 2>, 6> deltas{{
        {{-1, 1}}, {{0, 1}}, {{1, 1}},
        {{-1, 0}}, {{1, 0}}, {{0, -1}},
    }};
    for (const auto& delta : deltas) {
        add_step(position, moves, from, type, color, delta[0], delta[1]);
    }
}

bool has_unpromoted_pawn_on_file(const Position& position, Color color, std::uint8_t file) {
    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        const Piece& piece = position.at(Square{file, rank});
        if (!piece.empty() && piece.color == color && piece.type == PieceType::Pawn) {
            return true;
        }
    }
    return false;
}

bool drop_allowed(const Position& position, Color color, PieceType type, Square to) {
    const int last = color == Color::Black ? 1 : 9;
    const int second_last = color == Color::Black ? 2 : 8;

    if ((type == PieceType::Pawn || type == PieceType::Lance) && to.rank == last) {
        return false;
    }
    if (type == PieceType::Knight && (to.rank == last || to.rank == second_last)) {
        return false;
    }
    if (type == PieceType::Pawn && has_unpromoted_pawn_on_file(position, color, to.file)) {
        return false;
    }
    return true;
}

} // namespace

std::vector<Move> generate_pseudo_legal_moves(const Position& position) {
    std::vector<Move> moves;
    const Color side = position.side_to_move();

    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        for (std::uint8_t file = 1; file <= 9; ++file) {
            const Square from{file, rank};
            const Piece& piece = position.at(from);
            if (piece.empty() || piece.color != side) {
                continue;
            }

            switch (piece.type) {
            case PieceType::Pawn:
                add_step(position, moves, from, piece.type, side, 0, 1);
                break;
            case PieceType::Lance:
                add_slide(position, moves, from, piece.type, side, 0, 1, true);
                break;
            case PieceType::Knight:
                add_step(position, moves, from, piece.type, side, -1, 2);
                add_step(position, moves, from, piece.type, side, 1, 2);
                break;
            case PieceType::Silver:
                add_step(position, moves, from, piece.type, side, -1, 1);
                add_step(position, moves, from, piece.type, side, 0, 1);
                add_step(position, moves, from, piece.type, side, 1, 1);
                add_step(position, moves, from, piece.type, side, -1, -1);
                add_step(position, moves, from, piece.type, side, 1, -1);
                break;
            case PieceType::Gold:
            case PieceType::ProPawn:
            case PieceType::ProLance:
            case PieceType::ProKnight:
            case PieceType::ProSilver:
                add_gold_moves(position, moves, from, piece.type, side);
                break;
            case PieceType::Bishop:
            case PieceType::Horse:
                add_slide(position, moves, from, piece.type, side, -1, -1, false);
                add_slide(position, moves, from, piece.type, side, -1, 1, false);
                add_slide(position, moves, from, piece.type, side, 1, -1, false);
                add_slide(position, moves, from, piece.type, side, 1, 1, false);
                if (piece.type == PieceType::Horse) {
                    add_step(position, moves, from, piece.type, side, -1, 0);
                    add_step(position, moves, from, piece.type, side, 1, 0);
                    add_step(position, moves, from, piece.type, side, 0, 1);
                    add_step(position, moves, from, piece.type, side, 0, -1);
                }
                break;
            case PieceType::Rook:
            case PieceType::Dragon:
                add_slide(position, moves, from, piece.type, side, -1, 0, false);
                add_slide(position, moves, from, piece.type, side, 1, 0, false);
                add_slide(position, moves, from, piece.type, side, 0, -1, false);
                add_slide(position, moves, from, piece.type, side, 0, 1, false);
                if (piece.type == PieceType::Dragon) {
                    add_step(position, moves, from, piece.type, side, -1, 1);
                    add_step(position, moves, from, piece.type, side, 1, 1);
                    add_step(position, moves, from, piece.type, side, -1, -1);
                    add_step(position, moves, from, piece.type, side, 1, -1);
                }
                break;
            case PieceType::King:
                for (int df = -1; df <= 1; ++df) {
                    for (int dr = -1; dr <= 1; ++dr) {
                        if (df != 0 || dr != 0) {
                            add_step(position, moves, from, piece.type, side, df, dr);
                        }
                    }
                }
                break;
            case PieceType::None:
                break;
            }
        }
    }

    constexpr std::array<PieceType, 7> hand_types{
        PieceType::Pawn, PieceType::Lance, PieceType::Knight, PieceType::Silver,
        PieceType::Gold, PieceType::Bishop, PieceType::Rook,
    };
    for (PieceType type : hand_types) {
        if (position.hand_count(side, type) == 0) {
            continue;
        }
        for (std::uint8_t rank = 1; rank <= 9; ++rank) {
            for (std::uint8_t file = 1; file <= 9; ++file) {
                const Square to{file, rank};
                if (!position.at(to).empty() || !drop_allowed(position, side, type, to)) {
                    continue;
                }
                moves.push_back(Move{std::nullopt, to, type, false});
            }
        }
    }

    return moves;
}

} // namespace kadoka::shogi
