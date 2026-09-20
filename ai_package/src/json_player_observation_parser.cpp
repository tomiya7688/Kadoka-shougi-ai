#include "kadoka/ai_package/json_player_observation_parser.hpp"

#include "kadoka/runtime/player_api.hpp"

#include <exception>
#include <stdexcept>
#include <string>

namespace kadoka::shogi::ai_package {

ObservedGameState JsonPlayerObservationParser::parse(
    std::string_view json) const {
    try {
        const runtime::PlayerObservation observation =
            runtime::deserialize_player_observation(json);
        return observed_state_from_player_observation(observation);
    } catch (const std::invalid_argument& error) {
        throw PlayerObservationParseError(
            std::string("PlayerObservation parse failed: ")
            + error.what()
        );
    }
}

void JsonPlayerObservationParser::parse_into(
    std::string_view json,
    InternalBoardConverter& converter) const {
    const ObservedGameState state = parse(json);
    try {
        converter.assign_observed_state(state);
    } catch (const PlayerObservationParseError&) {
        throw;
    } catch (const std::exception& error) {
        throw PlayerObservationParseError(
            std::string("internal board conversion failed: ")
            + error.what()
        );
    }
}

ReferenceInternalBoard JsonPlayerObservationParser::parse_reference(
    std::string_view json) const {
    ReferenceInternalBoard board;
    parse_into(json, board);
    return board;
}

} // namespace kadoka::shogi::ai_package
