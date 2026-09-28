#include "kadoka/training/parser.hpp"
#include "kadoka/training/recursive_importer.hpp"

#include <cassert>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

using namespace kadoka::shogi;
using namespace kadoka::shogi::training;

namespace {

constexpr std::string_view kStartSfen =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/"
    "PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

constexpr std::string_view kAfterSfen =
    "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/"
    "PP1PPPPPP/1B5R1/LNSGKGSNL w - 2";

std::string game_id(unsigned suffix) {
    std::string id(26, '0');
    const std::string tail = std::to_string(suffix);
    assert(tail.size() <= id.size());
    id.replace(id.size() - tail.size(), tail.size(), tail);
    return id;
}

std::string board_jsonl(std::string_view id) {
    std::ostringstream out;
    out
        << R"({"schema":"kadoka.board_state","version":1,"game_id":")"
        << id
        << R"(","ply":0,"sfen":")"
        << kStartSfen
        << R"(","side_to_move":"black","clock":{"black_main_ms":60000,"white_main_ms":59000,"black_byoyomi_ms":30000,"white_byoyomi_ms":30000,"black_increment_ms":0,"white_increment_ms":0,"per_move_limit_ms":null}})"
        << '\n'
        << R"({"schema":"kadoka.board_state","version":1,"game_id":")"
        << id
        << R"(","ply":1,"sfen":")"
        << kAfterSfen
        << R"(","side_to_move":"white","clock":{"black_main_ms":59000,"white_main_ms":59000,"black_byoyomi_ms":30000,"white_byoyomi_ms":30000,"black_increment_ms":0,"white_increment_ms":0,"per_move_limit_ms":null}})"
        << '\n';
    return out.str();
}

std::string aux_jsonl(std::string_view id) {
    std::ostringstream out;
    out
        << R"({"schema":"kadoka.game_aux","version":1,"game_id":")"
        << id
        << R"(","ply":0,"event_index":0,"event_type":"action","actor":"black","action":{"type":"move","move":"7g7e"},"result":{"status":"illegal","reason":"illegal_move"},"terminal":null})"
        << '\n'
        << R"({"schema":"kadoka.game_aux","version":1,"game_id":")"
        << id
        << R"(","ply":0,"event_index":1,"event_type":"action","actor":"black","action":{"type":"move","move":"7g7f"},"result":{"status":"accepted","reason":null},"terminal":null})"
        << '\n'
        << R"({"schema":"kadoka.game_aux","version":1,"game_id":")"
        << id
        << R"(","ply":1,"event_index":2,"event_type":"action","actor":"white","action":{"type":"resign"},"result":{"status":"accepted","reason":null},"terminal":null})"
        << '\n'
        << R"({"schema":"kadoka.game_aux","version":1,"game_id":")"
        << id
        << R"(","ply":1,"event_index":3,"event_type":"terminal","actor":null,"action":null,"result":null,"terminal":{"result":"black_win","reason":"resignation"}})"
        << '\n';
    return out.str();
}

struct TempTree {
    explicit TempTree(std::string_view label) {
        static std::size_t sequence = 0;
        const auto stamp =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
        root =
            std::filesystem::temp_directory_path()
            / (
                "kadoka_recursive_import_"
                + std::string(label)
                + "_"
                + std::to_string(stamp)
                + "_"
                + std::to_string(sequence++)
            );
        std::filesystem::create_directories(root);
    }

    ~TempTree() {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    std::filesystem::path root{};
};

void write_text(
    const std::filesystem::path& path,
    std::string_view content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    assert(output);
    output.write(
        content.data(),
        static_cast<std::streamsize>(content.size())
    );
    assert(output);
}

bool has_diagnostic(
    const ImportSummary& summary,
    ImportDiagnosticCode code) {
    for (const ImportDiagnostic& diagnostic : summary.diagnostics) {
        if (diagnostic.code == code) return true;
    }
    return false;
}

std::size_t count_diagnostic(
    const ImportSummary& summary,
    ImportDiagnosticCode code) {
    std::size_t count = 0;
    for (const ImportDiagnostic& diagnostic : summary.diagnostics) {
        if (diagnostic.code == code) ++count;
    }
    return count;
}

class CountingSink final : public ParsedRecordSink {
public:
    void on_record(ParsedRecord record) override {
        ++records;
        if (record.game_id.has_value() && first_game_id.empty()) {
            first_game_id = *record.game_id;
        }
    }

    void on_error(ParseError) override {
        ++errors;
    }

    std::size_t records{0};
    std::size_t errors{0};
    std::string first_game_id{};
};

void write_large_board(
    const std::filesystem::path& path,
    std::string_view id,
    std::size_t record_count) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    assert(out);

    for (std::size_t index = 0; index < record_count; ++index) {
        out
            << R"({"schema":"kadoka.board_state","version":1,"game_id":")"
            << id
            << R"(","ply":)"
            << index
            << R"(,"sfen":")"
            << kStartSfen
            << R"(","side_to_move":"black","clock":{"black_main_ms":null,"white_main_ms":null,"black_byoyomi_ms":null,"white_byoyomi_ms":null,"black_increment_ms":null,"white_increment_ms":null,"per_move_limit_ms":null}})"
            << '\n';
    }
    assert(out);
}

void write_large_aux(
    const std::filesystem::path& path,
    std::string_view id) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out
        << R"({"schema":"kadoka.game_aux","version":1,"game_id":")"
        << id
        << R"(","ply":0,"event_index":0,"event_type":"action","actor":"black","action":{"type":"move","move":"7g7e"},"result":{"status":"illegal","reason":"illegal_move"},"terminal":null})"
        << '\n';
    assert(out);
}

} // namespace

int main() {
    const ParserRegistry registry = make_reference_parser_registry();
    const RecursiveTrainingImporter importer{registry};

    {
        TempTree tree{"single"};
        const std::string id = game_id(1);

        write_text(
            tree.root / "game-001" / "strange-board.JSONL",
            board_jsonl(id)
        );
        write_text(
            tree.root / "game-001" / "anything.json",
            aux_jsonl(id)
        );
        write_text(
            tree.root / "game-001" / "metadata.json",
            R"({"kind":"not-a-kadoka-history"})"
        );
        write_text(
            tree.root / "game-001" / "README.txt",
            "ignored because it is not JSON/JSONL\n"
        );

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(tree.root, sink);

        assert(summary.detected_files == 3);
        assert(summary.imported_games == 1);
        assert(summary.imported_records == 2);
        assert(summary.skipped_files == 1);
        assert(summary.errors == 0);
        assert(summary.warnings == 1);
        assert(
            count_diagnostic(
                summary,
                ImportDiagnosticCode::UnsupportedFile
            ) == 1
        );

        assert(sink.errors().empty());
        assert(sink.records().size() == 2);
        assert(sink.records()[0].game_id == id);
        assert(sink.records()[0].ply == 0);
        assert(
            sink.records()[0].provenance.locations[0].source_name
            == "game-001/strange-board.JSONL"
        );
        assert(
            sink.records()[0].provenance.locations[1].source_name
            == "game-001/anything.json"
        );
    }

    {
        TempTree tree{"nested"};
        const std::string first_id = game_id(11);
        const std::string second_id = game_id(12);
        const std::string missing_id = game_id(13);

        write_text(
            tree.root / "zeta" / "game" / "board_state.jsonl",
            board_jsonl(second_id)
        );
        write_text(
            tree.root / "zeta" / "game" / "game_aux.jsonl",
            aux_jsonl(second_id)
        );
        write_text(
            tree.root / "alpha" / "deep" / "game" / "x.jsonl",
            board_jsonl(first_id)
        );
        write_text(
            tree.root / "alpha" / "deep" / "game" / "y.jsonl",
            aux_jsonl(first_id)
        );
        write_text(
            tree.root / "middle" / "missing" / "board_state.jsonl",
            board_jsonl(missing_id)
        );
        write_text(
            tree.root / "middle" / "unrelated.json",
            R"({"schema":"other.project","version":1})"
        );

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(tree.root, sink);

        assert(summary.detected_files == 6);
        assert(summary.imported_games == 2);
        assert(summary.imported_records == 4);
        assert(summary.skipped_files == 2);
        assert(summary.errors == 0);
        assert(summary.warnings == 2);
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::MissingPair
            )
        );
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::UnsupportedFile
            )
        );

        assert(sink.records().size() == 4);
        assert(sink.records()[0].game_id == first_id);
        assert(sink.records()[1].game_id == first_id);
        assert(sink.records()[2].game_id == second_id);
        assert(sink.records()[3].game_id == second_id);
    }

    {
        TempTree tree{"duplicate"};
        const std::string id = game_id(20);

        write_text(
            tree.root / "a" / "board.jsonl",
            board_jsonl(id)
        );
        write_text(
            tree.root / "a" / "aux.jsonl",
            aux_jsonl(id)
        );
        write_text(
            tree.root / "b" / "board.jsonl",
            board_jsonl(id)
        );
        write_text(
            tree.root / "b" / "aux.jsonl",
            aux_jsonl(id)
        );

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(tree.root, sink);

        assert(summary.detected_files == 4);
        assert(summary.imported_games == 0);
        assert(summary.imported_records == 0);
        assert(summary.skipped_files == 4);
        assert(summary.errors == 2);
        assert(summary.warnings == 0);
        assert(
            count_diagnostic(
                summary,
                ImportDiagnosticCode::DuplicateGameId
            ) == 2
        );
        assert(sink.records().empty());
    }

    {
        TempTree tree{"malformed"};
        const std::string good_id = game_id(30);
        const std::string bad_id = game_id(31);

        write_text(
            tree.root / "good" / "board.jsonl",
            board_jsonl(good_id)
        );
        write_text(
            tree.root / "good" / "aux.jsonl",
            aux_jsonl(good_id)
        );

        write_text(
            tree.root / "bad" / "board.jsonl",
            R"({"schema":"kadoka.board_state","version":1,"game_id":")"
                + bad_id
                + R"(","ply":0,"broken":true})"
                + "\n"
        );
        write_text(
            tree.root / "bad" / "aux.jsonl",
            aux_jsonl(bad_id)
        );

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(tree.root, sink);

        assert(summary.imported_games == 1);
        assert(summary.imported_records == 2);
        assert(summary.skipped_files == 2);
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::MalformedCoreCandidate
            )
        );
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::MissingPair
            )
        );
        assert(sink.records().size() == 2);
        assert(sink.records()[0].game_id == good_id);
    }

    {
        TempTree tree{"broken_late"};
        const std::string good_id = game_id(40);
        const std::string bad_id = game_id(41);

        write_text(
            tree.root / "good" / "board.jsonl",
            board_jsonl(good_id)
        );
        write_text(
            tree.root / "good" / "aux.jsonl",
            aux_jsonl(good_id)
        );

        std::string late_bad = board_jsonl(bad_id);
        late_bad +=
            R"({"schema":"kadoka.board_state","version":1,"game_id":")"
            + bad_id
            + R"(","ply":99,"sfen":")"
            + std::string(kStartSfen)
            + R"(","side_to_move":"black","clock":{"black_main_ms":null,"white_main_ms":null,"black_byoyomi_ms":null,"white_byoyomi_ms":null,"black_increment_ms":null,"white_increment_ms":null,"per_move_limit_ms":null}})"
            + "\n";
        write_text(
            tree.root / "bad" / "board.jsonl",
            late_bad
        );
        write_text(
            tree.root / "bad" / "aux.jsonl",
            aux_jsonl(bad_id)
        );

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(tree.root, sink);

        assert(summary.imported_games == 1);
        assert(summary.imported_records == 2);
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::ParserFailed
            )
        );

        // Preflight prevents records from the broken game from leaking into
        // the caller sink even though its first two BoardState rows were valid.
        assert(sink.records().size() == 2);
        assert(sink.records()[0].game_id == good_id);
        assert(sink.records()[1].game_id == good_id);
        assert(!sink.errors().empty());
    }

    {
        TempTree tree{"large"};
        const std::string id = game_id(50);
        constexpr std::size_t kRecordCount = 3000;

        write_large_board(
            tree.root / "large-game" / "board_state.jsonl",
            id,
            kRecordCount
        );
        write_large_aux(
            tree.root / "large-game" / "game_aux.jsonl",
            id
        );

        CountingSink sink;
        const ImportSummary summary =
            importer.import_folder(tree.root, sink);

        assert(summary.detected_files == 2);
        assert(summary.imported_games == 1);
        assert(summary.imported_records == kRecordCount);
        assert(summary.skipped_files == 0);
        assert(summary.errors == 0);
        assert(summary.warnings == 0);
        assert(sink.records == kRecordCount);
        assert(sink.errors == 0);
        assert(sink.first_game_id == id);
    }

    {
        TempTree tree{"missing_root"};
        const std::filesystem::path missing =
            tree.root / "does-not-exist";

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(missing, sink);

        assert(summary.imported_games == 0);
        assert(summary.imported_records == 0);
        assert(summary.errors == 1);
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::RootNotFound
            )
        );
    }

    {
        TempTree tree{"not_directory"};
        const std::filesystem::path file =
            tree.root / "file.json";
        write_text(file, "{}");

        CollectingParsedRecordSink sink;
        const ImportSummary summary =
            importer.import_folder(file, sink);

        assert(summary.errors == 1);
        assert(
            has_diagnostic(
                summary,
                ImportDiagnosticCode::RootNotDirectory
            )
        );
    }

    return 0;
}
