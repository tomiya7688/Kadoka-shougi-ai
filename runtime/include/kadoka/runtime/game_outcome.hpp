#pragma once

#include "kadoka/types.hpp"

#include <cstdint>
#include <optional>

namespace kadoka::shogi::runtime {

// Result and reason are intentionally separate. League/dataset tooling can
// score the result while still preserving why the match ended.
enum class GameResult : std::uint8_t {
    BlackWin,
    WhiteWin,
    Draw,
    ReplayRequired,
    Unresolved,
};

enum class GameEndReason : std::uint8_t {
    Checkmate,
    Repetition,
    PerpetualCheckViolation,
    Resignation,
    TimeForfeit,
    Impasse,
    NoLegalMoves,
    EngineAttemptLimit,
    PlyLimit,
};

struct GameOutcome {
    GameResult result{GameResult::Unresolved};
    GameEndReason reason{GameEndReason::PlyLimit};
    std::optional<Color> winner{};
    std::optional<Color> loser{};
};

[[nodiscard]] constexpr GameResult win_result(Color winner) noexcept {
    return winner == Color::Black ? GameResult::BlackWin : GameResult::WhiteWin;
}

[[nodiscard]] constexpr GameOutcome make_win_outcome(Color winner, GameEndReason reason) noexcept {
    return GameOutcome{
        win_result(winner),
        reason,
        winner,
        opposite(winner),
    };
}

[[nodiscard]] constexpr GameOutcome make_replay_outcome(GameEndReason reason) noexcept {
    return GameOutcome{
        GameResult::ReplayRequired,
        reason,
        std::nullopt,
        std::nullopt,
    };
}

[[nodiscard]] constexpr GameOutcome make_unresolved_outcome(GameEndReason reason) noexcept {
    return GameOutcome{
        GameResult::Unresolved,
        reason,
        std::nullopt,
        std::nullopt,
    };
}

} // namespace kadoka::shogi::runtime
