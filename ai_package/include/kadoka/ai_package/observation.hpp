#pragma once

#include "kadoka/position.hpp"
#include "kadoka/runtime/player_api.hpp"

#include <string>
#include <string_view>

namespace kadoka::shogi::ai_package {

struct ObservedGameState {
    std::string sfen{};
    Color side_to_move{Color::Black};
    runtime::PlayerClock clock{};
};

// AI-package boundary used after an external observation has been normalized.
// Concrete engines can translate ObservedGameState into their own high-speed
// board representation. Screen-recognition adapters can target this same
// boundary without requiring JSON in the search hot path.
class InternalBoardConverter {
public:
    virtual ~InternalBoardConverter() = default;

    virtual void assign_observed_state(
        const ObservedGameState& state
    ) = 0;
};

// Reference implementation for packages that are happy to reuse the project's
// Position representation internally. Production engines remain free to use a
// different representation by implementing InternalBoardConverter.
class ReferenceInternalBoard final : public InternalBoardConverter {
public:
    void assign_observed_state(
        const ObservedGameState& state
    ) override;

    [[nodiscard]] const Position& position() const noexcept {
        return position_;
    }

    [[nodiscard]] const runtime::PlayerClock& clock() const noexcept {
        return clock_;
    }

    [[nodiscard]] bool initialized() const noexcept {
        return initialized_;
    }

private:
    Position position_{};
    runtime::PlayerClock clock_{};
    bool initialized_{false};
};

[[nodiscard]] ObservedGameState observed_state_from_player_observation(
    const runtime::PlayerObservation& observation
);

} // namespace kadoka::shogi::ai_package
