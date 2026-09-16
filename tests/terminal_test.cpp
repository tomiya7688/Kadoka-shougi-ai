#include "kadoka/terminal.hpp"

#include <cassert>

using namespace kadoka::shogi;

int main() {
    {
        const TerminalPositionResult result = adjudicate_terminal_position(Position::startpos());
        assert(result.status == TerminalPositionStatus::Ongoing);
        assert(!result.winner.has_value());
        assert(!result.loser.has_value());
    }

    {
        const Position mate = Position::from_sfen("4k4/9/9/9/9/9/9/3grg3/4K4 b - 1");
        const TerminalPositionResult result = adjudicate_terminal_position(mate);
        assert(result.status == TerminalPositionStatus::Checkmate);
        assert(result.winner == Color::White);
        assert(result.loser == Color::Black);
    }

    {
        const Position empty = Position::from_sfen("9/9/9/9/9/9/9/9/9 b - 1");
        const TerminalPositionResult result = adjudicate_terminal_position(empty);
        assert(result.status == TerminalPositionStatus::NoLegalMoves);
        assert(!result.winner.has_value());
        assert(!result.loser.has_value());
    }

    return 0;
}
