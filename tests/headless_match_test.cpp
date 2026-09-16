#include "kadoka/runtime/headless_match.hpp"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

class SequenceEngine final : public Engine {
public:
    explicit SequenceEngine(std::vector<Move> moves) : moves_(std::move(moves)) {}

    [[nodiscard]] std::string name() const override {
        return "sequence-test-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits&
    ) override {
        ++calls_;
        assert(!moves_.empty());

        const std::size_t selected = index_ < moves_.size() ? index_ : moves_.size() - 1;
        if (index_ < moves_.size()) {
            ++index_;
        }

        SearchResult result;
        result.best_move = moves_[selected];
        result.info = "headless-match-test";
        return result;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

private:
    std::vector<Move> moves_{};
    std::size_t index_{0};
    unsigned calls_{0};
};

} // namespace

int main() {
    {
        const Position initial = Position::startpos();
        SequenceEngine black({
            Move{Square{7, 7}, Square{7, 5}, PieceType::None, false},
            Move{Square{7, 7}, Square{7, 6}, PieceType::None, false},
        });
        SequenceEngine white({
            Move{Square{3, 3}, Square{3, 4}, PieceType::None, false},
        });

        MatchLimits limits;
        limits.max_engine_attempts_per_turn = 3;
        limits.max_plies = 2;

        const MatchResult result = run_headless_match(black, white, initial, limits);

        assert(result.end_reason == MatchEndReason::PlyLimit);
        assert(!result.stopped_side.has_value());
        assert(!result.losing_side.has_value());
        assert(result.accepted_moves.size() == 2);
        assert(result.black_illegal_outputs == 1);
        assert(result.white_illegal_outputs == 0);
        assert(black.calls() == 2);
        assert(white.calls() == 1);
        assert(result.final_position.side_to_move() == Color::Black);
        assert(result.final_position.at(Square{7, 6}).type == PieceType::Pawn);
        assert(result.final_position.at(Square{3, 4}).type == PieceType::Pawn);
    }

    {
        const Position initial = Position::startpos();
        const std::string before = initial.to_sfen();
        SequenceEngine black({
            Move{Square{7, 7}, Square{7, 5}, PieceType::None, false},
        });
        SequenceEngine white({
            Move{Square{3, 3}, Square{3, 4}, PieceType::None, false},
        });

        MatchLimits limits;
        limits.max_engine_attempts_per_turn = 2;
        limits.max_plies = 10;

        const MatchResult result = run_headless_match(black, white, initial, limits);

        assert(result.end_reason == MatchEndReason::EngineAttemptLimit);
        assert(result.stopped_side == Color::Black);
        assert(!result.losing_side.has_value());
        assert(result.accepted_moves.empty());
        assert(result.black_illegal_outputs == 2);
        assert(result.white_illegal_outputs == 0);
        assert(black.calls() == 2);
        assert(white.calls() == 0);
        assert(result.final_position.to_sfen() == before);
    }

    {
        const Position empty = Position::from_sfen("9/9/9/9/9/9/9/9/9 b - 1");
        SequenceEngine black({
            Move{Square{1, 1}, Square{1, 2}, PieceType::None, false},
        });
        SequenceEngine white({
            Move{Square{1, 1}, Square{1, 2}, PieceType::None, false},
        });

        const MatchResult result = run_headless_match(black, white, empty);

        assert(result.end_reason == MatchEndReason::NoLegalMoves);
        assert(result.stopped_side == Color::Black);
        assert(!result.losing_side.has_value());
        assert(result.accepted_moves.empty());
        assert(result.black_illegal_outputs == 0);
        assert(result.white_illegal_outputs == 0);
        assert(black.calls() == 0);
        assert(white.calls() == 0);
    }

    {
        const Position initial = Position::from_sfen("4k4/9/9/9/9/9/9/9/4K4 b - 1");
        SequenceEngine black({
            Move{Square{5, 9}, Square{4, 9}, PieceType::None, false},
            Move{Square{4, 9}, Square{5, 9}, PieceType::None, false},
            Move{Square{5, 9}, Square{4, 9}, PieceType::None, false},
            Move{Square{4, 9}, Square{5, 9}, PieceType::None, false},
            Move{Square{5, 9}, Square{4, 9}, PieceType::None, false},
            Move{Square{4, 9}, Square{5, 9}, PieceType::None, false},
        });
        SequenceEngine white({
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false},
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false},
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false},
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false},
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false},
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false},
        });

        MatchLimits limits;
        limits.max_plies = 64;

        const MatchResult result = run_headless_match(black, white, initial, limits);
        assert(result.end_reason == MatchEndReason::RepetitionDraw);
        assert(!result.stopped_side.has_value());
        assert(!result.losing_side.has_value());
        assert(result.accepted_moves.size() == 12);
    }

    {
        const Position initial = Position::from_sfen("4k4/5R3/9/9/9/9/9/9/K8 b - 1");
        SequenceEngine black({
            Move{Square{4, 2}, Square{5, 2}, PieceType::None, false},
            Move{Square{5, 2}, Square{4, 2}, PieceType::None, false},
            Move{Square{4, 2}, Square{5, 2}, PieceType::None, false},
            Move{Square{5, 2}, Square{4, 2}, PieceType::None, false},
            Move{Square{4, 2}, Square{5, 2}, PieceType::None, false},
            Move{Square{5, 2}, Square{4, 2}, PieceType::None, false},
        });
        SequenceEngine white({
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false},
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false},
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false},
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false},
            Move{Square{5, 1}, Square{4, 1}, PieceType::None, false},
            Move{Square{4, 1}, Square{5, 1}, PieceType::None, false},
        });

        MatchLimits limits;
        limits.max_plies = 64;

        const MatchResult result = run_headless_match(black, white, initial, limits);
        assert(result.end_reason == MatchEndReason::PerpetualCheckLoss);
        assert(!result.stopped_side.has_value());
        assert(result.losing_side == Color::Black);
        assert(result.accepted_moves.size() == 12);
    }

    return 0;
}
