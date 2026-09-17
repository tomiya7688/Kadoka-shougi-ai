#include "kadoka/runtime/turn_runner.hpp"

#include "kadoka/movegen.hpp"

#include <algorithm>
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
    SearchResult search_result) {
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
        };
    }

    const Position next = position.after_move(search_result.best_move);
    return TurnResult{
        TurnStatus::MoveApplied,
        std::move(search_result),
        next,
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

    SearchResult decision = backend.decide(position, limits);
    if (decision.action == EngineAction::Resign) {
        return TurnResult{TurnStatus::Resigned, std::move(decision), std::nullopt};
    }
    if (decision.action == EngineAction::DeclareEnteringKing) {
        return TurnResult{
            TurnStatus::EnteringKingDeclaration,
            std::move(decision),
            std::nullopt,
        };
    }

    return validate_and_apply(position, legal_moves, std::move(decision));
}

TurnResult run_engine_turn(
    Engine& engine,
    const Position& position,
    const SearchLimits& limits) {
    NativeEngineBackend backend{engine};
    return run_ai_turn(backend, position, limits);
}

} // namespace kadoka::shogi::runtime
