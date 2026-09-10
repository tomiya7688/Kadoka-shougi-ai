#include "kadoka/movegen.hpp"
#include "kadoka/attack.hpp"

namespace kadoka::shogi {

std::vector<Move> generate_legal_moves(const Position& position) {
    std::vector<Move> legal;
    const Color mover = position.side_to_move();
    for (const Move& move : generate_pseudo_legal_moves(position)) {
        const Position next = position.after_move(move);
        if (!is_in_check(next, mover)) {
            legal.push_back(move);
        }
    }
    return legal;
}

} // namespace kadoka::shogi
