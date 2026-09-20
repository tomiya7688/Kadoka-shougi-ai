#include "kadoka/runtime/player_api.hpp"

#include <cassert>
#include <stdexcept>
#include <string>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

bool same_move(const Move& lhs, const Move& rhs) {
    return lhs.from == rhs.from
        && lhs.to == rhs.to
        && lhs.drop_piece == rhs.drop_piece
        && lhs.promote == rhs.promote;
}

template <class Function>
void assert_invalid(Function&& function) {
    bool failed = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        failed = true;
    }
    assert(failed);
}

} // namespace

int main() {
    {
        const Move normal = move_from_usi("7g7f");
        assert(normal.from == Square{7, 7});
        assert(normal.to == Square{7, 6});
        assert(normal.drop_piece == PieceType::None);
        assert(!normal.promote);
        assert(move_to_usi(normal) == "7g7f");

        const Move promoted = move_from_usi("2b3c+");
        assert(promoted.from == Square{2, 2});
        assert(promoted.to == Square{3, 3});
        assert(promoted.promote);
        assert(move_to_usi(promoted) == "2b3c+");

        const Move drop = move_from_usi("P*7f");
        assert(!drop.from.has_value());
        assert(drop.to == Square{7, 6});
        assert(drop.drop_piece == PieceType::Pawn);
        assert(!drop.promote);
        assert(move_to_usi(drop) == "P*7f");

        assert_invalid([] { (void)move_from_usi("7z7f"); });
        assert_invalid([] { (void)move_from_usi("K*7f"); });
        assert_invalid([] { (void)move_from_usi("7g7f?"); });
    }

    {
        PlayerClock clock;
        clock.black.main_ms = 60000;
        clock.black.byoyomi_ms = 30000;
        clock.black.increment_ms = 0;
        clock.white.main_ms = 59000;
        clock.white.byoyomi_ms = 30000;
        clock.white.increment_ms = 0;
        clock.per_move_limit_ms = std::nullopt;

        const Position start = Position::startpos();
        const PlayerObservation observation =
            make_player_observation(start, clock);
        assert(observation.sfen == start.to_sfen());
        assert(observation.side_to_move == Color::Black);

        const std::string json =
            serialize_player_observation(observation);
        const PlayerObservation decoded =
            deserialize_player_observation(json);
        assert(decoded.sfen == observation.sfen);
        assert(decoded.side_to_move == Color::Black);
        assert(decoded.clock.black.main_ms == 60000);
        assert(decoded.clock.white.main_ms == 59000);
        assert(decoded.clock.per_move_limit_ms == std::nullopt);
    }

    {
        const Position start = Position::startpos();
        std::string json = serialize_player_observation(
            make_player_observation(start)
        );
        assert(!json.empty() && json.back() == '}');
        json.pop_back();
        json += R"(,"future":{"numbers":[1,1.5,true,null,{"text":"ok"}]},"optional":"ignored"})";
        const PlayerObservation decoded =
            deserialize_player_observation(json);
        assert(decoded.sfen == start.to_sfen());
    }

    {
        const Position start = Position::startpos();
        std::string json = serialize_player_observation(
            make_player_observation(start)
        );
        const std::string needle = R"("side_to_move":"black")";
        const std::size_t at = json.find(needle);
        assert(at != std::string::npos);
        json.replace(at, needle.size(), R"("side_to_move":"white")");
        assert_invalid([&] {
            (void)deserialize_player_observation(json);
        });
    }

    {
        const std::string wrong_schema = R"({
            "schema":"other",
            "version":1,
            "type":"resign"
        })";
        assert_invalid([&] {
            (void)deserialize_player_action(wrong_schema);
        });

        const std::string wrong_version = R"({
            "schema":"kadoka.player_action",
            "version":2,
            "type":"resign"
        })";
        assert_invalid([&] {
            (void)deserialize_player_action(wrong_version);
        });
    }

    {
        const PlayerAction move =
            PlayerAction::move_action(move_from_usi("7g7f"));
        const std::string json = serialize_player_action(move);
        const PlayerAction decoded = deserialize_player_action(json);
        assert(decoded.type == PlayerActionType::Move);
        assert(decoded.move.has_value());
        assert(same_move(*decoded.move, *move.move));

        const PlayerAction resign = PlayerAction::resign_action();
        const PlayerAction resign_decoded =
            deserialize_player_action(serialize_player_action(resign));
        assert(resign_decoded.type == PlayerActionType::Resign);
        assert(!resign_decoded.move.has_value());

        const PlayerAction future = deserialize_player_action(R"({
            "schema":"kadoka.player_action",
            "version":1,
            "type":"move",
            "move":"7g7f",
            "future":{"engine_hint":"ignored"}
        })");
        assert(future.type == PlayerActionType::Move);
        assert(future.move.has_value());
    }

    {
        const Position start = Position::startpos();
        const std::string before = start.to_sfen();

        const NativeActionApplication legal = apply_player_action(
            start,
            PlayerAction::move_action(move_from_usi("7g7f"))
        );
        assert(legal.result.status == ActionResultStatus::Accepted);
        assert(!legal.result.reason.has_value());
        assert(legal.next_position.has_value());
        assert(!legal.resigned);
        assert(legal.next_position->side_to_move() == Color::White);
        assert(start.to_sfen() == before);

        const NativeActionApplication illegal = apply_player_action(
            start,
            PlayerAction::move_action(move_from_usi("7g7e"))
        );
        assert(illegal.result.status == ActionResultStatus::Illegal);
        assert(illegal.result.reason == "illegal_move");
        assert(!illegal.next_position.has_value());
        assert(!illegal.resigned);
        assert(start.to_sfen() == before);
        assert(start.side_to_move() == Color::Black);

        const NativeActionApplication resigned = apply_player_action(
            start,
            PlayerAction::resign_action()
        );
        assert(resigned.result.status == ActionResultStatus::Accepted);
        assert(!resigned.next_position.has_value());
        assert(resigned.resigned);
        assert(start.to_sfen() == before);
    }

    {
        const ActionResult accepted{
            ActionResultStatus::Accepted,
            std::nullopt,
        };
        const ActionResult accepted_decoded =
            deserialize_action_result(serialize_action_result(accepted));
        assert(
            accepted_decoded.status
            == ActionResultStatus::Accepted
        );
        assert(!accepted_decoded.reason.has_value());

        const ActionResult illegal{
            ActionResultStatus::Illegal,
            std::string("illegal_move"),
        };
        const ActionResult illegal_decoded =
            deserialize_action_result(serialize_action_result(illegal));
        assert(illegal_decoded.status == ActionResultStatus::Illegal);
        assert(illegal_decoded.reason == "illegal_move");

        assert_invalid([] {
            (void)deserialize_action_result(R"({
                "schema":"kadoka.action_result",
                "version":1,
                "status":"illegal"
            })");
        });
    }

    {
        PlayerClock invalid_clock;
        invalid_clock.black.main_ms = -1;
        assert_invalid([&] {
            (void)serialize_player_observation(
                make_player_observation(
                    Position::startpos(),
                    invalid_clock
                )
            );
        });

        assert_invalid([] {
            (void)deserialize_player_observation(R"({
                "schema":"kadoka.player_observation",
                "version":1,
                "board":{"sfen":"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"},
                "side_to_move":"black",
                "clock":{
                    "black":{"main_ms":-1,"byoyomi_ms":0,"increment_ms":0},
                    "white":{"main_ms":0,"byoyomi_ms":0,"increment_ms":0},
                    "per_move_limit_ms":null
                }
            })");
        });
    }

    return 0;
}
