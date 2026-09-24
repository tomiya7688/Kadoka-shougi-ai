#include "kadoka/training/core_history_parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <istream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace kadoka::shogi::training {
namespace {

struct LineRecord {
    std::string text{};
    std::uint64_t line{0};
};

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

class StreamCursor {
public:
    explicit StreamCursor(const ParserSource& source)
        : source_(source) {}

    [[nodiscard]] const ParserSource& source() const noexcept {
        return source_;
    }

    [[nodiscard]] const std::optional<LineRecord>& peek() {
        fill();
        return buffered_;
    }

    [[nodiscard]] std::optional<LineRecord> pop() {
        fill();
        std::optional<LineRecord> result =
            std::move(buffered_);
        buffered_.reset();
        return result;
    }

private:
    void fill() {
        if (buffered_.has_value() || exhausted_) {
            return;
        }

        std::string line;
        while (std::getline(*source_.stream, line)) {
            ++line_number_;
            if (is_blank(line)) {
                continue;
            }
            buffered_ = LineRecord{
                std::move(line),
                line_number_,
            };
            return;
        }

        if (!source_.stream->eof() && source_.stream->fail()) {
            throw std::runtime_error(
                "failed reading source: " + source_.name
            );
        }
        exhausted_ = true;
    }

    ParserSource source_{};
    std::uint64_t line_number_{0};
    std::optional<LineRecord> buffered_{};
    bool exhausted_{false};
};

enum class CoreSourceKind {
    BoardState,
    GameAux,
};

CoreSourceKind classify_line(const LineRecord& line) {
    try {
        (void)runtime::deserialize_board_state_record(line.text);
        return CoreSourceKind::BoardState;
    } catch (const std::invalid_argument&) {
    }

    try {
        (void)runtime::deserialize_game_aux_record(line.text);
        return CoreSourceKind::GameAux;
    } catch (const std::invalid_argument&) {
    }

    throw std::invalid_argument(
        "first data line is neither kadoka.board_state "
        "nor kadoka.game_aux schema v1"
    );
}

void emit_error(
    ParsedRecordSink& sink,
    ParseSummary& summary,
    ParseErrorCode code,
    std::string_view source_name,
    std::optional<std::uint64_t> line,
    std::string message,
    bool recoverable = false) {
    sink.on_error(ParseError{
        code,
        std::string(kCoreHistoryParserId),
        std::string(source_name),
        line,
        std::move(message),
        recoverable,
    });
    ++summary.errors_emitted;
}

ParsedEvent convert_event(
    const runtime::GameAuxRecord& event,
    const SourceLocation& location) {
    return ParsedEvent{
        event.event_index,
        event.event_type == runtime::GameAuxEventType::Action
            ? ParsedEventType::Action
            : ParsedEventType::Terminal,
        event.actor,
        event.action,
        event.result,
        event.terminal,
        location,
    };
}

bool same_source_name(
    const ParserSource& lhs,
    const ParserSource& rhs) {
    return lhs.name == rhs.name;
}

} // namespace

ParseSummary CoreHistoryParser::parse(
    std::span<const ParserSource> sources,
    ParsedRecordSink& sink) const {
    ParseSummary summary;

    if (sources.size() != 2) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::InvalidInputCount,
            {},
            std::nullopt,
            "Core history parser requires exactly two sources"
        );
        return summary;
    }

    if (sources[0].stream == nullptr
        || sources[1].stream == nullptr) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::UnsupportedInput,
            {},
            std::nullopt,
            "parser source stream must not be null"
        );
        return summary;
    }

    if (sources[0].name.empty() || sources[1].name.empty()) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::UnsupportedInput,
            {},
            std::nullopt,
            "parser source name must not be empty"
        );
        return summary;
    }

    if (same_source_name(sources[0], sources[1])) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::DuplicateSourceName,
            sources[0].name,
            std::nullopt,
            "Core history source names must be unique"
        );
        return summary;
    }

    try {
        StreamCursor first{sources[0]};
        StreamCursor second{sources[1]};

        const auto& first_line = first.peek();
        const auto& second_line = second.peek();

        if (!first_line.has_value() || !second_line.has_value()) {
            const ParserSource& empty_source =
                !first_line.has_value()
                    ? first.source()
                    : second.source();
            emit_error(
                sink,
                summary,
                ParseErrorCode::UnsupportedInput,
                empty_source.name,
                std::nullopt,
                "Core history source must contain at least one record"
            );
            return summary;
        }

        CoreSourceKind first_kind;
        CoreSourceKind second_kind;
        try {
            first_kind = classify_line(*first_line);
        } catch (const std::invalid_argument& error) {
            emit_error(
                sink,
                summary,
                ParseErrorCode::MalformedRecord,
                first.source().name,
                first_line->line,
                error.what()
            );
            return summary;
        }
        try {
            second_kind = classify_line(*second_line);
        } catch (const std::invalid_argument& error) {
            emit_error(
                sink,
                summary,
                ParseErrorCode::MalformedRecord,
                second.source().name,
                second_line->line,
                error.what()
            );
            return summary;
        }

        if (first_kind == second_kind) {
            emit_error(
                sink,
                summary,
                ParseErrorCode::UnsupportedInput,
                {},
                std::nullopt,
                "Core history parser requires one BoardState "
                "source and one GameAux source"
            );
            return summary;
        }

        StreamCursor* board_cursor =
            first_kind == CoreSourceKind::BoardState
                ? &first
                : &second;
        StreamCursor* aux_cursor =
            first_kind == CoreSourceKind::GameAux
                ? &first
                : &second;

        std::optional<std::string> expected_game_id;
        std::uint64_t expected_ply = 0;
        std::uint64_t expected_event_index = 0;
        bool saw_board = false;
        bool terminal_seen = false;

        while (board_cursor->peek().has_value()) {
            if (terminal_seen) {
                const LineRecord& extra = *board_cursor->peek();
                emit_error(
                    sink,
                    summary,
                    ParseErrorCode::OrderingViolation,
                    board_cursor->source().name,
                    extra.line,
                    "BoardState record exists after terminal event"
                );
                return summary;
            }

            LineRecord board_line =
                std::move(*board_cursor->pop());
            runtime::BoardStateRecord board;
            try {
                board = runtime::deserialize_board_state_record(
                    board_line.text
                );
            } catch (const std::invalid_argument& error) {
                emit_error(
                    sink,
                    summary,
                    ParseErrorCode::MalformedRecord,
                    board_cursor->source().name,
                    board_line.line,
                    error.what()
                );
                return summary;
            }

            if (!expected_game_id.has_value()) {
                expected_game_id = board.game_id;
            } else if (board.game_id != *expected_game_id) {
                emit_error(
                    sink,
                    summary,
                    ParseErrorCode::JoinMismatch,
                    board_cursor->source().name,
                    board_line.line,
                    "BoardState game_id changed within one parse pair"
                );
                return summary;
            }

            if (board.ply != expected_ply) {
                emit_error(
                    sink,
                    summary,
                    ParseErrorCode::OrderingViolation,
                    board_cursor->source().name,
                    board_line.line,
                    "BoardState ply must start at 0 and increase by 1"
                );
                return summary;
            }

            saw_board = true;
            ParsedRecord record;
            record.game_id = board.game_id;
            record.ply = board.ply;
            record.sfen = board.sfen;
            record.side_to_move = board.side_to_move;
            record.clock = board.clock;
            record.provenance.parser_id =
                std::string(kCoreHistoryParserId);
            record.provenance.locations.push_back(SourceLocation{
                board_cursor->source().name,
                board_line.line,
            });

            while (aux_cursor->peek().has_value()) {
                const LineRecord& pending = *aux_cursor->peek();
                runtime::GameAuxRecord event;
                try {
                    event = runtime::deserialize_game_aux_record(
                        pending.text
                    );
                } catch (const std::invalid_argument& error) {
                    emit_error(
                        sink,
                        summary,
                        ParseErrorCode::MalformedRecord,
                        aux_cursor->source().name,
                        pending.line,
                        error.what()
                    );
                    return summary;
                }

                if (event.game_id != board.game_id) {
                    emit_error(
                        sink,
                        summary,
                        ParseErrorCode::JoinMismatch,
                        aux_cursor->source().name,
                        pending.line,
                        "GameAux game_id does not match BoardState"
                    );
                    return summary;
                }

                if (event.event_index != expected_event_index) {
                    emit_error(
                        sink,
                        summary,
                        ParseErrorCode::OrderingViolation,
                        aux_cursor->source().name,
                        pending.line,
                        "GameAux event_index must start at 0 "
                        "and increase by 1"
                    );
                    return summary;
                }

                if (event.ply < board.ply) {
                    emit_error(
                        sink,
                        summary,
                        ParseErrorCode::JoinMismatch,
                        aux_cursor->source().name,
                        pending.line,
                        "GameAux event refers to an already-emitted ply"
                    );
                    return summary;
                }
                if (event.ply > board.ply) {
                    break;
                }

                LineRecord consumed =
                    std::move(*aux_cursor->pop());
                const SourceLocation location{
                    aux_cursor->source().name,
                    consumed.line,
                };
                record.events.push_back(
                    convert_event(event, location)
                );
                record.provenance.locations.push_back(location);
                ++expected_event_index;

                if (event.event_type
                    == runtime::GameAuxEventType::Terminal) {
                    terminal_seen = true;
                    if (aux_cursor->peek().has_value()) {
                        const LineRecord& after_terminal =
                            *aux_cursor->peek();
                        emit_error(
                            sink,
                            summary,
                            ParseErrorCode::OrderingViolation,
                            aux_cursor->source().name,
                            after_terminal.line,
                            "GameAux event exists after terminal event"
                        );
                        return summary;
                    }
                }
            }

            sink.on_record(std::move(record));
            ++summary.records_emitted;
            ++expected_ply;
        }

        if (!saw_board) {
            emit_error(
                sink,
                summary,
                ParseErrorCode::UnsupportedInput,
                board_cursor->source().name,
                std::nullopt,
                "BoardState source contains no records"
            );
            return summary;
        }

        if (aux_cursor->peek().has_value()) {
            const LineRecord& leftover = *aux_cursor->peek();
            emit_error(
                sink,
                summary,
                ParseErrorCode::JoinMismatch,
                aux_cursor->source().name,
                leftover.line,
                "GameAux event has no matching BoardState ply"
            );
            return summary;
        }

        summary.completed = true;
        return summary;
    } catch (const std::exception& error) {
        emit_error(
            sink,
            summary,
            ParseErrorCode::InternalError,
            {},
            std::nullopt,
            error.what()
        );
        return summary;
    }
}

} // namespace kadoka::shogi::training
