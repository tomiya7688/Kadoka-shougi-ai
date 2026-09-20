#include "kadoka/runtime/game_history.hpp"

#include <array>
#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

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

template <class Function>
void assert_logic_error(Function&& function) {
    bool failed = false;
    try {
        function();
    } catch (const std::logic_error&) {
        failed = true;
    }
    assert(failed);
}

HistoryClock sample_clock() {
    HistoryClock clock;
    clock.black_main_ms = 60000;
    clock.white_main_ms = 59000;
    clock.black_byoyomi_ms = 30000;
    clock.white_byoyomi_ms = 30000;
    clock.black_increment_ms = 0;
    clock.white_increment_ms = 0;
    clock.per_move_limit_ms = std::nullopt;
    return clock;
}

} // namespace

int main() {
    const std::array<std::uint8_t, 10> zero_entropy{};
    const std::string zero_ulid = make_ulid(0, zero_entropy);
    assert(zero_ulid == "00000000000000000000000000");
    assert(is_valid_ulid(zero_ulid));
    assert(!is_valid_ulid("0000000000000000000000000"));
    assert(!is_valid_ulid("80000000000000000000000000"));
    assert(!is_valid_ulid("0000000000000000000000000I"));

    {
        const BoardStateRecord record = make_board_state_record(
            zero_ulid,
            0,
            Position::startpos(),
            sample_clock()
        );
        const std::string json = serialize_board_state_record(record);
        const BoardStateRecord decoded =
            deserialize_board_state_record(json);

        assert(decoded.game_id == zero_ulid);
        assert(decoded.ply == 0);
        assert(decoded.sfen == Position::startpos().to_sfen());
        assert(decoded.side_to_move == Color::Black);
        assert(decoded.clock.black_main_ms == 60000);
        assert(decoded.clock.white_main_ms == 59000);
    }

    {
        BoardStateRecord record = make_board_state_record(
            zero_ulid,
            0,
            Position::startpos()
        );
        std::string json = serialize_board_state_record(record);
        assert(json.back() == '}');
        json.pop_back();
        json += R"(,"future":{"nested":[1,true,null,{"x":"y"}]}})";
        const BoardStateRecord decoded =
            deserialize_board_state_record(json);
        assert(decoded.game_id == zero_ulid);
        assert(decoded.ply == 0);
    }

    {
        BoardStateRecord record = make_board_state_record(
            zero_ulid,
            0,
            Position::startpos()
        );
        record.side_to_move = Color::White;
        assert_invalid([&] {
            (void)serialize_board_state_record(record);
        });
    }

    {
        const PlayerAction move =
            PlayerAction::move_action(move_from_usi("7g7f"));
        const ActionResult accepted{
            ActionResultStatus::Accepted,
            std::nullopt,
        };

        const GameAuxRecord action_record{
            zero_ulid,
            0,
            0,
            GameAuxEventType::Action,
            Color::Black,
            move,
            accepted,
            std::nullopt,
        };

        const GameAuxRecord decoded = deserialize_game_aux_record(
            serialize_game_aux_record(action_record)
        );
        assert(decoded.game_id == zero_ulid);
        assert(decoded.ply == 0);
        assert(decoded.event_index == 0);
        assert(decoded.event_type == GameAuxEventType::Action);
        assert(decoded.actor == Color::Black);
        assert(decoded.action.has_value());
        assert(decoded.action->type == PlayerActionType::Move);
        assert(decoded.action->move.has_value());
        assert(move_to_usi(*decoded.action->move) == "7g7f");
        assert(decoded.result.has_value());
        assert(
            decoded.result->status
            == ActionResultStatus::Accepted
        );
        assert(!decoded.terminal.has_value());
    }

    {
        const GameOutcome outcome =
            make_win_outcome(Color::Black, GameEndReason::Resignation);
        const GameAuxRecord terminal{
            zero_ulid,
            120,
            121,
            GameAuxEventType::Terminal,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            outcome,
        };

        std::string json = serialize_game_aux_record(terminal);
        assert(json.back() == '}');
        json.pop_back();
        json += R"(,"future":"ignored"})";

        const GameAuxRecord decoded =
            deserialize_game_aux_record(json);
        assert(decoded.event_type == GameAuxEventType::Terminal);
        assert(decoded.ply == 120);
        assert(decoded.event_index == 121);
        assert(decoded.terminal.has_value());
        assert(
            decoded.terminal->result
            == GameResult::BlackWin
        );
        assert(
            decoded.terminal->reason
            == GameEndReason::Resignation
        );
        assert(decoded.terminal->winner == Color::Black);
        assert(decoded.terminal->loser == Color::White);
    }

    {
        GameHistoryRecorder recorder{
            Position::startpos(),
            sample_clock(),
            zero_ulid,
        };

        assert(recorder.game_id() == zero_ulid);
        assert(recorder.ply() == 0);
        assert(recorder.board_states().size() == 1);
        assert(recorder.events().empty());

        const std::string initial_sfen =
            recorder.position().to_sfen();

        const ActionResult illegal = recorder.apply_and_record(
            Color::Black,
            PlayerAction::move_action(move_from_usi("7g7e")),
            sample_clock()
        );
        assert(illegal.status == ActionResultStatus::Illegal);
        assert(illegal.reason == "illegal_move");
        assert(recorder.ply() == 0);
        assert(recorder.board_states().size() == 1);
        assert(recorder.events().size() == 1);
        assert(recorder.events()[0].ply == 0);
        assert(recorder.events()[0].event_index == 0);
        assert(
            recorder.position().to_sfen()
            == initial_sfen
        );
        assert(
            recorder.position().side_to_move()
            == Color::Black
        );

        const ActionResult illegal_again = recorder.apply_and_record(
            Color::Black,
            PlayerAction::move_action(move_from_usi("2g2e")),
            sample_clock()
        );
        assert(
            illegal_again.status
            == ActionResultStatus::Illegal
        );
        assert(recorder.ply() == 0);
        assert(recorder.board_states().size() == 1);
        assert(recorder.events().size() == 2);
        assert(recorder.events()[1].ply == 0);
        assert(recorder.events()[1].event_index == 1);

        const ActionResult legal = recorder.apply_and_record(
            Color::Black,
            PlayerAction::move_action(move_from_usi("7g7f")),
            sample_clock()
        );
        assert(legal.status == ActionResultStatus::Accepted);
        assert(recorder.ply() == 1);
        assert(recorder.board_states().size() == 2);
        assert(recorder.events().size() == 3);
        assert(recorder.events()[2].ply == 0);
        assert(recorder.events()[2].event_index == 2);
        assert(recorder.board_states()[1].ply == 1);
        assert(
            recorder.board_states()[1].side_to_move
            == Color::White
        );

        recorder.record_terminal(
            make_win_outcome(
                Color::Black,
                GameEndReason::Resignation
            )
        );
        assert(recorder.events().size() == 4);
        assert(
            recorder.events().back().event_type
            == GameAuxEventType::Terminal
        );
        assert(recorder.events().back().ply == 1);
        assert(recorder.events().back().event_index == 3);

        assert_logic_error([&] {
            (void)recorder.apply_and_record(
                Color::White,
                PlayerAction::resign_action()
            );
        });
        assert_logic_error([&] {
            recorder.record_terminal(
                make_unresolved_outcome(
                    GameEndReason::PlyLimit
                )
            );
        });

        const std::string board_jsonl =
            serialize_board_state_jsonl(
                recorder.board_states()
            );
        const std::string aux_jsonl =
            serialize_game_aux_jsonl(
                recorder.events()
            );

        const auto board_roundtrip =
            deserialize_board_state_jsonl(board_jsonl);
        const auto aux_roundtrip =
            deserialize_game_aux_jsonl(aux_jsonl);
        assert(board_roundtrip.size() == 2);
        assert(aux_roundtrip.size() == 4);

        // Join contract: accepted/illegal actions at ply 0 join the
        // canonical pre-action BoardState at game_id + ply.
        for (std::size_t index = 0; index < 3; ++index) {
            assert(
                aux_roundtrip[index].game_id
                == board_roundtrip[0].game_id
            );
            assert(aux_roundtrip[index].ply == 0);
            assert(board_roundtrip[0].ply == 0);
        }
        assert(
            aux_roundtrip.back().game_id
            == board_roundtrip[1].game_id
        );
        assert(aux_roundtrip.back().ply == 1);
        assert(board_roundtrip[1].ply == 1);

        std::stringstream board_stream;
        write_board_state_jsonl(
            board_stream,
            recorder.board_states()
        );
        const auto board_from_stream =
            read_board_state_jsonl(board_stream);
        assert(board_from_stream.size() == 2);

        std::stringstream aux_stream;
        write_game_aux_jsonl(aux_stream, recorder.events());
        const auto aux_from_stream =
            read_game_aux_jsonl(aux_stream);
        assert(aux_from_stream.size() == 4);
    }

    {
        GameHistoryRecorder recorder{
            Position::startpos(),
            {},
            zero_ulid,
        };
        const ActionResult resign = recorder.apply_and_record(
            Color::Black,
            PlayerAction::resign_action()
        );
        assert(resign.status == ActionResultStatus::Accepted);
        assert(recorder.ply() == 0);
        assert(recorder.board_states().size() == 1);
        assert(recorder.events().size() == 1);

        assert_logic_error([&] {
            (void)recorder.apply_and_record(
                Color::Black,
                PlayerAction::move_action(
                    move_from_usi("7g7f")
                )
            );
        });

        recorder.record_terminal(
            make_win_outcome(
                Color::White,
                GameEndReason::Resignation
            )
        );
        assert(recorder.events().size() == 2);
        assert(recorder.events()[1].ply == 0);
    }

    {
        assert_invalid([&] {
            (void)deserialize_board_state_record(R"({
                "schema":"kadoka.board_state",
                "version":2,
                "game_id":"00000000000000000000000000",
                "ply":0,
                "sfen":"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
                "side_to_move":"black",
                "clock":{
                    "black_main_ms":null,
                    "white_main_ms":null,
                    "black_byoyomi_ms":null,
                    "white_byoyomi_ms":null,
                    "black_increment_ms":null,
                    "white_increment_ms":null,
                    "per_move_limit_ms":null
                }
            })");
        });

        assert_invalid([&] {
            (void)deserialize_game_aux_record(R"({
                "schema":"kadoka.game_aux",
                "version":1,
                "game_id":"00000000000000000000000000",
                "ply":0,
                "event_index":0,
                "event_type":"terminal",
                "actor":"black",
                "action":null,
                "result":null,
                "terminal":{
                    "result":"black_win",
                    "reason":"resignation"
                }
            })");
        });
    }

    return 0;
}
