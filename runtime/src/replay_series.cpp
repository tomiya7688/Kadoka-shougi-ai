#include "kadoka/runtime/replay_series.hpp"

#include <chrono>
#include <utility>

namespace kadoka::shogi::runtime {
namespace {

struct ParticipantConfig {
    SearchLimits search{};
    std::optional<PlayerTimeControl> configured_time{};
    std::optional<std::chrono::milliseconds> remaining_main{};
};

[[nodiscard]] SeriesPlayer opposite_player(SeriesPlayer player) noexcept {
    return player == SeriesPlayer::A ? SeriesPlayer::B : SeriesPlayer::A;
}

[[nodiscard]] ParticipantConfig make_participant_config(
    const SearchLimits& search,
    const std::optional<PlayerTimeControl>& time_control) {
    ParticipantConfig config;
    config.search = search;
    config.configured_time = time_control;
    if (time_control.has_value()) {
        config.remaining_main = time_control->main_time;
    }
    return config;
}

[[nodiscard]] std::optional<PlayerTimeControl> time_control_for(
    const ParticipantConfig& participant,
    ReplayClockPolicy policy) {
    if (!participant.configured_time.has_value()) {
        return std::nullopt;
    }
    if (policy == ReplayClockPolicy::ResetConfigured) {
        return participant.configured_time;
    }

    PlayerTimeControl carried = *participant.configured_time;
    carried.main_time = participant.remaining_main.value_or(
        std::chrono::milliseconds::zero()
    );
    return carried;
}

void update_remaining_main(
    ParticipantConfig& participant,
    const std::optional<std::chrono::milliseconds>& remaining) {
    if (participant.configured_time.has_value() && remaining.has_value()) {
        participant.remaining_main = *remaining;
    }
}

[[nodiscard]] MatchLimits match_limits_for_game(
    const ReplaySeriesLimits& series,
    const ParticipantConfig& player_a,
    const ParticipantConfig& player_b,
    bool a_is_black) {
    MatchLimits result = series.match;

    const ParticipantConfig& black = a_is_black ? player_a : player_b;
    const ParticipantConfig& white = a_is_black ? player_b : player_a;

    result.black_search = black.search;
    result.white_search = white.search;
    result.black_time_control = time_control_for(black, series.clock_policy);
    result.white_time_control = time_control_for(white, series.clock_policy);
    return result;
}

[[nodiscard]] SeriesPlayer player_for_color(
    Color color,
    bool a_is_black) noexcept {
    if (color == Color::Black) {
        return a_is_black ? SeriesPlayer::A : SeriesPlayer::B;
    }
    return a_is_black ? SeriesPlayer::B : SeriesPlayer::A;
}

} // namespace

ReplaySeriesResult run_replay_series(
    Engine& player_a,
    Engine& player_b,
    const Position& initial_position,
    const ReplaySeriesLimits& limits) {
    ReplaySeriesResult result;

    ParticipantConfig a = make_participant_config(
        limits.match.black_search,
        limits.match.black_time_control
    );
    ParticipantConfig b = make_participant_config(
        limits.match.white_search,
        limits.match.white_time_control
    );

    bool a_is_black = true;
    Position game_initial = initial_position;

    for (std::uint32_t replay_count = 0;; ++replay_count) {
        Engine& black_engine = a_is_black ? player_a : player_b;
        Engine& white_engine = a_is_black ? player_b : player_a;
        const MatchLimits game_limits =
            match_limits_for_game(limits, a, b, a_is_black);

        MatchResult match = run_headless_match(
            black_engine,
            white_engine,
            game_initial,
            game_limits
        );

        if (limits.clock_policy == ReplayClockPolicy::CarryRemainingMain) {
            if (a_is_black) {
                update_remaining_main(a, match.clock.black_main_remaining);
                update_remaining_main(b, match.clock.white_main_remaining);
            } else {
                update_remaining_main(b, match.clock.black_main_remaining);
                update_remaining_main(a, match.clock.white_main_remaining);
            }
        }

        result.final_outcome = match.outcome;
        result.games.push_back(ReplayGameRecord{
            static_cast<std::uint32_t>(result.games.size() + 1),
            a_is_black ? SeriesPlayer::A : SeriesPlayer::B,
            a_is_black ? SeriesPlayer::B : SeriesPlayer::A,
            std::move(match),
        });

        switch (result.final_outcome.result) {
        case GameResult::BlackWin:
        case GameResult::WhiteWin: {
            const Color winning_color =
                result.final_outcome.result == GameResult::BlackWin
                    ? Color::Black
                    : Color::White;
            result.status = ReplaySeriesStatus::Decided;
            result.winner = player_for_color(winning_color, a_is_black);
            result.loser = opposite_player(*result.winner);
            return result;
        }
        case GameResult::Draw:
            result.status = ReplaySeriesStatus::Draw;
            return result;
        case GameResult::Unresolved:
            result.status = ReplaySeriesStatus::Unresolved;
            return result;
        case GameResult::ReplayRequired:
            if (replay_count >= limits.max_replays) {
                result.status = ReplaySeriesStatus::ReplayLimit;
                return result;
            }
            break;
        }

        a_is_black = !a_is_black;
        game_initial = Position::startpos();
    }
}

} // namespace kadoka::shogi::runtime
