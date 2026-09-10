#include "kadoka/attack.hpp"
#include "kadoka/movegen.hpp"
#include "kadoka/position.hpp"

#include <algorithm>
#include <cassert>

using namespace kadoka::shogi;

namespace {

bool has_move(const std::vector<Move>& moves, Square from, Square to) {
    return std::any_of(moves.begin(), moves.end(), [&](const Move& move) {
        return move.from.has_value() && *move.from == from && move.to == to;
    });
}

} // namespace

int main() {
    {
        const Position start = Position::startpos();
        assert(!is_in_check(start, Color::Black));
        assert(!is_in_check(start, Color::White));
        assert(generate_legal_moves(start).size() == 30);
    }

    {
        const Position checked = Position::from_sfen("4r3k/9/9/9/9/9/9/9/4K4 b - 1");
        assert(is_in_check(checked, Color::Black));

        const auto pseudo = generate_pseudo_legal_moves(checked);
        const auto legal = generate_legal_moves(checked);
        assert(has_move(pseudo, Square{5, 9}, Square{5, 8}));
        assert(!has_move(legal, Square{5, 9}, Square{5, 8}));
        assert(has_move(legal, Square{5, 9}, Square{4, 8}));
    }

    {
        const Position position = Position::from_sfen("4k4/9/9/9/4p4/4P4/9/9/4K4 b - 1");
        const Move capture{Square{5, 6}, Square{5, 5}, PieceType::None, false};
        const Position next = position.after_move(capture);
        assert(next.hand_count(Color::Black, PieceType::Pawn) == 1);
        assert(next.side_to_move() == Color::White);
        assert(next.ply() == 2);
    }

    return 0;
}
