#include "kadoka/runtime/headless_match.hpp"

#include <cassert>
#include <chrono>
#include <optional>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

class TimedMoveEngine final : public Engine {
public:
    TimedMoveEngine(Move move, std::chrono::milliseconds delay)
        : move_(move), delay_(delay) {}

    [[nodiscard]] std::string name() const override {
        return "timed-move-test-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits& limits
    ) override {
        ++calls_;
        seen_time_limit_ = limits.time_limit;
        std::this_thread::sleep_for(delay_);

        SearchResult result;
        result.best_move = move_;
        return result;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

    [[nodiscard]] std::optional<std::chrono::milliseconds> seen_time_limit() const {
        return seen_time_limit_;
    }

private:
    Move move_{};
    std::chrono::milliseconds delay_{0};
    unsigned calls_{0};
    std::optional<std::chrono::milliseconds> seen_time_limit_{};
};

} // namespace

int main() {
    {
        const Position initial = Position::startpos();
        TimedMoveEngine black{
            Move{Square{7, 7}, Square{7, 6}, PieceType::None, false},
            10ms,
        };
        TimedMoveEngine white{
            Move{Square{3, 3}, Square{3, 4}, PieceType::None, false},
            0ms,
        };

        MatchLimits limits;
        limits.black_time_control = PlayerTimeControl{1ms, 0ms};
        limits.white_time_control = PlayerTimeControl{1s, 0ms};
        limits.max_plies = 4;

        const MatchResult result = run_headless_match(black, white, initial, limits);

        assert(result.outcome.result == GameResult::WhiteWin);
        assert(result.outcome.reason == GameEndReason::TimeForfeit);
        assert(result.outcome.winner == Color::White);
        assert(result.outcome.loser == Color::Black);
        assert(result.accepted_moves.empty());
        assert(!result.stopped_side.has_value());
        assert(black.calls() == 1);
        assert(white.calls() == 0);
        assert(black.seen_time_limit() == 1ms);
        assert(result.clock.black_main_remaining == 0ms);
        assert(result.clock.black_elapsed >= 1ms);
    }

    {
        const Position initial = Position::startpos();
        TimedMoveEngine black{
            Move{Square{7, 7}, Square{7, 6}, PieceType::None, false},
            0ms,
        };
        TimedMoveEngine white{
            Move{Square{3, 3}, Square{3, 4}, PieceType::None, false},
            0ms,
        };

        MatchLimits limits;
        limits.black_time_control = PlayerTimeControl{0ms, 100ms};
        limits.max_plies = 1;

        const MatchResult result = run_headless_match(black, white, initial, limits);

        assert(result.outcome.result == GameResult::Unresolved);
        assert(result.outcome.reason == GameEndReason::PlyLimit);
        assert(result.accepted_moves.size() == 1);
        assert(black.calls() == 1);
        assert(black.seen_time_limit().has_value());
        assert(*black.seen_time_limit() <= 100ms);
        assert(*black.seen_time_limit() > 0ms);
        assert(result.clock.black_main_remaining == 0ms);
    }

    return 0;
}
