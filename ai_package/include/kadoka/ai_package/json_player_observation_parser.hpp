#pragma once

#include "kadoka/ai_package/observation.hpp"

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace kadoka::shogi::ai_package {

class PlayerObservationParseError : public std::runtime_error {
public:
    explicit PlayerObservationParseError(std::string message)
        : std::runtime_error(std::move(message)) {}
};

class JsonPlayerObservationParser {
public:
    [[nodiscard]] ObservedGameState parse(
        std::string_view json
    ) const;

    // Parse + normalize + convert to the package's chosen internal board.
    // JSON does not survive beyond this adapter unless a concrete package
    // explicitly chooses to retain it.
    void parse_into(
        std::string_view json,
        InternalBoardConverter& converter
    ) const;

    [[nodiscard]] ReferenceInternalBoard parse_reference(
        std::string_view json
    ) const;
};

} // namespace kadoka::shogi::ai_package
