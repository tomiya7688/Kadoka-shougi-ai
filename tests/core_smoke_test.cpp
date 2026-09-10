#include "kadoka/position.hpp"

#include <cassert>
#include <string>

int main() {
    using kadoka::shogi::Color;
    using kadoka::shogi::PieceType;
    using kadoka::shogi::Position;

    {
        const auto position = Position::startpos();
        const std::string expected =
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

        assert(position.side_to_move() == Color::Black);
        assert(position.ply() == 1);
        assert(position.to_sfen() == expected);
        assert(Position::from_sfen(expected).to_sfen() == expected);
    }

    {
        const std::string sfen = "9/9/9/9/4+P4/9/9/9/9 w 2RBG3S2n4p 42";
        const Position position = Position::from_sfen(sfen);

        assert(position.side_to_move() == Color::White);
        assert(position.ply() == 42);
        assert(position.hand_count(Color::Black, PieceType::Rook) == 2);
        assert(position.hand_count(Color::Black, PieceType::Bishop) == 1);
        assert(position.hand_count(Color::Black, PieceType::Gold) == 1);
        assert(position.hand_count(Color::Black, PieceType::Silver) == 3);
        assert(position.hand_count(Color::White, PieceType::Knight) == 2);
        assert(position.hand_count(Color::White, PieceType::Pawn) == 4);
        assert(position.to_sfen() == sfen);
    }

    return 0;
}
