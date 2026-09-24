#include "kadoka/training/recursive_importer.hpp"

#include "kadoka/training/core_history_parser.hpp"
#include "kadoka/runtime/game_history.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <stdexcept>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace kadoka::shogi::training {
namespace {

enum class CandidateKind : std::uint8_t {
    BoardState,
    GameAux,
};

struct CandidateFile {
    std::filesystem::path absolute_path{};
    std::string relative_path{};
    std::string directory{};
    std::string game_id{};
    CandidateKind kind{CandidateKind::BoardState};
};

struct GroupKey {
    std::string directory{};
    std::string game_id{};

    friend bool operator<(const GroupKey& lhs, const GroupKey& rhs) {
        return std::tie(lhs.directory, lhs.game_id)
            < std::tie(rhs.directory, rhs.game_id);
    }
};

struct PairGroup {
    GroupKey key{};
    std::optional<CandidateFile> board{};
    std::optional<CandidateFile> aux{};
    bool ambiguous{false};
};

struct ClassifiedLine {
    CandidateKind kind{CandidateKind::BoardState};
    std::string game_id{};
};

std::string lower_ascii(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        }
    );
    return value;
}

bool is_candidate_extension(const std::filesystem::path& path) {
    const std::string extension =
        lower_ascii(path.extension().string());
    return extension == ".json" || extension == ".jsonl";
}

bool is_blank(std::string_view text) {
    return std::all_of(
        text.begin(),
        text.end(),
        [](char ch) {
            return std::isspace(
                static_cast<unsigned char>(ch)
            ) != 0;
        }
    );
}

std::optional<std::string> first_data_line(
    const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;

    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        if (first_line) {
            first_line = false;
            if (line.size() >= 3
                && static_cast<unsigned char>(line[0]) == 0xEFU
                && static_cast<unsigned char>(line[1]) == 0xBBU
                && static_cast<unsigned char>(line[2]) == 0xBFU) {
                line.erase(0, 3);
            }
        }
        if (!is_blank(line)) return line;
    }
    return std::nullopt;
}

bool looks_like_core_schema(std::string_view line) {
    return line.find("kadoka.board_state") != std::string_view::npos
        || line.find("kadoka.game_aux") != std::string_view::npos;
}

std::optional<ClassifiedLine> classify_core_line(
    std::string_view line) {
    try {
        const runtime::BoardStateRecord board =
            runtime::deserialize_board_state_record(line);
        return ClassifiedLine{
            CandidateKind::BoardState,
            board.game_id,
        };
    } catch (const std::invalid_argument&) {
    }

    try {
        const runtime::GameAuxRecord aux =
            runtime::deserialize_game_aux_record(line);
        return ClassifiedLine{
            CandidateKind::GameAux,
            aux.game_id,
        };
    } catch (const std::invalid_argument&) {
    }

    return std::nullopt;
}

std::string relative_generic(
    const std::filesystem::path& path,
    const std::filesystem::path& root) {
    const std::filesystem::path relative =
        path.lexically_relative(root);
    if (relative.empty()) return path.filename().generic_string();
    return relative.generic_string();
}

std::string parent_relative(
    const std::filesystem::path& path,
    const std::filesystem::path& root) {
    const std::filesystem::path parent =
        path.parent_path().lexically_relative(root);
    if (parent.empty() || parent == ".") return ".";
    return parent.generic_string();
}

void add_diagnostic(
    ImportSummary& summary,
    ImportDiagnosticSeverity severity,
    ImportDiagnosticCode code,
    std::string path,
    std::string game_id,
    std::string message) {
    if (severity == ImportDiagnosticSeverity::Warning) {
        ++summary.warnings;
    } else {
        ++summary.errors;
    }
    summary.diagnostics.push_back(ImportDiagnostic{
        severity,
        code,
        std::move(path),
        std::move(game_id),
        std::move(message),
    });
}

class PreflightSink final : public ParsedRecordSink {
public:
    explicit PreflightSink(ParsedRecordSink& downstream)
        : downstream_(&downstream) {}

    void on_record(ParsedRecord) override {
        ++records_;
    }

    void on_error(ParseError error) override {
        errors_.push_back(error);
        downstream_->on_error(std::move(error));
    }

    [[nodiscard]] std::size_t records() const noexcept {
        return records_;
    }

    [[nodiscard]] const std::vector<ParseError>& errors()
        const noexcept {
        return errors_;
    }

private:
    ParsedRecordSink* downstream_{nullptr};
    std::size_t records_{0};
    std::vector<ParseError> errors_{};
};

class ForwardCountingSink final : public ParsedRecordSink {
public:
    explicit ForwardCountingSink(ParsedRecordSink& downstream)
        : downstream_(&downstream) {}

    void on_record(ParsedRecord record) override {
        ++records_;
        downstream_->on_record(std::move(record));
    }

    void on_error(ParseError error) override {
        errors_.push_back(error);
        downstream_->on_error(std::move(error));
    }

    [[nodiscard]] std::size_t records() const noexcept {
        return records_;
    }

    [[nodiscard]] const std::vector<ParseError>& errors()
        const noexcept {
        return errors_;
    }

private:
    ParsedRecordSink* downstream_{nullptr};
    std::size_t records_{0};
    std::vector<ParseError> errors_{};
};

struct OpenPair {
    std::ifstream board{};
    std::ifstream aux{};
    std::array<ParserSource, 2> sources{};
};

std::optional<OpenPair> open_pair(
    const PairGroup& group,
    ImportSummary& summary) {
    OpenPair opened;
    opened.board.open(
        group.board->absolute_path,
        std::ios::binary
    );
    if (!opened.board) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::FileOpenFailed,
            group.board->relative_path,
            group.key.game_id,
            "failed to open BoardState source"
        );
        return std::nullopt;
    }

    opened.aux.open(
        group.aux->absolute_path,
        std::ios::binary
    );
    if (!opened.aux) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::FileOpenFailed,
            group.aux->relative_path,
            group.key.game_id,
            "failed to open GameAux source"
        );
        return std::nullopt;
    }

    opened.sources = {
        ParserSource{
            group.board->relative_path,
            &opened.board,
        },
        ParserSource{
            group.aux->relative_path,
            &opened.aux,
        },
    };
    return std::optional<OpenPair>{std::move(opened)};
}

void report_parser_failure(
    ImportSummary& summary,
    const PairGroup& group,
    const std::vector<ParseError>& errors,
    std::string_view phase) {
    if (errors.empty()) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::ParserFailed,
            group.key.directory,
            group.key.game_id,
            std::string("Core history parser did not complete during ")
                + std::string(phase)
        );
        return;
    }

    for (const ParseError& error : errors) {
        std::string location = error.source_name;
        if (error.line.has_value()) {
            location += ":" + std::to_string(*error.line);
        }
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::ParserFailed,
            std::move(location),
            group.key.game_id,
            std::string(phase)
                + ": "
                + error.message
        );
    }
}

} // namespace

ImportSummary RecursiveTrainingImporter::import_folder(
    const std::filesystem::path& root,
    ParsedRecordSink& sink) const {
    ImportSummary summary;

    std::error_code ec;
    const std::filesystem::path root_absolute =
        std::filesystem::absolute(root, ec).lexically_normal();
    if (ec || !std::filesystem::exists(root_absolute, ec)) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::RootNotFound,
            root.generic_string(),
            {},
            "training-data root does not exist"
        );
        return summary;
    }
    if (ec || !std::filesystem::is_directory(root_absolute, ec)) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::RootNotDirectory,
            root.generic_string(),
            {},
            "training-data root is not a directory"
        );
        return summary;
    }

    const Parser* core_parser =
        registry_ == nullptr
            ? nullptr
            : registry_->find(kCoreHistoryParserId);
    if (core_parser == nullptr) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::MissingParser,
            {},
            {},
            "ParserRegistry does not contain kadoka.core_history.v1"
        );
        return summary;
    }

    std::vector<std::filesystem::path> candidate_paths;
    std::filesystem::recursive_directory_iterator iterator{
        root_absolute,
        std::filesystem::directory_options::skip_permission_denied,
        ec
    };
    const std::filesystem::recursive_directory_iterator end{};

    if (ec) {
        add_diagnostic(
            summary,
            ImportDiagnosticSeverity::Error,
            ImportDiagnosticCode::ScanFailed,
            root_absolute.generic_string(),
            {},
            "failed to start recursive directory scan: "
                + ec.message()
        );
        return summary;
    }

    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        std::error_code entry_ec;

        if (entry.is_symlink(entry_ec)) {
            // Do not follow file or directory symlinks. This keeps traversal
            // inside the selected root and avoids duplicate/loop surprises.
        } else if (!entry_ec
                   && entry.is_regular_file(entry_ec)
                   && !entry_ec
                   && is_candidate_extension(entry.path())) {
            candidate_paths.push_back(
                entry.path().lexically_normal()
            );
        }

        iterator.increment(ec);
        if (ec) {
            add_diagnostic(
                summary,
                ImportDiagnosticSeverity::Warning,
                ImportDiagnosticCode::ScanFailed,
                relative_generic(entry.path(), root_absolute),
                {},
                "directory entry could not be scanned: "
                    + ec.message()
            );
            ec.clear();
        }
    }

    std::sort(
        candidate_paths.begin(),
        candidate_paths.end(),
        [&](const auto& lhs, const auto& rhs) {
            return relative_generic(lhs, root_absolute)
                < relative_generic(rhs, root_absolute);
        }
    );

    summary.detected_files = candidate_paths.size();

    std::map<GroupKey, PairGroup> groups;

    for (const std::filesystem::path& path : candidate_paths) {
        const std::string relative =
            relative_generic(path, root_absolute);

        std::ifstream probe(path, std::ios::binary);
        if (!probe) {
            add_diagnostic(
                summary,
                ImportDiagnosticSeverity::Error,
                ImportDiagnosticCode::FileOpenFailed,
                relative,
                {},
                "failed to open candidate file"
            );
            continue;
        }
        probe.close();

        const std::optional<std::string> line =
            first_data_line(path);
        if (!line.has_value()) {
            add_diagnostic(
                summary,
                ImportDiagnosticSeverity::Warning,
                ImportDiagnosticCode::UnsupportedFile,
                relative,
                {},
                "empty JSON/JSONL candidate skipped"
            );
            continue;
        }

        const std::optional<ClassifiedLine> classified =
            classify_core_line(*line);
        if (!classified.has_value()) {
            if (looks_like_core_schema(*line)) {
                add_diagnostic(
                    summary,
                    ImportDiagnosticSeverity::Error,
                    ImportDiagnosticCode::MalformedCoreCandidate,
                    relative,
                    {},
                    "file declares a Kadoka Core history schema "
                    "but the first record is malformed"
                );
            } else {
                add_diagnostic(
                    summary,
                    ImportDiagnosticSeverity::Warning,
                    ImportDiagnosticCode::UnsupportedFile,
                    relative,
                    {},
                    "unsupported JSON/JSONL file skipped"
                );
            }
            continue;
        }

        const GroupKey key{
            parent_relative(path, root_absolute),
            classified->game_id,
        };
        PairGroup& group = groups[key];
        group.key = key;

        CandidateFile candidate{
            path,
            relative,
            key.directory,
            key.game_id,
            classified->kind,
        };

        std::optional<CandidateFile>* slot =
            classified->kind == CandidateKind::BoardState
                ? &group.board
                : &group.aux;

        if (slot->has_value()) {
            group.ambiguous = true;
            add_diagnostic(
                summary,
                ImportDiagnosticSeverity::Error,
                ImportDiagnosticCode::DuplicateRole,
                relative,
                key.game_id,
                classified->kind == CandidateKind::BoardState
                    ? "multiple BoardState sources for one game"
                    : "multiple GameAux sources for one game"
            );
        } else {
            *slot = std::move(candidate);
        }
    }

    std::map<std::string, std::vector<const PairGroup*>>
        complete_by_game_id;

    for (const auto& [_, group] : groups) {
        if (group.ambiguous) continue;

        if (!group.board.has_value() || !group.aux.has_value()) {
            const std::string path =
                group.board.has_value()
                    ? group.board->relative_path
                    : group.aux.has_value()
                        ? group.aux->relative_path
                        : group.key.directory;
            add_diagnostic(
                summary,
                ImportDiagnosticSeverity::Warning,
                ImportDiagnosticCode::MissingPair,
                path,
                group.key.game_id,
                group.board.has_value()
                    ? "BoardState source has no matching GameAux source"
                    : "GameAux source has no matching BoardState source"
            );
            continue;
        }

        complete_by_game_id[group.key.game_id].push_back(&group);
    }

    std::set<GroupKey> duplicate_groups;
    for (const auto& [game_id, matching_groups] : complete_by_game_id) {
        if (matching_groups.size() <= 1) continue;

        for (const PairGroup* group : matching_groups) {
            duplicate_groups.insert(group->key);
            add_diagnostic(
                summary,
                ImportDiagnosticSeverity::Error,
                ImportDiagnosticCode::DuplicateGameId,
                group->key.directory,
                game_id,
                "duplicate game_id found in multiple game folders"
            );
        }
    }

    std::size_t imported_files = 0;

    for (const auto& [key, group] : groups) {
        if (group.ambiguous
            || !group.board.has_value()
            || !group.aux.has_value()
            || duplicate_groups.contains(key)) {
            continue;
        }

        std::optional<OpenPair> preflight =
            open_pair(group, summary);
        if (!preflight.has_value()) continue;

        PreflightSink preflight_sink{sink};
        const ParseSummary preflight_summary = core_parser->parse(
            preflight->sources,
            preflight_sink
        );
        if (!preflight_summary.completed
            || preflight_summary.errors_emitted != 0
            || !preflight_sink.errors().empty()) {
            report_parser_failure(
                summary,
                group,
                preflight_sink.errors(),
                "preflight"
            );
            continue;
        }

        std::optional<OpenPair> actual =
            open_pair(group, summary);
        if (!actual.has_value()) continue;

        ForwardCountingSink forward_sink{sink};
        const ParseSummary parse_summary = core_parser->parse(
            actual->sources,
            forward_sink
        );
        if (!parse_summary.completed
            || parse_summary.errors_emitted != 0
            || !forward_sink.errors().empty()) {
            report_parser_failure(
                summary,
                group,
                forward_sink.errors(),
                "import"
            );
            continue;
        }

        ++summary.imported_games;
        summary.imported_records += forward_sink.records();
        imported_files += 2;
    }

    summary.skipped_files =
        summary.detected_files >= imported_files
            ? summary.detected_files - imported_files
            : 0;

    return summary;
}

} // namespace kadoka::shogi::training
