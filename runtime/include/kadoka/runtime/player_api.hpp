#pragma once

#include "kadoka/position.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace kadoka::shogi::runtime {

inline constexpr std::uint32_t kPlayerApiSchemaVersion = 1;

struct PlayerClockSide {
    std::optional<std::int64_t> main_ms{};
    std::optional<std::int64_t> byoyomi_ms{};
    std::optional<std::int64_t> increment_ms{};
};

struct PlayerClock {
    PlayerClockSide black{};
    PlayerClockSide white{};
    std::optional<std::int64_t> per_move_limit_ms{};
};

struct PlayerObservation {
    std::string sfen{};
    Color side_to_move{Color::Black};
    PlayerClock clock{};
};

enum class PlayerActionType : std::uint8_t {
    Move,
    Resign,
};

struct PlayerAction {
    PlayerActionType type{PlayerActionType::Move};
    std::optional<Move> move{};

    [[nodiscard]] static PlayerAction move_action(Move value);
    [[nodiscard]] static PlayerAction resign_action();
};

enum class ActionResultStatus : std::uint8_t {
    Accepted,
    Illegal,
};

struct ActionResult {
    ActionResultStatus status{ActionResultStatus::Accepted};
    std::optional<std::string> reason{};
};

struct NativeActionApplication {
    ActionResult result{};
    std::optional<Position> next_position{};
    bool resigned{false};
};

[[nodiscard]] PlayerObservation make_player_observation(
    const Position& position,
    PlayerClock clock = {}
);

[[nodiscard]] NativeActionApplication apply_player_action(
    const Position& position,
    const PlayerAction& action
);

[[nodiscard]] std::string move_to_usi(const Move& move);
[[nodiscard]] Move move_from_usi(std::string_view usi);

[[nodiscard]] std::string serialize_player_observation(
    const PlayerObservation& observation
);
[[nodiscard]] PlayerObservation deserialize_player_observation(
    std::string_view json
);

[[nodiscard]] std::string serialize_player_action(const PlayerAction& action);
[[nodiscard]] PlayerAction deserialize_player_action(std::string_view json);

[[nodiscard]] std::string serialize_action_result(const ActionResult& result);
[[nodiscard]] ActionResult deserialize_action_result(std::string_view json);

} // namespace kadoka::shogi::runtime
