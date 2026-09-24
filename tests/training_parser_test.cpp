#include "kadoka/training/core_history_parser.hpp"
#include "kadoka/training/parser.hpp"
#include "kadoka/training/sfen_lines_parser.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;
using namespace kadoka::shogi::training;

namespace {

constexpr const char* kGameId =
    "00000000000000000000000000";

std::string board_jsonl() {
    return std::string{
        R"({"schema":"kadoka.board_state","version":1,"game_id":"00000000000000000000000000","ply":0,"sfen":"lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1","side_to_move":"black","clock":{"black_main_ms":60000,"white_main_ms":59000,"black_byoyomi_ms":30000,"white_byoyomi_ms":30000,"black_increment_ms":0,"white_increment_ms":0,"per_move_limit_ms":null}})"
        "\n"
        R"({"schema":"kadoka.board_state","version":1,"game_id":"00000000000000000000000000","ply":1,"sfen":"lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2","side_to_move":"white","clock":{"black_main_ms":59000,"white_main_ms":59000,"black_byoyomi_ms":30000,"white_byoyomi_ms":30000,"black_increment_ms":0,"white_increment_ms":0,"per_move_limit_ms":null}})"
        "\n"
    };
}

std::string aux_jsonl() {
    return std::string{
        R"({"schema":"kadoka.game_aux","version":1,"game_id":"00000000000000000000000000","ply":0,"event_index":0,"event_type":"action","actor":"black","action":{"type":"move","move":"7g7e"},"result":{"status":"illegal","reason":"illegal_move"},"terminal":null})"
        "\n"
        R"({"schema":"kadoka.game_aux","version":1,"game_id":"00000000000000000000000000","ply":0,"event_index":1,"event_type":"action","actor":"black","action":{"type":"move","move":"7g7f"},"result":{"status":"accepted","reason":null},"terminal":null})"
        "\n"
        R"({"schema":"kadoka.game_aux","version":1,"game_id":"00000000000000000000000000","ply":1,"event_index":2,"event_type":"action","actor":"white","action":{"type":"resign"},"result":{"status":"accepted","reason":null},"terminal":null})"
        "\n"
        R"({"schema":"kadoka.game_aux","version":1,"game_id":"00000000000000000000000000","ply":1,"event_index":3,"event_type":"terminal","actor":null,"action":null,"result":null,"terminal":{"result":"black_win","reason":"resignation"}})"
        "\n"
    };
}

template <class Function>
void assert_invalid(Function&& function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

} // namespace

int main() {
    {
        std::istringstream board{board_jsonl()};
        std::istringstream aux{aux_jsonl()};

        // Intentionally reverse the source order. The Core parser identifies
        // stream roles from the schema rather than relying on filenames/order.
        const std::vector<ParserSource> sources{
            ParserSource{"game_aux.jsonl", &aux},
            ParserSource{"board_state.jsonl", &board},
        };

        CoreHistoryParser parser;
        CollectingParsedRecordSink sink;
        const ParseSummary summary = parser.parse(sources, sink);

        assert(summary.completed);
        assert(summary.records_emitted == 2);
        assert(summary.errors_emitted == 0);
        assert(sink.errors().empty());
        assert(sink.records().size() == 2);

        const ParsedRecord& first = sink.records()[0];
        assert(first.game_id == kGameId);
        assert(first.ply == 0);
        assert(first.side_to_move == Color::Black);
        assert(first.clock.black_main_ms == 60000);
        assert(first.clock.white_main_ms == 59000);
        assert(first.events.size() == 2);
        assert(
            first.provenance.parser_id
            == kCoreHistoryParserId
        );
        assert(first.provenance.locations.size() == 3);
        assert(
            first.provenance.locations[0].source_name
            == "board_state.jsonl"
        );
        assert(first.provenance.locations[0].line == 1);
        assert(
            first.provenance.locations[1].source_name
            == "game_aux.jsonl"
        );
        assert(first.provenance.locations[1].line == 1);
        assert(first.events[0].event_index == 0);
        assert(first.events[0].type == ParsedEventType::Action);
        assert(first.events[0].actor == Color::Black);
        assert(first.events[0].action.has_value());
        assert(first.events[0].action->move.has_value());
        assert(
            move_to_usi(*first.events[0].action->move)
            == "7g7e"
        );
        assert(first.events[0].result.has_value());
        assert(
            first.events[0].result->status
            == ActionResultStatus::Illegal
        );
        assert(first.events[0].result->reason == "illegal_move");

        assert(first.events[1].event_index == 1);
        assert(first.events[1].result.has_value());
        assert(
            first.events[1].result->status
            == ActionResultStatus::Accepted
        );
        assert(
            first.events[1].action.has_value()
            && first.events[1].action->move.has_value()
            && move_to_usi(*first.events[1].action->move)
                == "7g7f"
        );

        const ParsedRecord& second = sink.records()[1];
        assert(second.game_id == kGameId);
        assert(second.ply == 1);
        assert(second.side_to_move == Color::White);
        assert(second.clock.black_main_ms == 59000);
        assert(second.events.size() == 2);
        assert(second.events[0].event_index == 2);
        assert(second.events[0].action.has_value());
        assert(
            second.events[0].action->type
            == PlayerActionType::Resign
        );
        assert(second.events[1].event_index == 3);
        assert(second.events[1].type == ParsedEventType::Terminal);
        assert(second.events[1].terminal.has_value());
        assert(
            second.events[1].terminal->result
            == GameResult::BlackWin
        );
        assert(
            second.events[1].terminal->reason
            == GameEndReason::Resignation
        );
        assert(second.provenance.locations.size() == 3);
        assert(second.provenance.locations[0].line == 2);
        assert(second.provenance.locations[1].line == 3);
        assert(second.provenance.locations[2].line == 4);
    }

    {
        std::istringstream only_board{board_jsonl()};
        const std::vector<ParserSource> sources{
            ParserSource{"board.jsonl", &only_board},
        };
        CoreHistoryParser parser;
        CollectingParsedRecordSink sink;
        const ParseSummary summary = parser.parse(sources, sink);
        assert(!summary.completed);
        assert(summary.records_emitted == 0);
        assert(summary.errors_emitted == 1);
        assert(
            sink.errors()[0].code
            == ParseErrorCode::InvalidInputCount
        );
        assert(!sink.errors()[0].recoverable);
    }

    {
        std::istringstream board{board_jsonl()};
        std::istringstream malformed_aux{
            R"({"schema":"kadoka.game_aux","version":1,"broken":true})"
            "\n"
        };
        const std::vector<ParserSource> sources{
            ParserSource{"board.jsonl", &board},
            ParserSource{"aux.jsonl", &malformed_aux},
        };
        CoreHistoryParser parser;
        CollectingParsedRecordSink sink;
        const ParseSummary summary = parser.parse(sources, sink);
        assert(!summary.completed);
        assert(summary.records_emitted == 0);
        assert(summary.errors_emitted == 1);
        assert(
            sink.errors()[0].code
            == ParseErrorCode::MalformedRecord
        );
        assert(sink.errors()[0].source_name == "aux.jsonl");
        assert(sink.errors()[0].line == 1);
    }

    {
        std::string mismatched = aux_jsonl();
        const std::string original = kGameId;
        const std::string replacement =
            "00000000000000000000000001";
        const std::size_t offset = mismatched.find(original);
        assert(offset != std::string::npos);
        mismatched.replace(
            offset,
            original.size(),
            replacement
        );

        std::istringstream board{board_jsonl()};
        std::istringstream aux{mismatched};
        const std::vector<ParserSource> sources{
            ParserSource{"board.jsonl", &board},
            ParserSource{"aux.jsonl", &aux},
        };

        CoreHistoryParser parser;
        CollectingParsedRecordSink sink;
        const ParseSummary summary = parser.parse(sources, sink);
        assert(!summary.completed);
        assert(summary.errors_emitted == 1);
        assert(
            sink.errors()[0].code
            == ParseErrorCode::JoinMismatch
        );
    }

    {
        std::string bad_order = aux_jsonl();
        const std::string old_fragment =
            R"("event_index":1)";
        const std::string new_fragment =
            R"("event_index":9)";
        const std::size_t offset = bad_order.find(old_fragment);
        assert(offset != std::string::npos);
        bad_order.replace(
            offset,
            old_fragment.size(),
            new_fragment
        );

        std::istringstream board{board_jsonl()};
        std::istringstream aux{bad_order};
        const std::vector<ParserSource> sources{
            ParserSource{"board.jsonl", &board},
            ParserSource{"aux.jsonl", &aux},
        };

        CoreHistoryParser parser;
        CollectingParsedRecordSink sink;
        const ParseSummary summary = parser.parse(sources, sink);
        assert(!summary.completed);
        assert(summary.records_emitted == 0);
        assert(summary.errors_emitted == 1);
        assert(
            sink.errors()[0].code
            == ParseErrorCode::OrderingViolation
        );
        assert(sink.errors()[0].line == 2);
    }

    {
        std::istringstream sfen{
            "# one SFEN per line\n"
            "\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/"
            "1B5R1/LNSGKGSNL b - 1\n"
            "this is not sfen\n"
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/"
            "1B5R1/LNSGKGSNL w - 2\n"
        };
        const std::vector<ParserSource> sources{
            ParserSource{"positions.sfen", &sfen},
        };

        SfenLinesParser parser;
        CollectingParsedRecordSink sink;
        const ParseSummary summary = parser.parse(sources, sink);

        assert(summary.completed);
        assert(summary.records_emitted == 2);
        assert(summary.errors_emitted == 1);
        assert(sink.records().size() == 2);
        assert(sink.errors().size() == 1);
        assert(sink.errors()[0].recoverable);
        assert(
            sink.errors()[0].code
            == ParseErrorCode::MalformedRecord
        );
        assert(sink.errors()[0].source_name == "positions.sfen");
        assert(sink.errors()[0].line == 4);

        assert(!sink.records()[0].game_id.has_value());
        assert(!sink.records()[0].ply.has_value());
        assert(sink.records()[0].events.empty());
        assert(
            sink.records()[0].side_to_move
            == Color::Black
        );
        assert(
            sink.records()[0].provenance.parser_id
            == kSfenLinesParserId
        );
        assert(
            sink.records()[0].provenance.locations[0].line
            == 3
        );
        assert(
            sink.records()[1].side_to_move
            == Color::White
        );
        assert(
            sink.records()[1].provenance.locations[0].line
            == 5
        );
    }

    {
        ParserRegistry registry = make_reference_parser_registry();
        assert(registry.find(kCoreHistoryParserId) != nullptr);
        assert(registry.find(kSfenLinesParserId) != nullptr);
        assert(registry.find("missing") == nullptr);

        const std::vector<std::string> ids =
            registry.parser_ids();
        assert(ids.size() == 2);
        assert(ids[0] == kCoreHistoryParserId);
        assert(ids[1] == kSfenLinesParserId);

        assert_invalid([&] {
            registry.add(std::make_shared<CoreHistoryParser>());
        });
        assert_invalid([&] {
            registry.add(nullptr);
        });
    }

    return 0;
}
