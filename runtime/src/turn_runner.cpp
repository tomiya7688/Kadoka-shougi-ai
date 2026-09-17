#include "kadoka/runtime/turn_runner.hpp"

#include "kadoka/movegen.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace kadoka::shogi::runtime {
namespace {

bool same_move(const Move& lhs, const Move& rhs) {
    return lhs.from == rhs.from
        && lhs.to == rhs.to
        && lhs.drop_piece == rhs.drop_piece
        && lhs.promote == rhs.promote;
}

TurnResult validate_and_apply(
    const Position& position,
    const std::vector<Move>& legal_moves,
    SearchResult search_result,
    std::chrono::nanoseconds decision_time) {
    const bool legal = std::any_of(
        legal_moves.begin(),
        legal_moves.end(),
        [&](const Move& legal_move) {
            return same_move(legal_move, search_result.best_move);
        }
    );

    if (!legal) {
        return TurnResult{
            TurnStatus::IllegalMove,
            std::move(search_result),
            std::nullopt,
            decision_time,
        };
    }

    const Position next = position.after_move(search_result.best_move);
    return TurnResult{
        TurnStatus::MoveApplied,
        std::move(search_result),
        next,
        decision_time,
    };
}

} // namespace

TurnResult run_ai_turn(
    AIBackend& backend,
    const Position& position,
    const SearchLimits& limits) {
    const auto legal_moves = generate_legal_moves(position);
    if (legal_moves.empty()) {
        return TurnResult{TurnStatus::NoLegalMoves, std::nullopt, std::nullopt};
    }

    const auto decision_started = std::chrono::steady_clock::now();
    SearchResult decision = backend.decide(position, limits);
    const auto decision_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - decision_started
    );

    if (decision.action == EngineAction::Resign) {
        return TurnResult{
            TurnStatus::Resigned,
            std::move(decision),
            std::nullopt,
            decision_time,
        };
    }
    if (decision.action == EngineAction::DeclareEnteringKing) {
        return TurnResult{
            TurnStatus::EnteringKingDeclaration,
            std::move(decision),
            std::nullopt,
            decision_time,
        };
    }
    if (decision.action == EngineAction::OfferMutualImpasse) {
        return TurnResult{
            TurnStatus::MutualImpasseOffered,
            std::move(decision),
            std::nullopt,
            decision_time,
        };
    }

    return validate_and_apply(
        position,
        legal_moves,
        std::move(decision),
        decision_time
    );
}

TurnResult run_engine_turn(
    Engine& engine,
    const Position& position,
    const SearchLimits& limits) {
    NativeEngineBackend backend{engine};
    return run_ai_turn(backend, position, limits);
}

} // namespace kadoka::shogi::runtime
