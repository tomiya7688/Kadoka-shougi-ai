#include "kadoka/training/preprocessing_pipeline.hpp"
#include "kadoka/training/sfen_lines_parser.hpp"

#include "kadoka/runtime/player_api.hpp"

#include <cassert>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi;
using namespace kadoka::shogi::training;

namespace {

constexpr std::string_view kStartSfen =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
constexpr std::string_view kMirroredStartSfen =
    "lnsgkgsnl/1b5r1/ppppppppp/9/9/9/PPPPPPPPP/1R5B1/LNSGKGSNL b - 1";
ParsedRecord action_record(
    std::string sfen,
    Color side,
    Color actor,
    Move move) {
    ParsedRecord record;
    record.game_id = "game-1";
    record.ply = 0;
    record.sfen = std::move(sfen);
    record.side_to_move = side;
    record.events.push_back(ParsedEvent{
        0,
        ParsedEventType::Action,
        actor,
        runtime::PlayerAction::move_action(move),
        runtime::ActionResult{
            runtime::ActionResultStatus::Accepted,
            std::nullopt,
        },
        std::nullopt,
        SourceLocation{"game_aux.jsonl", 1},
    });
    record.provenance.parser_id = "test";
    record.provenance.locations.push_back(
        SourceLocation{"board_state.jsonl", 1}
    );
    return record;
}

void assert_invalid_config(std::string_view json) {
    bool rejected = false;
    try {
        (void)deserialize_preprocessing_pipeline_config(json);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}

class DuplicateStage final : public Preprocessor {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return "test_duplicate";
    }

    void apply(
        const ParsedRecord& input,
        std::vector<ParsedRecord>& output,
        PreprocessorContext&) const override {
        output.push_back(input);
        output.push_back(input);
    }
};

} // namespace

int main() {
    const PreprocessingPipelineConfig defaults =
        make_reference_preprocessing_pipeline_config();
    assert(defaults.steps.size() == 4);
    assert(defaults.steps[0] == kExcludeInvalidPreprocessorId);
    assert(defaults.steps[1] == kNormalizeSidePreprocessorId);
    assert(defaults.steps[2] == kExpandFileMirrorPreprocessorId);
    assert(defaults.steps[3] == kDeduplicatePreprocessorId);

    const std::string serialized =
        serialize_preprocessing_pipeline_config(defaults);
    const PreprocessingPipelineConfig decoded =
        deserialize_preprocessing_pipeline_config(serialized);
    assert(decoded.steps == defaults.steps);
    assert(
        deserialize_preprocessing_pipeline_config(
            R"({ "steps" : ["exclude_invalid"], "version":1,"schema":"kadoka.training_preprocessing_pipeline" })"
        ).steps == std::vector<std::string>{"exclude_invalid"}
    );
    assert_invalid_config(
        R"({"schema":"kadoka.training_preprocessing_pipeline","version":2,"steps":[]})"
    );
    assert_invalid_config(
        R"({"schema":"kadoka.training_preprocessing_pipeline","version":01,"steps":[]})"
    );
    assert_invalid_config(
        R"({"schema":"kadoka.training_preprocessing_pipeline","version":1,"steps":[],})"
    );

    const PreprocessorRegistry registry =
        make_reference_preprocessor_registry();

    {
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{defaults, registry, output};
        const Move move = runtime::move_from_usi("7g7f");
        ParsedRecord record = action_record(
            std::string(kStartSfen),
            Color::Black,
            Color::Black,
            move
        );
        pipeline.on_record(record);
        pipeline.on_record(record);

        assert(output.records().size() == 2);
        assert(output.records()[0].sfen == kStartSfen);
        assert(output.records()[1].sfen == kMirroredStartSfen);
        assert(
            runtime::move_to_usi(
                *output.records()[0].events[0].action->move
            ) == "7g7f"
        );
        assert(
            runtime::move_to_usi(
                *output.records()[1].events[0].action->move
            ) == "3g3f"
        );
        assert(pipeline.summary().input_records == 2);
        assert(pipeline.summary().emitted_records == 2);
        assert(pipeline.summary().duplicate_records == 2);
        assert(pipeline.summary().expanded_records == 2);

        pipeline.on_error(ParseError{
            ParseErrorCode::MalformedRecord,
            "test",
            "bad.jsonl",
            1,
            "bad record",
            true,
        });
        assert(output.errors().size() == 1);
        assert(pipeline.summary().forwarded_errors == 1);

        pipeline.reset();
        assert(pipeline.summary().input_records == 0);
        assert(pipeline.summary().forwarded_errors == 0);
        pipeline.on_record(record);
        assert(output.records().size() == 4);
    }

    {
        PreprocessingPipelineConfig config{{
            std::string(kExcludeInvalidPreprocessorId),
            std::string(kNormalizeSidePreprocessorId),
        }};
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{config, registry, output};

        ParsedRecord white_to_move = action_record(
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1",
            Color::White,
            Color::White,
            runtime::move_from_usi("3c3d")
        );
        white_to_move.clock.black_main_ms = 1100;
        white_to_move.clock.white_main_ms = 2200;
        white_to_move.clock.black_byoyomi_ms = 300;
        white_to_move.clock.white_byoyomi_ms = 400;
        white_to_move.clock.black_increment_ms = 10;
        white_to_move.clock.white_increment_ms = 20;
        white_to_move.clock.per_move_limit_ms = 900;
        white_to_move.events.push_back(ParsedEvent{
            1,
            ParsedEventType::Terminal,
            std::nullopt,
            std::nullopt,
            std::nullopt,
            runtime::make_win_outcome(
                Color::Black,
                runtime::GameEndReason::Resignation
            ),
            SourceLocation{"game_aux.jsonl", 2},
        });

        pipeline.on_record(white_to_move);
        assert(output.records().size() == 1);
        const ParsedRecord& normalized = output.records()[0];
        assert(normalized.side_to_move == Color::Black);
        assert(normalized.sfen == kStartSfen);
        assert(normalized.clock.black_main_ms == 2200);
        assert(normalized.clock.white_main_ms == 1100);
        assert(normalized.clock.black_byoyomi_ms == 400);
        assert(normalized.clock.white_byoyomi_ms == 300);
        assert(normalized.clock.black_increment_ms == 20);
        assert(normalized.clock.white_increment_ms == 10);
        assert(normalized.clock.per_move_limit_ms == 900);
        assert(normalized.events[0].actor == Color::Black);
        assert(
            runtime::move_to_usi(*normalized.events[0].action->move)
            == "7g7f"
        );
        assert(
            normalized.events[1].terminal->result
            == runtime::GameResult::WhiteWin
        );
        assert(normalized.events[1].terminal->winner == Color::White);
        assert(normalized.events[1].terminal->loser == Color::Black);

        ParsedRecord invalid = white_to_move;
        invalid.sfen = "not a sfen";
        pipeline.on_record(std::move(invalid));
        ParsedRecord inconsistent = white_to_move;
        inconsistent.side_to_move = Color::Black;
        pipeline.on_record(std::move(inconsistent));
        assert(output.records().size() == 1);
        assert(pipeline.summary().invalid_records == 2);
    }

    {
        PreprocessingPipelineConfig config{{
            std::string(kNormalizeSidePreprocessorId),
        }};
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{config, registry, output};
        pipeline.on_record(action_record(
            "4k4/9/9/9/9/9/9/9/4K4 w 2R3p 1",
            Color::White,
            Color::White,
            runtime::move_from_usi("5a5b")
        ));

        assert(output.records().size() == 1);
        const Position normalized = Position::from_sfen(
            output.records()[0].sfen
        );
        assert(normalized.side_to_move() == Color::Black);
        assert(normalized.hand_count(Color::Black, PieceType::Pawn) == 3);
        assert(normalized.hand_count(Color::White, PieceType::Rook) == 2);
    }

    {
        PreprocessingPipelineConfig config{{
            std::string(kDeduplicatePreprocessorId),
        }};
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{config, registry, output};
        ParsedRecord first = action_record(
            std::string(kStartSfen),
            Color::Black,
            Color::Black,
            runtime::move_from_usi("7g7f")
        );
        ParsedRecord same_label_different_game = first;
        same_label_different_game.game_id = "game-2";
        same_label_different_game.ply = 25;
        same_label_different_game.sfen =
            "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 01";
        same_label_different_game.events[0].event_index = 25;
        same_label_different_game.provenance.locations.clear();
        pipeline.on_record(first);
        pipeline.on_record(same_label_different_game);

        ParsedRecord different_label = action_record(
            std::string(kStartSfen),
            Color::Black,
            Color::Black,
            runtime::move_from_usi("2g2f")
        );
        pipeline.on_record(different_label);

        ParsedRecord different_clock = first;
        different_clock.clock.black_main_ms = 1000;
        pipeline.on_record(different_clock);
        assert(output.records().size() == 3);
        assert(pipeline.summary().duplicate_records == 1);
    }

    {
        PreprocessingPipelineConfig config{{
            std::string(kExpandFileMirrorPreprocessorId),
        }};
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{config, registry, output};
        const std::string asymmetric_sfen =
            "4k4/9/9/9/9/9/2P6/9/4K4 b - 1";
        pipeline.on_record(action_record(
            asymmetric_sfen,
            Color::Black,
            Color::Black,
            runtime::move_from_usi("7g7f")
        ));

        assert(output.records().size() == 2);
        const Position original = Position::from_sfen(
            output.records()[0].sfen
        );
        const Position mirrored = Position::from_sfen(
            output.records()[1].sfen
        );
        assert(original.at(Square{7, 7}).type == PieceType::Pawn);
        assert(mirrored.at(Square{3, 7}).type == PieceType::Pawn);
        assert(mirrored.at(Square{3, 7}).color == Color::Black);
        assert(
            runtime::move_to_usi(*output.records()[1].events[0].action->move)
            == "3g3f"
        );
        assert(output.records()[1].provenance.parser_id == "test");
        assert(output.records()[1].provenance.locations[0].source_name
            == "board_state.jsonl");
    }

    {
        PreprocessorRegistry extensible =
            make_reference_preprocessor_registry();
        extensible.add(
            "test_duplicate",
            [] { return std::make_shared<DuplicateStage>(); }
        );
        PreprocessingPipelineConfig config{{"test_duplicate"}};
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{config, extensible, output};
        pipeline.on_record(action_record(
            std::string(kStartSfen),
            Color::Black,
            Color::Black,
            runtime::move_from_usi("7g7f")
        ));
        assert(output.records().size() == 2);
        assert(pipeline.summary().emitted_records == 2);
    }

    {
        PreprocessingPipelineConfig config{{
            std::string(kExcludeInvalidPreprocessorId),
        }};
        CollectingParsedRecordSink output;
        PreprocessingPipeline pipeline{config, registry, output};
        std::istringstream input{
            std::string(kStartSfen) + "\ninvalid sfen\n"
        };
        const std::vector<ParserSource> sources{
            ParserSource{"positions.sfen", &input},
        };
        SfenLinesParser parser;
        const ParseSummary parse_summary = parser.parse(sources, pipeline);

        assert(parse_summary.completed);
        assert(parse_summary.records_emitted == 1);
        assert(parse_summary.errors_emitted == 1);
        assert(output.records().size() == 1);
        assert(output.errors().size() == 1);
        assert(pipeline.summary().input_records == 1);
        assert(pipeline.summary().forwarded_errors == 1);
    }

    return 0;
}
