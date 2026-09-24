#include "kadoka/training/sfen_lines_parser.hpp"

#include "kadoka/position.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <istream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace kadoka::shogi::training {
namespace {

std::string_view trim(std::string_view value) {
    while (!value.empty()
           && std::isspace(
               static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty()
           && std::isspace(
               static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

void emit_error(
    ParsedRecordSink& sink,
    ParseSummary& summary,
    ParseErrorCode code,
    std::string_view source_name,
    std::optional<std::uint64_t> line,
    std::string message,
    bool recoverable) {
    sink.on_error(ParseError{
        code,
        std::string(kSfenLinesParserId),
        std::string(source_name),
        line,
        std::move(message),
        recoverable,
    });
    ++summary.errors_emitted;
}

} // namespace

ParseSummary SfenLinesParser::parse(
    std::span<const ParserSource> sources,
    ParsedRecordSink& sink) const {
    ParseSummary summary;

    if (sources.size() != 1) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::InvalidInputCount,
            {},
            std::nullopt,
            "SFEN-lines parser requires exactly one source",
            false
        );
        return summary;
    }
    if (sources[0].stream == nullptr) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::UnsupportedInput,
            sources[0].name,
            std::nullopt,
            "parser source stream must not be null",
            false
        );
        return summary;
    }
    if (sources[0].name.empty()) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::UnsupportedInput,
            {},
            std::nullopt,
            "parser source name must not be empty",
            false
        );
        return summary;
    }

    std::string line;
    std::uint64_t line_number = 0;
    while (std::getline(*sources[0].stream, line)) {
        ++line_number;
        const std::string_view value = trim(line);
        if (value.empty() || value.front() == '#') {
            continue;
        }

        try {
            const Position position = Position::from_sfen(value);
            ParsedRecord record;
            record.sfen = position.to_sfen();
            record.side_to_move = position.side_to_move();
            record.provenance.parser_id =
                std::string(kSfenLinesParserId);
            record.provenance.locations.push_back(SourceLocation{
                sources[0].name,
                line_number,
            });
            sink.on_record(std::move(record));
            ++summary.records_emitted;
        } catch (const std::invalid_argument& error) {
            emit_error(
                sink,
                summary,
                ParseErrorCode::MalformedRecord,
                sources[0].name,
                line_number,
                error.what(),
                true
            );
        }
    }

    if (!sources[0].stream->eof()
        && sources[0].stream->fail()) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::InternalError,
            sources[0].name,
            line_number,
            "failed while reading SFEN source",
            false
        );
        return summary;
    }

    summary.completed = true;
    return summary;
}

} // namespace kadoka::shogi::training
