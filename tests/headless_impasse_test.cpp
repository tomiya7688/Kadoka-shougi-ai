#include "kadoka/runtime/headless_match.hpp"

#include <cassert>
#include <string>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

class FixedMoveEngine final : public Engine {
public:
    explicit FixedMoveEngine(Move move) : move_(move) {}

    [[nodiscard]] std::string name() const override {
        return "fixed-impasse-test-engine";
    }

    [[nodiscard]] SearchResult search(const Position&, const SearchLimits&) override {
        ++calls_;
        SearchResult result;
        result.best_move = move_;
        return result;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

private:
    Move move_{};
    unsigned calls_{0};
};

} // namespace

int main() {
    {
        const Position initial = Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b - 500");
        FixedMoveEngine black{Move{Square{5, 9}, Square{4, 9}, PieceType::None, false}};
        FixedMoveEngine white{Move{Square{5, 1}, Square{4, 1}, PieceType::None, false}};

        MatchLimits limits;
        limits.max_plies = 16;
        const MatchResult result = run_headless_match(black, white, initial, limits);

        assert(result.outcome.result == GameResult::ReplayRequired);
        assert(result.outcome.reason == GameEndReason::Impasse);
        assert(!result.outcome.winner.has_value());
        assert(!result.outcome.loser.has_value());
        assert(result.accepted_moves.size() == 1);
        assert(black.calls() == 1);
        assert(white.calls() == 0);
        assert(result.final_position.ply() == 501);
    }

    {
        const Position initial = Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b - 500");
        FixedMoveEngine black{Move{Square{5, 9}, Square{4, 9}, PieceType::None, false}};
        FixedMoveEngine white{Move{Square{5, 1}, Square{4, 1}, PieceType::None, false}};

        MatchLimits limits;
        limits.max_plies = 1;
        limits.automatic_impasse_rule = AutomaticImpasseRule::Disabled;
        const MatchResult result = run_headless_match(black, white, initial, limits);

        assert(result.outcome.result == GameResult::Unresolved);
        assert(result.outcome.reason == GameEndReason::PlyLimit);
        assert(result.accepted_moves.size() == 1);
        assert(black.calls() == 1);
        assert(white.calls() == 0);
        assert(result.final_position.ply() == 501);
    }

    return 0;
}
