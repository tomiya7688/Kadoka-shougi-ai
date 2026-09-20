#include "kadoka/ai_package/json_player_observation_parser.hpp"
#include "kadoka/runtime/game_history.hpp"
#include "kadoka/runtime/player_api.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;
using namespace kadoka::shogi::ai_package;

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error(
            "failed to open fixture: " + path.string()
        );
    }
    std::string content{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()
    };
    if (!input.eof() && input.fail()) {
        throw std::runtime_error(
            "failed to read fixture: " + path.string()
        );
    }
    return content;
}

std::string one_line(std::string text) {
    while (!text.empty()
           && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <class Function>
void require_rejected(Function&& function, std::string_view message) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    } catch (const PlayerObservationParseError&) {
        rejected = true;
    }
    require(rejected, message);
}

const BoardStateRecord* find_board(
    const std::vector<BoardStateRecord>& boards,
    std::string_view game_id,
    std::uint64_t ply) {
    const auto found = std::find_if(
        boards.begin(),
        boards.end(),
        [&](const BoardStateRecord& board) {
            return board.game_id == game_id && board.ply == ply;
        }
    );
    return found == boards.end() ? nullptr : &*found;
}

void validate_player_fixtures(const std::filesystem::path& root) {
    const std::string observation_text =
        read_text(root / "player_observation.json");
    const PlayerObservation observation =
        deserialize_player_observation(observation_text);

    require(
        serialize_player_observation(observation)
            == one_line(observation_text),
        "PlayerObservation canonical round-trip changed"
    );
    require(
        observation.side_to_move == Color::Black,
        "PlayerObservation side_to_move fixture changed"
    );
    require(
        observation.clock.black.main_ms == 60000,
        "black main clock fixture changed"
    );
    require(
        observation.clock.white.main_ms == 59000,
        "white main clock fixture changed"
    );
    require(
        observation.clock.black.byoyomi_ms == 30000,
        "black byoyomi fixture changed"
    );
    require(
        !observation.clock.per_move_limit_ms.has_value(),
        "per_move_limit fixture must remain null"
    );

    const JsonPlayerObservationParser package_parser;
    const ReferenceInternalBoard reference =
        package_parser.parse_reference(observation_text);
    require(
        reference.initialized(),
        "AI package reference board was not initialized"
    );
    require(
        reference.position().to_sfen() == observation.sfen,
        "AI package parser disagrees with PlayerObservation SFEN"
    );
    require(
        reference.clock().black.main_ms
            == observation.clock.black.main_ms,
        "AI package parser lost visible clock data"
    );

    const std::string unknown_text =
        read_text(root / "player_observation_unknown_optional.json");
    const PlayerObservation unknown =
        deserialize_player_observation(unknown_text);
    require(
        serialize_player_observation(unknown)
            == one_line(observation_text),
        "unknown optional fields changed canonical observation"
    );
    const ReferenceInternalBoard unknown_reference =
        package_parser.parse_reference(unknown_text);
    require(
        unknown_reference.position().to_sfen()
            == reference.position().to_sfen(),
        "AI package parser mishandled unknown optional fields"
    );

    const std::string unsupported =
        read_text(root / "player_observation_unsupported_version.json");
    require_rejected(
        [&] { (void)deserialize_player_observation(unsupported); },
        "PlayerObservation unsupported version was accepted"
    );
    require_rejected(
        [&] { (void)package_parser.parse(unsupported); },
        "AI package parser accepted unsupported version"
    );

    const std::string move_text =
        read_text(root / "player_action_move.json");
    const PlayerAction move =
        deserialize_player_action(move_text);
    require(
        move.type == PlayerActionType::Move
            && move.move.has_value()
            && move_to_usi(*move.move) == "7g7f",
        "move action fixture changed"
    );
    require(
        serialize_player_action(move) == one_line(move_text),
        "move action canonical round-trip changed"
    );

    const std::string resign_text =
        read_text(root / "player_action_resign.json");
    const PlayerAction resign =
        deserialize_player_action(resign_text);
    require(
        resign.type == PlayerActionType::Resign
            && !resign.move.has_value(),
        "resign action fixture changed"
    );
    require(
        serialize_player_action(resign) == one_line(resign_text),
        "resign action canonical round-trip changed"
    );

    const std::string accepted_text =
        read_text(root / "action_result_accepted.json");
    const ActionResult accepted =
        deserialize_action_result(accepted_text);
    require(
        accepted.status == ActionResultStatus::Accepted,
        "accepted result fixture changed"
    );
    require(
        serialize_action_result(accepted)
            == one_line(accepted_text),
        "accepted result canonical round-trip changed"
    );

    const std::string illegal_text =
        read_text(root / "action_result_illegal.json");
    const ActionResult illegal =
        deserialize_action_result(illegal_text);
    require(
        illegal.status == ActionResultStatus::Illegal
            && illegal.reason == "illegal_move",
        "illegal result fixture changed"
    );
    require(
        serialize_action_result(illegal)
            == one_line(illegal_text),
        "illegal result canonical round-trip changed"
    );
}

void validate_history_fixtures(const std::filesystem::path& root) {
    const std::string board_text =
        read_text(root / "board_state.jsonl");
    const std::string aux_text =
        read_text(root / "game_aux.jsonl");

    const std::vector<BoardStateRecord> boards =
        deserialize_board_state_jsonl(board_text);
    const std::vector<GameAuxRecord> events =
        deserialize_game_aux_jsonl(aux_text);

    require(boards.size() == 2, "BoardState fixture count changed");
    require(events.size() == 4, "GameAux fixture count changed");
    require(
        serialize_board_state_jsonl(boards) == board_text,
        "BoardState JSONL canonical round-trip changed"
    );
    require(
        serialize_game_aux_jsonl(events) == aux_text,
        "GameAux JSONL canonical round-trip changed"
    );

    require(
        boards[0].game_id == boards[1].game_id,
        "BoardState fixture game_id split"
    );
    require(
        boards[0].ply == 0 && boards[1].ply == 1,
        "BoardState ply progression changed"
    );
    require(
        boards[0].clock.black_main_ms == 60000
            && boards[1].clock.black_main_ms == 59000,
        "BoardState clock round-trip changed"
    );

    std::uint64_t expected_event_index = 0;
    for (const GameAuxRecord& event : events) {
        require(
            event.event_index == expected_event_index++,
            "GameAux event_index is not monotonic from zero"
        );
        require(
            find_board(boards, event.game_id, event.ply) != nullptr,
            "GameAux event cannot join BoardState by game_id + ply"
        );
    }

    require(
        events[0].event_type == GameAuxEventType::Action
            && events[0].ply == 0
            && events[0].result.has_value()
            && events[0].result->status
                == ActionResultStatus::Illegal,
        "illegal action fixture changed"
    );
    require(
        events[1].event_type == GameAuxEventType::Action
            && events[1].ply == 0
            && events[1].result.has_value()
            && events[1].result->status
                == ActionResultStatus::Accepted,
        "accepted move fixture changed"
    );
    require(
        boards[0].ply == events[0].ply
            && boards[0].ply == events[1].ply
            && boards[1].ply == 1,
        "illegal action unexpectedly advanced canonical ply"
    );

    require(
        events[2].event_type == GameAuxEventType::Action
            && events[2].ply == 1
            && events[2].action.has_value()
            && events[2].action->type
                == PlayerActionType::Resign,
        "resignation action fixture changed"
    );
    require(
        events[3].event_type == GameAuxEventType::Terminal
            && events[3].ply == 1
            && events[3].terminal.has_value()
            && events[3].terminal->result
                == GameResult::BlackWin
            && events[3].terminal->reason
                == GameEndReason::Resignation,
        "terminal fixture changed"
    );

    // Re-run the action prefix through the authoritative reference bridge.
    GameHistoryRecorder recorder{
        Position::startpos(),
        boards[0].clock,
        boards[0].game_id,
    };
    require(
        recorder.apply_and_record(
            Color::Black,
            *events[0].action,
            boards[0].clock
        ).status == ActionResultStatus::Illegal,
        "fixture illegal action no longer rejects"
    );
    require(
        recorder.ply() == 0
            && recorder.board_states().size() == 1,
        "illegal fixture action mutated canonical history"
    );
    require(
        recorder.apply_and_record(
            Color::Black,
            *events[1].action,
            boards[1].clock
        ).status == ActionResultStatus::Accepted,
        "fixture legal action no longer accepts"
    );
    require(
        recorder.ply() == 1
            && recorder.position().to_sfen() == boards[1].sfen,
        "fixture accepted move no longer reaches BoardState ply 1"
    );
    require(
        recorder.apply_and_record(
            Color::White,
            *events[2].action,
            boards[1].clock
        ).status == ActionResultStatus::Accepted,
        "fixture resignation no longer accepts"
    );
    recorder.record_terminal(*events[3].terminal);
    require(
        recorder.events().size() == events.size(),
        "fixture replay event count changed"
    );
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) {
            std::cerr
                << "usage: kadoka_wire_compat <installed-fixture-dir>\n";
            return 2;
        }

        const std::filesystem::path root{argv[1]};
        require(
            std::filesystem::is_directory(root),
            "fixture directory does not exist"
        );

        validate_player_fixtures(root);
        validate_history_fixtures(root);

        std::cout
            << "Kadoka wire compatibility fixtures: OK\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "Kadoka wire compatibility fixtures: FAILED: "
            << error.what()
            << '\n';
        return 1;
    }
}
