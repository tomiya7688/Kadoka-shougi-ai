#include "kadoka/ai_package/observation.hpp"

#include <stdexcept>
#include <utility>

namespace kadoka::shogi::ai_package {

ObservedGameState observed_state_from_player_observation(
    const runtime::PlayerObservation& observation) {
    const Position parsed = Position::from_sfen(observation.sfen);
    if (parsed.side_to_move() != observation.side_to_move) {
        throw std::invalid_argument(
            "PlayerObservation side_to_move disagrees with SFEN"
        );
    }

    return ObservedGameState{
        observation.sfen,
        observation.side_to_move,
        observation.clock,
    };
}

void ReferenceInternalBoard::assign_observed_state(
    const ObservedGameState& state) {
    Position parsed = Position::from_sfen(state.sfen);
    if (parsed.side_to_move() != state.side_to_move) {
        throw std::invalid_argument(
            "ObservedGameState side_to_move disagrees with SFEN"
        );
    }

    position_ = std::move(parsed);
    clock_ = state.clock;
    initialized_ = true;
}

} // namespace kadoka::shogi::ai_package
