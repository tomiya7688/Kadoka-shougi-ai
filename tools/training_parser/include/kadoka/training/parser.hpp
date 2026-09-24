#pragma once

#include "kadoka/runtime/game_history.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <istream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::training {

struct SourceLocation {
    std::string source_name{};
    std::uint64_t line{0};
};

struct SourceProvenance {
    std::string parser_id{};
    std::vector<SourceLocation> locations{};
};

enum class ParsedEventType : std::uint8_t {
    Action,
    Terminal,
};

struct ParsedEvent {
    std::uint64_t event_index{0};
    ParsedEventType type{ParsedEventType::Action};
    std::optional<Color> actor{};
    std::optional<runtime::PlayerAction> action{};
    std::optional<runtime::ActionResult> result{};
    std::optional<runtime::GameOutcome> terminal{};
    SourceLocation source{};
};

struct ParsedRecord {
    std::optional<std::string> game_id{};
    std::optional<std::uint64_t> ply{};
    std::string sfen{};
    Color side_to_move{Color::Black};
    runtime::HistoryClock clock{};
    std::vector<ParsedEvent> events{};
    SourceProvenance provenance{};
};

enum class ParseErrorCode : std::uint8_t {
    InvalidInputCount,
    DuplicateSourceName,
    UnsupportedInput,
    MalformedRecord,
    JoinMismatch,
    OrderingViolation,
    InternalError,
};

struct ParseError {
    ParseErrorCode code{ParseErrorCode::MalformedRecord};
    std::string parser_id{};
    std::string source_name{};
    std::optional<std::uint64_t> line{};
    std::string message{};
    bool recoverable{false};
};

struct ParseSummary {
    std::size_t records_emitted{0};
    std::size_t errors_emitted{0};
    bool completed{false};
};

struct ParserSource {
    std::string name{};
    std::istream* stream{nullptr};
};

class ParsedRecordSink {
public:
    virtual ~ParsedRecordSink() = default;

    virtual void on_record(ParsedRecord record) = 0;
    virtual void on_error(ParseError error) = 0;
};

class CollectingParsedRecordSink final : public ParsedRecordSink {
public:
    void on_record(ParsedRecord record) override;
    void on_error(ParseError error) override;

    [[nodiscard]] const std::vector<ParsedRecord>& records()
        const noexcept {
        return records_;
    }

    [[nodiscard]] const std::vector<ParseError>& errors()
        const noexcept {
        return errors_;
    }

    [[nodiscard]] std::vector<ParsedRecord>& records() noexcept {
        return records_;
    }

    [[nodiscard]] std::vector<ParseError>& errors() noexcept {
        return errors_;
    }

private:
    std::vector<ParsedRecord> records_{};
    std::vector<ParseError> errors_{};
};

class Parser {
public:
    virtual ~Parser() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;
    [[nodiscard]] virtual std::string_view display_name() const noexcept = 0;

    [[nodiscard]] virtual ParseSummary parse(
        std::span<const ParserSource> sources,
        ParsedRecordSink& sink
    ) const = 0;
};

class ParserRegistry {
public:
    void add(std::shared_ptr<const Parser> parser);

    [[nodiscard]] const Parser* find(
        std::string_view parser_id
    ) const noexcept;

    [[nodiscard]] std::vector<std::string> parser_ids() const;

private:
    std::map<std::string, std::shared_ptr<const Parser>> parsers_{};
};

[[nodiscard]] ParserRegistry make_reference_parser_registry();

} // namespace kadoka::shogi::training
