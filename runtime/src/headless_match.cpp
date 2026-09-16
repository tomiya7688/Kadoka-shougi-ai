#include "kadoka/runtime/headless_match.hpp"

#include "kadoka/repetition.hpp"

#include <cstddef>
#include <vector>

namespace kadoka::shogi::runtime {
namespace {

Engine& engine_for(Color side, Engine& black_engine, Engine& white_engine) {
    return side == Color::Black ? black_engine : white_engine;
}

const SearchLimits& search_limits_for(Color side, const MatchLimits& limits) {
    return side == Color::Black ? limits.black_search : limits.white_search;
}

std::uint32_t& illegal_counter_for(Color side, MatchResult& result) {
    return side == Color::Black ? result.black_illegal_outputs : result.white_illegal_outputs;
}

std::optional<Color> repetition_loser(RepetitionStatus status) {
    if (status == RepetitionStatus::BlackLosesPerpetualCheck) {
        return Color::Black;
    }
    if (status == RepetitionStatus::WhiteLosesPerpetualCheck) {
        return Color::White;
    }
    return std::nullopt;
}

} // namespace

MatchResult run_headless_match(
    Engine& black_engine,
    Engine& white_engine,
    const Position& initial_position,
    const MatchLimits& limits) {
    MatchResult result;
    result.final_position = initial_position;

    std::vector<Position> history;
    history.reserve(static_cast<std::size_t>(limits.max_plies) + 1);
    history.push_back(initial_position);

    while (result.accepted_moves.size() < static_cast<std::size_t>(limits.max_plies)) {
        const Color side = result.final_position.side_to_move();
        Engine& engine = engine_for(side, black_engine, white_engine);
        const SearchLimits& search_limits = search_limits_for(side, limits);

        bool move_applied = false;
        for (std::uint32_t attempt = 0; attempt < limits.max_engine_attempts_per_turn; ++attempt) {
            const TurnResult turn = run_engine_turn(engine, result.final_position, search_limits);

            if (turn.status == TurnStatus::NoLegalMoves) {
                result.end_reason = MatchEndReason::NoLegalMoves;
                result.stopped_side = side;
                return result;
            }

            if (turn.status == TurnStatus::IllegalMove) {
                ++illegal_counter_for(side, result);
                continue;
            }

            result.accepted_moves.push_back(turn.search_result->best_move);
            result.final_position = *turn.next_position;
            history.push_back(result.final_position);
            move_applied = true;
            break;
        }

        if (!move_applied) {
            result.end_reason = MatchEndReason::EngineAttemptLimit;
            result.stopped_side = side;
            return result;
        }

        const RepetitionResult repetition = adjudicate_repetition(history);
        if (repetition.status == RepetitionStatus::Draw) {
            result.end_reason = MatchEndReason::RepetitionDraw;
            result.stopped_side.reset();
            result.losing_side.reset();
            return result;
        }

        if (const std::optional<Color> loser = repetition_loser(repetition.status); loser.has_value()) {
            result.end_reason = MatchEndReason::PerpetualCheckLoss;
            result.stopped_side.reset();
            result.losing_side = loser;
            return result;
        }
    }

    result.end_reason = MatchEndReason::PlyLimit;
    result.stopped_side.reset();
    result.losing_side.reset();
    return result;
}

} // namespace kadoka::shogi::runtime
