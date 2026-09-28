#pragma once

#include "kadoka/training/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace kadoka::shogi::training {

enum class ImportDiagnosticSeverity : std::uint8_t {
    Warning,
    Error,
};

enum class ImportDiagnosticCode : std::uint8_t {
    RootNotFound,
    RootNotDirectory,
    ScanFailed,
    FileOpenFailed,
    UnsupportedFile,
    MalformedCoreCandidate,
    MissingPair,
    DuplicateRole,
    DuplicateGameId,
    MissingParser,
    ParserFailed,
};

struct ImportDiagnostic {
    ImportDiagnosticSeverity severity{
        ImportDiagnosticSeverity::Warning
    };
    ImportDiagnosticCode code{ImportDiagnosticCode::UnsupportedFile};
    std::string path{};
    std::string game_id{};
    std::string message{};
};

struct ImportSummary {
    std::size_t detected_files{0};
    std::size_t imported_games{0};
    std::size_t imported_records{0};
    std::size_t skipped_files{0};
    std::size_t warnings{0};
    std::size_t errors{0};
    std::vector<ImportDiagnostic> diagnostics{};
};

class RecursiveTrainingImporter {
public:
    explicit RecursiveTrainingImporter(const ParserRegistry& registry)
        : registry_(&registry) {}

    [[nodiscard]] ImportSummary import_folder(
        const std::filesystem::path& root,
        ParsedRecordSink& sink
    ) const;

private:
    const ParserRegistry* registry_{nullptr};
};

} // namespace kadoka::shogi::training
