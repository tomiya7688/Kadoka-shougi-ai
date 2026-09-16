#pragma once

#include "kadoka/impasse.hpp"
#include "kadoka/runtime/game_outcome.hpp"

namespace kadoka::shogi::runtime {

[[nodiscard]] constexpr GameOutcome entering_king_declaration_outcome(
    const EnteringKingDeclarationResult& declaration,
    Color declarer) noexcept {
    switch (declaration.verdict) {
    case EnteringKingDeclarationVerdict::Win:
        return make_win_outcome(declarer, GameEndReason::Impasse);
    case EnteringKingDeclarationVerdict::Replay:
        return make_replay_outcome(GameEndReason::Impasse);
    case EnteringKingDeclarationVerdict::Loss:
        return make_win_outcome(opposite(declarer), GameEndReason::Impasse);
    }
    return make_unresolved_outcome(GameEndReason::Impasse);
}

[[nodiscard]] constexpr GameOutcome mutual_impasse_outcome(
    const MutualImpasseResult& impasse) noexcept {
    switch (impasse.verdict) {
    case MutualImpasseVerdict::Replay:
        return make_replay_outcome(GameEndReason::Impasse);
    case MutualImpasseVerdict::BlackLoses:
        return make_win_outcome(Color::White, GameEndReason::Impasse);
    case MutualImpasseVerdict::WhiteLoses:
        return make_win_outcome(Color::Black, GameEndReason::Impasse);
    case MutualImpasseVerdict::InvalidMaterial:
        return make_unresolved_outcome(GameEndReason::Impasse);
    }
    return make_unresolved_outcome(GameEndReason::Impasse);
}

} // namespace kadoka::shogi::runtime
