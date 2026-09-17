#include "kadoka/runtime/headless_match.hpp"
#include "kadoka/runtime/turn_runner.hpp"

#include <cassert>
#include <string>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

class FixedActionEngine final : public Engine {
public:
    explicit FixedActionEngine(EngineAction action) : action_(action) {}

    [[nodiscard]] std::string name() const override {
        return "fixed-action-test-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits&
    ) override {
        ++calls_;
        SearchResult result;
        result.action = action_;
        result.info = "semantic-action-test";
        return result;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

private:
    EngineAction action_{EngineAction::Move};
    unsigned calls_{0};
};

} // namespace

int main() {
    {
        const Position initial = Position::startpos();
        const std::string before = initial.to_sfen();
        FixedActionEngine black{EngineAction::Resign};

        const TurnResult turn = run_engine_turn(black, initial);
        assert(turn.status == TurnStatus::Resigned);
        assert(turn.search_result.has_value());
        assert(turn.search_result->action == EngineAction::Resign);
        assert(!turn.next_position.has_value());
        assert(initial.to_sfen() == before);
        assert(black.calls() == 1);
    }

    {
        const Position initial = Position::startpos();
        FixedActionEngine black{EngineAction::Resign};
        FixedActionEngine white{EngineAction::Move};

        const MatchResult result = run_headless_match(black, white, initial);
        assert(result.outcome.result == GameResult::WhiteWin);
        assert(result.outcome.reason == GameEndReason::Resignation);
        assert(result.outcome.winner == Color::White);
        assert(result.outcome.loser == Color::Black);
        assert(result.accepted_moves.empty());
        assert(!result.stopped_side.has_value());
        assert(black.calls() == 1);
        assert(white.calls() == 0);
    }

    {
        const Position initial = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2R2BP 1"
        );
        FixedActionEngine black{EngineAction::DeclareEnteringKing};
        FixedActionEngine white{EngineAction::Move};

        const MatchResult result = run_headless_match(black, white, initial);
        assert(result.outcome.result == GameResult::BlackWin);
        assert(result.outcome.reason == GameEndReason::Impasse);
        assert(result.outcome.winner == Color::Black);
        assert(result.outcome.loser == Color::White);
        assert(result.accepted_moves.empty());
        assert(!result.stopped_side.has_value());
        assert(black.calls() == 1);
        assert(white.calls() == 0);
    }

    {
        const Position initial = Position::from_sfen(
            "9/PPPPPPPPP/G3K4/9/9/9/9/9/4k4 b 2RB 1"
        );
        FixedActionEngine black{EngineAction::DeclareEnteringKing};
        FixedActionEngine white{EngineAction::Move};

        const MatchResult result = run_headless_match(black, white, initial);
        assert(result.outcome.result == GameResult::ReplayRequired);
        assert(result.outcome.reason == GameEndReason::Impasse);
        assert(!result.outcome.winner.has_value());
        assert(!result.outcome.loser.has_value());
        assert(result.accepted_moves.empty());
        assert(black.calls() == 1);
        assert(white.calls() == 0);
    }

    {
        const Position initial = Position::startpos();
        FixedActionEngine black{EngineAction::DeclareEnteringKing};
        FixedActionEngine white{EngineAction::Move};

        const MatchResult result = run_headless_match(black, white, initial);
        assert(result.outcome.result == GameResult::WhiteWin);
        assert(result.outcome.reason == GameEndReason::Impasse);
        assert(result.outcome.winner == Color::White);
        assert(result.outcome.loser == Color::Black);
        assert(result.accepted_moves.empty());
        assert(black.calls() == 1);
        assert(white.calls() == 0);
    }

    return 0;
}
