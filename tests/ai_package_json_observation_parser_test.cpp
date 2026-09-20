#include "kadoka/ai_package/json_player_observation_parser.hpp"

#include "kadoka/runtime/player_api.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;
using namespace kadoka::shogi::ai_package;

namespace {

class CapturingConverter final : public InternalBoardConverter {
public:
    void assign_observed_state(
        const ObservedGameState& state
    ) override {
        assigned = true;
        captured = state;
    }

    bool assigned{false};
    ObservedGameState captured{};
};

class ThrowingConverter final : public InternalBoardConverter {
public:
    void assign_observed_state(
        const ObservedGameState&
    ) override {
        throw std::runtime_error("conversion exploded");
    }
};

template <class Function>
void assert_parse_error(Function&& function) {
    bool failed = false;
    try {
        function();
    } catch (const PlayerObservationParseError&) {
        failed = true;
    }
    assert(failed);
}

} // namespace

int main() {
    const JsonPlayerObservationParser parser;

    {
        PlayerClock clock;
        clock.black.main_ms = 120000;
        clock.black.byoyomi_ms = 30000;
        clock.black.increment_ms = 1000;
        clock.white.main_ms = 90000;
        clock.white.byoyomi_ms = 20000;
        clock.white.increment_ms = 0;
        clock.per_move_limit_ms = 5000;

        const Position position = Position::from_sfen(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/"
            "1B5R1/LNSGKGSNL b Pp 1"
        );
        const PlayerObservation observation =
            make_player_observation(position, clock);

        const std::string json =
            serialize_player_observation(observation);

        const ObservedGameState parsed = parser.parse(json);
        assert(parsed.sfen == position.to_sfen());
        assert(parsed.side_to_move == Color::Black);
        assert(parsed.clock.black.main_ms == 120000);
        assert(parsed.clock.black.byoyomi_ms == 30000);
        assert(parsed.clock.black.increment_ms == 1000);
        assert(parsed.clock.white.main_ms == 90000);
        assert(parsed.clock.white.byoyomi_ms == 20000);
        assert(parsed.clock.white.increment_ms == 0);
        assert(parsed.clock.per_move_limit_ms == 5000);

        const ReferenceInternalBoard reference =
            parser.parse_reference(json);
        assert(reference.initialized());
        assert(reference.position().to_sfen() == position.to_sfen());
        assert(
            reference.position().hand_count(
                Color::Black,
                PieceType::Pawn
            ) == 1
        );
        assert(
            reference.position().hand_count(
                Color::White,
                PieceType::Pawn
            ) == 1
        );
        assert(reference.clock().black.main_ms == 120000);

        CapturingConverter converter;
        parser.parse_into(json, converter);
        assert(converter.assigned);
        assert(converter.captured.sfen == position.to_sfen());
        assert(converter.captured.side_to_move == Color::Black);
    }

    {
        // Unknown fields are deliberately tolerated by Player API v1.
        std::string json = serialize_player_observation(
            make_player_observation(Position::startpos())
        );
        assert(!json.empty() && json.back() == '}');
        json.pop_back();
        json += R"(,"future":{"vision":{"confidence":0.99},"ignored":[1,true,null]}})";

        const ReferenceInternalBoard reference =
            parser.parse_reference(json);
        assert(reference.initialized());
        assert(
            reference.position().to_sfen()
            == Position::startpos().to_sfen()
        );
    }

    {
        // Demonstrates the shared post-observation boundary: a future screen
        // recognizer can construct the same ObservedGameState without JSON.
        const PlayerObservation observation =
            make_player_observation(Position::startpos());
        const ObservedGameState normalized =
            observed_state_from_player_observation(observation);

        ReferenceInternalBoard from_direct_observation;
        from_direct_observation.assign_observed_state(normalized);

        const ReferenceInternalBoard from_json =
            parser.parse_reference(
                serialize_player_observation(observation)
            );

        assert(
            from_direct_observation.position().to_sfen()
            == from_json.position().to_sfen()
        );
        assert(
            from_direct_observation.position().side_to_move()
            == from_json.position().side_to_move()
        );
    }

    {
        assert_parse_error([&] {
            (void)parser.parse(R"({
                "schema":"kadoka.player_observation",
                "version":2,
                "board":{"sfen":"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"},
                "side_to_move":"black",
                "clock":{
                    "black":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "white":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "per_move_limit_ms":null
                }
            })");
        });

        assert_parse_error([&] {
            (void)parser.parse(R"({
                "schema":"kadoka.player_observation",
                "version":1,
                "side_to_move":"black",
                "clock":{
                    "black":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "white":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "per_move_limit_ms":null
                }
            })");
        });

        assert_parse_error([&] {
            (void)parser.parse(R"({
                "schema":"kadoka.player_observation",
                "version":1,
                "board":{"sfen":"not-sfen"},
                "side_to_move":"black",
                "clock":{
                    "black":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "white":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "per_move_limit_ms":null
                }
            })");
        });

        assert_parse_error([&] {
            (void)parser.parse(R"({
                "schema":"kadoka.player_observation",
                "version":1,
                "board":{"sfen":"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"},
                "side_to_move":"white",
                "clock":{
                    "black":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "white":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "per_move_limit_ms":null
                }
            })");
        });
    }

    {
        ThrowingConverter converter;
        assert_parse_error([&] {
            parser.parse_into(
                serialize_player_observation(
                    make_player_observation(Position::startpos())
                ),
                converter
            );
        });
    }

    {
        ReferenceInternalBoard reference;
        const ObservedGameState inconsistent{
            Position::startpos().to_sfen(),
            Color::White,
            {},
        };

        bool rejected = false;
        try {
            reference.assign_observed_state(inconsistent);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
        assert(!reference.initialized());
    }

    return 0;
}
