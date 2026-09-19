#include "kadoka/runtime/replay_series.hpp"

#include <cassert>
#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;
using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

const Position mutual_impasse_position() {
    return Position::from_sfen(
        "4K4/9/9/9/9/9/9/9/4k4 b "
        "RB2G2S2N2L9Prb2g2s2n2l9p 1"
    );
}

class OfferEngine final : public Engine {
public:
    explicit OfferEngine(std::chrono::milliseconds delay = 0ms)
        : delay_(delay) {}

    [[nodiscard]] std::string name() const override {
        return "offer-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits& limits
    ) override {
        ++calls_;
        seen_limits_.push_back(limits.time_limit);
        if (calls_ == 1) {
            std::this_thread::sleep_for(delay_);
            SearchResult result;
            result.action = EngineAction::OfferMutualImpasse;
            return result;
        }

        SearchResult result;
        result.action = EngineAction::Resign;
        return result;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

    [[nodiscard]] const std::vector<std::optional<std::chrono::milliseconds>>&
    seen_limits() const noexcept {
        return seen_limits_;
    }

private:
    std::chrono::milliseconds delay_{0};
    unsigned calls_{0};
    std::vector<std::optional<std::chrono::milliseconds>> seen_limits_{};
};

class AcceptThenMoveEngine final : public Engine {
public:
    explicit AcceptThenMoveEngine(bool resign_in_replay = false)
        : resign_in_replay_(resign_in_replay) {}

    [[nodiscard]] std::string name() const override {
        return "accept-then-move-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits& limits
    ) override {
        ++calls_;
        seen_limits_.push_back(limits.time_limit);
        SearchResult result;
        if (resign_in_replay_) {
            result.action = EngineAction::Resign;
        } else {
            result.best_move = Move{
                Square{7, 7},
                Square{7, 6},
                PieceType::None,
                false,
            };
        }
        return result;
    }

    [[nodiscard]] MutualImpasseResponse respond_to_mutual_impasse_offer(
        const Position&
    ) override {
        ++response_calls_;
        return MutualImpasseResponse::Accept;
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

    [[nodiscard]] unsigned response_calls() const noexcept {
        return response_calls_;
    }

    [[nodiscard]] const std::vector<std::optional<std::chrono::milliseconds>>&
    seen_limits() const noexcept {
        return seen_limits_;
    }

private:
    bool resign_in_replay_{false};
    unsigned calls_{0};
    unsigned response_calls_{0};
    std::vector<std::optional<std::chrono::milliseconds>> seen_limits_{};
};

class NeverCalledEngine final : public Engine {
public:
    [[nodiscard]] std::string name() const override {
        return "never-called-engine";
    }

    [[nodiscard]] SearchResult search(
        const Position&,
        const SearchLimits&
    ) override {
        ++calls_;
        return {};
    }

    [[nodiscard]] unsigned calls() const noexcept {
        return calls_;
    }

private:
    unsigned calls_{0};
};

} // namespace

int main() {
    {
        OfferEngine a;
        AcceptThenMoveEngine b{true};

        ReplaySeriesLimits limits;
        limits.max_replays = 4;

        const ReplaySeriesResult result = run_replay_series(
            a,
            b,
            mutual_impasse_position(),
            limits
        );

        assert(result.status == ReplaySeriesStatus::Decided);
        assert(result.winner == SeriesPlayer::A);
        assert(result.loser == SeriesPlayer::B);
        assert(result.games.size() == 2);

        assert(result.games[0].game_number == 1);
        assert(result.games[0].black_player == SeriesPlayer::A);
        assert(result.games[0].white_player == SeriesPlayer::B);
        assert(
            result.games[0].match.outcome.result
            == GameResult::ReplayRequired
        );

        assert(result.games[1].game_number == 2);
        assert(result.games[1].black_player == SeriesPlayer::B);
        assert(result.games[1].white_player == SeriesPlayer::A);
        assert(result.games[1].match.final_position.to_sfen()
               == Position::startpos().to_sfen());
        assert(result.games[1].match.outcome.result == GameResult::WhiteWin);
        assert(result.final_outcome.result == GameResult::WhiteWin);

        assert(a.calls() == 1);
        assert(b.response_calls() == 1);
        assert(b.calls() == 1);
    }

    {
        OfferEngine a;
        AcceptThenMoveEngine b;

        ReplaySeriesLimits limits;
        limits.max_replays = 0;

        const ReplaySeriesResult result = run_replay_series(
            a,
            b,
            mutual_impasse_position(),
            limits
        );

        assert(result.status == ReplaySeriesStatus::ReplayLimit);
        assert(!result.winner.has_value());
        assert(!result.loser.has_value());
        assert(result.games.size() == 1);
        assert(result.final_outcome.result == GameResult::ReplayRequired);
        assert(b.calls() == 0);
    }

    {
        NeverCalledEngine a;
        NeverCalledEngine b;

        ReplaySeriesLimits limits;
        limits.match.max_plies = 0;

        const ReplaySeriesResult result = run_replay_series(
            a,
            b,
            Position::startpos(),
            limits
        );

        assert(result.status == ReplaySeriesStatus::Unresolved);
        assert(result.games.size() == 1);
        assert(result.final_outcome.result == GameResult::Unresolved);
        assert(result.final_outcome.reason == GameEndReason::PlyLimit);
        assert(a.calls() == 0);
        assert(b.calls() == 0);
    }

    {
        OfferEngine a;
        AcceptThenMoveEngine b;

        ReplaySeriesLimits limits;
        limits.max_replays = 1;
        limits.match.black_search.time_limit = 111ms;
        limits.match.white_search.time_limit = 222ms;

        const ReplaySeriesResult result = run_replay_series(
            a,
            b,
            mutual_impasse_position(),
            limits
        );

        assert(result.games.size() == 2);
        assert(!b.seen_limits().empty());
        assert(b.seen_limits().front() == 222ms);
        assert(a.seen_limits().size() == 2);
        assert(a.seen_limits()[1] == 111ms);
    }

    {
        OfferEngine a{20ms};
        AcceptThenMoveEngine b;

        ReplaySeriesLimits limits;
        limits.max_replays = 1;
        limits.clock_policy = ReplayClockPolicy::CarryRemainingMain;
        limits.match.black_time_control = PlayerTimeControl{200ms, 0ms};
        limits.match.white_time_control = PlayerTimeControl{300ms, 0ms};

        const ReplaySeriesResult result = run_replay_series(
            a,
            b,
            mutual_impasse_position(),
            limits
        );

        assert(result.games.size() == 2);
        const auto a_remaining_after_first =
            result.games[0].match.clock.black_main_remaining;
        assert(a_remaining_after_first.has_value());
        assert(*a_remaining_after_first < 200ms);

        assert(a.seen_limits().size() == 2);
        assert(a.seen_limits()[1] == a_remaining_after_first);

        assert(!b.seen_limits().empty());
        assert(b.seen_limits().front() == 300ms);
    }

    {
        OfferEngine a{20ms};
        AcceptThenMoveEngine b;

        ReplaySeriesLimits limits;
        limits.max_replays = 1;
        limits.clock_policy = ReplayClockPolicy::ResetConfigured;
        limits.match.black_time_control = PlayerTimeControl{200ms, 0ms};
        limits.match.white_time_control = PlayerTimeControl{300ms, 0ms};

        const ReplaySeriesResult result = run_replay_series(
            a,
            b,
            mutual_impasse_position(),
            limits
        );

        assert(result.games.size() == 2);
        assert(a.seen_limits().size() == 2);
        assert(a.seen_limits()[1] == 200ms);
        assert(!b.seen_limits().empty());
        assert(b.seen_limits().front() == 300ms);
    }

    return 0;
}
