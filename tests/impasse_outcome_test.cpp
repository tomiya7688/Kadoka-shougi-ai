#include "kadoka/impasse.hpp"
#include "kadoka/runtime/impasse_outcome.hpp"

#include <cassert>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

int main() {
    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2R2BP 1"
        );
        const auto declaration = adjudicate_entering_king_declaration(position, Color::Black);
        const GameOutcome outcome = entering_king_declaration_outcome(declaration, Color::Black);
        assert(outcome.result == GameResult::BlackWin);
        assert(outcome.reason == GameEndReason::Impasse);
        assert(outcome.winner == Color::Black);
        assert(outcome.loser == Color::White);
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2RB 1"
        );
        const auto declaration = adjudicate_entering_king_declaration(position, Color::Black);
        const GameOutcome outcome = entering_king_declaration_outcome(declaration, Color::Black);
        assert(outcome.result == GameResult::ReplayRequired);
        assert(outcome.reason == GameEndReason::Impasse);
        assert(!outcome.winner.has_value());
        assert(!outcome.loser.has_value());
    }

    {
        const Position position = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b RB 1"
        );
        const auto declaration = adjudicate_entering_king_declaration(position, Color::Black);
        const GameOutcome outcome = entering_king_declaration_outcome(declaration, Color::Black);
        assert(outcome.result == GameResult::WhiteWin);
        assert(outcome.reason == GameEndReason::Impasse);
        assert(outcome.winner == Color::White);
        assert(outcome.loser == Color::Black);
    }

    {
        const Position position = Position::from_sfen(
            "4k4/9/9/9/9/9/9/9/4K4 b 2R2B3P2r2b4p 1"
        );
        const auto impasse = adjudicate_mutual_impasse_points(position);
        const GameOutcome outcome = mutual_impasse_outcome(impasse);
        assert(outcome.result == GameResult::WhiteWin);
        assert(outcome.reason == GameEndReason::Impasse);
        assert(outcome.winner == Color::White);
        assert(outcome.loser == Color::Black);
    }

    return 0;
}
