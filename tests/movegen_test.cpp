#include "kadoka/movegen.hpp"

#include <cassert>
#include <cstddef>

using kadoka::shogi::Move;
using kadoka::shogi::PieceType;
using kadoka::shogi::Position;
using kadoka::shogi::Square;
using kadoka::shogi::generate_pseudo_legal_moves;

namespace {

std::size_t count_moves_to(const std::vector<Move>& moves, Square to, bool promote) {
    std::size_t count = 0;
    for (const Move& move : moves) {
        if (!move.is_drop() && move.to == to && move.promote == promote) {
            ++count;
        }
    }
    return count;
}

bool has_pawn_drop_on_file(const std::vector<Move>& moves, std::uint8_t file) {
    for (const Move& move : moves) {
        if (move.is_drop() && move.drop_piece == PieceType::Pawn && move.to.file == file) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    {
        const Position position = Position::startpos();
        const auto moves = generate_pseudo_legal_moves(position);
        assert(moves.size() == 30);
    }

    {
        const Position position = Position::from_sfen("9/9/9/4P4/9/9/9/9/9 b - 1");
        const auto moves = generate_pseudo_legal_moves(position);
        assert(moves.size() == 2);
        assert(count_moves_to(moves, Square{5, 3}, false) == 1);
        assert(count_moves_to(moves, Square{5, 3}, true) == 1);
    }

    {
        const Position position = Position::from_sfen("9/4P4/9/9/9/9/9/9/9 b - 1");
        const auto moves = generate_pseudo_legal_moves(position);
        assert(moves.size() == 1);
        assert(count_moves_to(moves, Square{5, 1}, true) == 1);
    }

    {
        const Position position = Position::from_sfen("9/9/9/9/4P4/9/9/9/9 b P 1");
        const auto moves = generate_pseudo_legal_moves(position);
        assert(!has_pawn_drop_on_file(moves, 5));
    }

    {
        const Position position = Position::from_sfen("9/9/9/9/9/9/9/9/9 b P 1");
        const auto moves = generate_pseudo_legal_moves(position);
        assert(!has_pawn_drop_on_file(moves, 0));
        assert(moves.size() == 72);
        for (const Move& move : moves) {
            assert(move.is_drop());
            assert(move.drop_piece == PieceType::Pawn);
            assert(move.to.rank != 1);
        }
    }

    return 0;
}
