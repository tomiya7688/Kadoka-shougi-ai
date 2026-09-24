#include "kadoka/ai_package/observation.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace kadoka::shogi::ai_package {
namespace {

void validate_clock_value(
    const std::optional<std::int64_t>& value,
    const char* name) {
    if (value.has_value() && *value < 0) {
        throw std::invalid_argument(
            std::string(name) + " must be nonnegative or null"
        );
    }
}

void validate_clock(const runtime::PlayerClock& clock) {
    validate_clock_value(clock.black.main_ms, "black.main_ms");
    validate_clock_value(clock.black.byoyomi_ms, "black.byoyomi_ms");
    validate_clock_value(clock.black.increment_ms, "black.increment_ms");
    validate_clock_value(clock.white.main_ms, "white.main_ms");
    validate_clock_value(clock.white.byoyomi_ms, "white.byoyomi_ms");
    validate_clock_value(clock.white.increment_ms, "white.increment_ms");
    validate_clock_value(clock.per_move_limit_ms, "per_move_limit_ms");
}

} // namespace

void validate_observed_game_state(const ObservedGameState& state) {
    const Position parsed = Position::from_sfen(state.sfen);
    if (parsed.side_to_move() != state.side_to_move) {
        throw std::invalid_argument(
            "ObservedGameState side_to_move disagrees with SFEN"
        );
    }
    validate_clock(state.clock);
}

ObservedGameState observed_state_from_player_observation(
    const runtime::PlayerObservation& observation) {
    ObservedGameState state{
        observation.sfen,
        observation.side_to_move,
        observation.clock,
    };
    validate_observed_game_state(state);
    return state;
}

void ReferenceInternalBoard::assign_observed_state(
    const ObservedGameState& state) {
    validate_observed_game_state(state);
    position_ = Position::from_sfen(state.sfen);
    clock_ = state.clock;
    initialized_ = true;
}

} // namespace kadoka::shogi::ai_package
