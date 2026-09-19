#pragma once

#include "kadoka/runtime/headless_match.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace kadoka::shogi::runtime {

enum class SeriesPlayer : std::uint8_t {
    A,
    B,
};

enum class ReplayClockPolicy : std::uint8_t {
    // Each replay gives both participants their originally configured main
    // time and byoyomi again.
    ResetConfigured,

    // Remaining main time follows each participant across the color swap.
    // Byoyomi remains the participant's configured per-move allowance.
    CarryRemainingMain,
};

enum class ReplaySeriesStatus : std::uint8_t {
    Decided,
    Draw,
    Unresolved,
    ReplayLimit,
};

struct ReplaySeriesLimits {
    MatchLimits match{};
    std::uint32_t max_replays{16};
    ReplayClockPolicy clock_policy{ReplayClockPolicy::ResetConfigured};
};

struct ReplayGameRecord {
    std::uint32_t game_number{1};
    SeriesPlayer black_player{SeriesPlayer::A};
    SeriesPlayer white_player{SeriesPlayer::B};
    MatchResult match{};
};

struct ReplaySeriesResult {
    ReplaySeriesStatus status{ReplaySeriesStatus::Unresolved};
    std::optional<SeriesPlayer> winner{};
    std::optional<SeriesPlayer> loser{};
    GameOutcome final_outcome{};
    std::vector<ReplayGameRecord> games{};
};

// Runs one logical shogi contest through any number of no-result replays.
// Player A is Black in the first game. Every ReplayRequired result restarts
// from the standard initial position with the players' colors swapped.
[[nodiscard]] ReplaySeriesResult run_replay_series(
    Engine& player_a,
    Engine& player_b,
    const Position& initial_position,
    const ReplaySeriesLimits& limits = {}
);

} // namespace kadoka::shogi::runtime
