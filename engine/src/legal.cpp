#include "kadoka/movegen.hpp"
#include "kadoka/attack.hpp"

#include <cstdint>

namespace kadoka::shogi {
namespace {

bool dropped_pawn_attacks_king(const Position& position, const Move& move, Color mover) {
    if (!move.is_drop() || move.drop_piece != PieceType::Pawn) {
        return false;
    }

    const int attack_rank = static_cast<int>(move.to.rank)
        + (mover == Color::Black ? -1 : 1);
    if (attack_rank < 1 || attack_rank > 9) {
        return false;
    }

    const Square attacked{
        move.to.file,
        static_cast<std::uint8_t>(attack_rank),
    };
    const Piece& target = position.at(attacked);
    return !target.empty()
        && target.color == opposite(mover)
        && target.type == PieceType::King;
}

bool has_legal_evasion(const Position& checked_position) {
    const Color defender = checked_position.side_to_move();
    for (const Move& reply : generate_pseudo_legal_moves(checked_position)) {
        const Position next = checked_position.after_move(reply);
        if (!is_in_check(next, defender)) {
            return true;
        }
    }
    return false;
}

bool is_pawn_drop_mate(const Position& position, const Move& move, Color mover) {
    return dropped_pawn_attacks_king(position, move, mover)
        && !has_legal_evasion(position);
}

} // namespace

std::vector<Move> generate_legal_moves(const Position& position) {
    std::vector<Move> legal;
    const Color mover = position.side_to_move();
    for (const Move& move : generate_pseudo_legal_moves(position)) {
        const Position next = position.after_move(move);
        if (is_in_check(next, mover)) {
            continue;
        }
        if (is_pawn_drop_mate(next, move, mover)) {
            continue;
        }
        legal.push_back(move);
    }
    return legal;
}

} // namespace kadoka::shogi
