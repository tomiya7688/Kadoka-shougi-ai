#include "kadoka/runtime/headless_match.hpp"

#include "kadoka/impasse.hpp"
#include "kadoka/runtime/impasse_outcome.hpp"
#include "kadoka/repetition.hpp"
#include "kadoka/terminal.hpp"

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

bool apply_automatic_impasse_if_ready(
    const std::vector<Position>& history,
    const MatchLimits& limits,
    MatchResult& result) {
    if (limits.automatic_impasse_rule == AutomaticImpasseRule::Disabled) {
        return false;
    }

    if (adjudicate_500_move_impasse(history) != Move500ImpasseStatus::Replay) {
        return false;
    }

    result.outcome = make_replay_outcome(GameEndReason::Impasse);
    result.stopped_side.reset();
    return true;
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

    if (apply_automatic_impasse_if_ready(history, limits, result)) {
        return result;
    }

    while (result.accepted_moves.size() < static_cast<std::size_t>(limits.max_plies)) {
        const Color side = result.final_position.side_to_move();
        Engine& engine = engine_for(side, black_engine, white_engine);
        const SearchLimits& search_limits = search_limits_for(side, limits);

        bool move_applied = false;
        for (std::uint32_t attempt = 0; attempt < limits.max_engine_attempts_per_turn; ++attempt) {
            const TurnResult turn = run_engine_turn(engine, result.final_position, search_limits);

            if (turn.status == TurnStatus::NoLegalMoves) {
                const TerminalPositionResult terminal = adjudicate_terminal_position(result.final_position);
                if (terminal.status == TerminalPositionStatus::Checkmate) {
                    result.outcome = make_win_outcome(*terminal.winner, GameEndReason::Checkmate);
                    result.stopped_side.reset();
                } else {
                    result.outcome = make_unresolved_outcome(GameEndReason::NoLegalMoves);
                    result.stopped_side = side;
                }
                return result;
            }

            if (turn.status == TurnStatus::Resigned) {
                result.outcome = make_win_outcome(opposite(side), GameEndReason::Resignation);
                result.stopped_side.reset();
                return result;
            }

            if (turn.status == TurnStatus::EnteringKingDeclaration) {
                const EnteringKingDeclarationResult declaration =
                    adjudicate_entering_king_declaration(result.final_position, side);
                result.outcome = entering_king_declaration_outcome(declaration, side);
                result.stopped_side.reset();
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
            result.outcome = make_unresolved_outcome(GameEndReason::EngineAttemptLimit);
            result.stopped_side = side;
            return result;
        }

        const RepetitionResult repetition = adjudicate_repetition(history);
        if (repetition.status == RepetitionStatus::Draw) {
            result.outcome = make_replay_outcome(GameEndReason::Repetition);
            result.stopped_side.reset();
            return result;
        }

        if (const std::optional<Color> loser = repetition_loser(repetition.status); loser.has_value()) {
            result.outcome = make_win_outcome(opposite(*loser), GameEndReason::PerpetualCheckViolation);
            result.stopped_side.reset();
            return result;
        }

        if (apply_automatic_impasse_if_ready(history, limits, result)) {
            return result;
        }
    }

    result.outcome = make_unresolved_outcome(GameEndReason::PlyLimit);
    result.stopped_side.reset();
    return result;
}

} // namespace kadoka::shogi::runtime
