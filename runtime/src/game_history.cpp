#include "kadoka/runtime/game_history.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kadoka::shogi::runtime {
namespace {

constexpr std::string_view kUlidAlphabet =
    "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

struct JsonValue {
    enum class Kind {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Array,
    };

    Kind kind{Kind::Null};
    bool boolean{false};
    std::string text{};
    std::map<std::string, JsonValue> object{};
    std::vector<JsonValue> array{};
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    JsonValue parse() {
        skip_ws();
        JsonValue result = parse_value();
        skip_ws();
        if (pos_ != input_.size()) {
            fail("trailing JSON data");
        }
        return result;
    }

private:
    [[noreturn]] void fail(const char* message) const {
        throw std::invalid_argument(
            std::string("invalid JSON at byte ")
            + std::to_string(pos_)
            + ": "
            + message
        );
    }

    void skip_ws() {
        while (pos_ < input_.size()
               && std::isspace(
                   static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    bool consume(char expected) {
        if (pos_ < input_.size() && input_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    char take() {
        if (pos_ >= input_.size()) fail("unexpected end of input");
        return input_[pos_++];
    }

    void expect(char expected) {
        if (!consume(expected)) fail("unexpected token");
    }

    JsonValue parse_value() {
        skip_ws();
        if (pos_ >= input_.size()) fail("missing value");

        switch (input_[pos_]) {
        case 'n':
            return parse_literal("null", JsonValue::Kind::Null, false);
        case 't':
            return parse_literal("true", JsonValue::Kind::Boolean, true);
        case 'f':
            return parse_literal("false", JsonValue::Kind::Boolean, false);
        case '"': {
            JsonValue value;
            value.kind = JsonValue::Kind::String;
            value.text = parse_string();
            return value;
        }
        case '{':
            return parse_object();
        case '[':
            return parse_array();
        default:
            if (input_[pos_] == '-'
                || std::isdigit(
                    static_cast<unsigned char>(input_[pos_]))) {
                return parse_number();
            }
            fail("unknown value");
        }
    }

    JsonValue parse_literal(
        std::string_view literal,
        JsonValue::Kind kind,
        bool boolean) {
        if (input_.substr(pos_, literal.size()) != literal) {
            fail("invalid literal");
        }
        pos_ += literal.size();
        JsonValue value;
        value.kind = kind;
        value.boolean = boolean;
        return value;
    }

    static int hex_value(char ch) {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    }

    std::uint32_t parse_hex4() {
        if (input_.size() - pos_ < 4) fail("short unicode escape");
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const int hex = hex_value(input_[pos_++]);
            if (hex < 0) fail("invalid unicode escape");
            value = (value << 4U) | static_cast<std::uint32_t>(hex);
        }
        return value;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp <= 0x7FU) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FFU) {
            out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp <= 0xFFFFU) {
            out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
            out.push_back(static_cast<char>(
                0x80U | ((cp >> 6U) & 0x3FU)
            ));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp <= 0x10FFFFU) {
            out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
            out.push_back(static_cast<char>(
                0x80U | ((cp >> 12U) & 0x3FU)
            ));
            out.push_back(static_cast<char>(
                0x80U | ((cp >> 6U) & 0x3FU)
            ));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else {
            throw std::invalid_argument("unicode codepoint out of range");
        }
    }

    std::string parse_string() {
        expect('"');
        std::string out;
        while (true) {
            if (pos_ >= input_.size()) fail("unterminated string");
            const unsigned char ch =
                static_cast<unsigned char>(take());
            if (ch == '"') break;
            if (ch < 0x20U) fail("control character in string");

            if (ch != '\\') {
                out.push_back(static_cast<char>(ch));
                continue;
            }

            switch (const char escaped = take()) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                std::uint32_t cp = parse_hex4();
                if (cp >= 0xD800U && cp <= 0xDBFFU) {
                    if (input_.size() - pos_ < 6
                        || input_[pos_] != '\\'
                        || input_[pos_ + 1] != 'u') {
                        fail("unpaired high surrogate");
                    }
                    pos_ += 2;
                    const std::uint32_t low = parse_hex4();
                    if (low < 0xDC00U || low > 0xDFFFU) {
                        fail("invalid low surrogate");
                    }
                    cp = 0x10000U
                        + ((cp - 0xD800U) << 10U)
                        + (low - 0xDC00U);
                } else if (cp >= 0xDC00U && cp <= 0xDFFFU) {
                    fail("unpaired low surrogate");
                }
                append_utf8(out, cp);
                break;
            }
            default:
                fail("invalid string escape");
            }
        }
        return out;
    }

    JsonValue parse_number() {
        const std::size_t start = pos_;
        consume('-');

        if (consume('0')) {
            if (pos_ < input_.size()
                && std::isdigit(
                    static_cast<unsigned char>(input_[pos_]))) {
                fail("leading zero in number");
            }
        } else {
            if (pos_ >= input_.size()
                || !std::isdigit(
                    static_cast<unsigned char>(input_[pos_]))) {
                fail("invalid number");
            }
            while (pos_ < input_.size()
                   && std::isdigit(
                       static_cast<unsigned char>(input_[pos_]))) {
                ++pos_;
            }
        }

        if (consume('.')) {
            if (pos_ >= input_.size()
                || !std::isdigit(
                    static_cast<unsigned char>(input_[pos_]))) {
                fail("invalid fraction");
            }
            while (pos_ < input_.size()
                   && std::isdigit(
                       static_cast<unsigned char>(input_[pos_]))) {
                ++pos_;
            }
        }

        if (pos_ < input_.size()
            && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size()
                && (input_[pos_] == '+' || input_[pos_] == '-')) {
                ++pos_;
            }
            if (pos_ >= input_.size()
                || !std::isdigit(
                    static_cast<unsigned char>(input_[pos_]))) {
                fail("invalid exponent");
            }
            while (pos_ < input_.size()
                   && std::isdigit(
                       static_cast<unsigned char>(input_[pos_]))) {
                ++pos_;
            }
        }

        JsonValue value;
        value.kind = JsonValue::Kind::Number;
        value.text = std::string(input_.substr(start, pos_ - start));
        return value;
    }

    JsonValue parse_object() {
        expect('{');
        JsonValue value;
        value.kind = JsonValue::Kind::Object;
        skip_ws();
        if (consume('}')) return value;

        while (true) {
            skip_ws();
            if (pos_ >= input_.size() || input_[pos_] != '"') {
                fail("object key must be string");
            }
            std::string key = parse_string();
            skip_ws();
            expect(':');
            skip_ws();
            JsonValue child = parse_value();
            const auto [_, inserted] =
                value.object.emplace(std::move(key), std::move(child));
            if (!inserted) fail("duplicate object key");

            skip_ws();
            if (consume('}')) break;
            expect(',');
        }
        return value;
    }

    JsonValue parse_array() {
        expect('[');
        JsonValue value;
        value.kind = JsonValue::Kind::Array;
        skip_ws();
        if (consume(']')) return value;

        while (true) {
            value.array.push_back(parse_value());
            skip_ws();
            if (consume(']')) break;
            expect(',');
            skip_ws();
        }
        return value;
    }

    std::string_view input_;
    std::size_t pos_{0};
};

const JsonValue& require_object(
    const JsonValue& value,
    const char* context) {
    if (value.kind != JsonValue::Kind::Object) {
        throw std::invalid_argument(
            std::string(context) + " must be object"
        );
    }
    return value;
}

const JsonValue& require_field(
    const JsonValue& object,
    std::string_view key) {
    require_object(object, "JSON object");
    const auto found = object.object.find(std::string(key));
    if (found == object.object.end()) {
        throw std::invalid_argument(
            "missing JSON field: " + std::string(key)
        );
    }
    return found->second;
}

const JsonValue* optional_field(
    const JsonValue& object,
    std::string_view key) {
    require_object(object, "JSON object");
    const auto found = object.object.find(std::string(key));
    return found == object.object.end() ? nullptr : &found->second;
}

std::string require_string(
    const JsonValue& value,
    const char* context) {
    if (value.kind != JsonValue::Kind::String) {
        throw std::invalid_argument(
            std::string(context) + " must be string"
        );
    }
    return value.text;
}

std::uint64_t require_uint64(
    const JsonValue& value,
    const char* context) {
    if (value.kind != JsonValue::Kind::Number
        || value.text.empty()
        || value.text.front() == '-'
        || value.text.find_first_of(".eE") != std::string::npos) {
        throw std::invalid_argument(
            std::string(context) + " must be unsigned integer"
        );
    }
    std::uint64_t result = 0;
    const char* first = value.text.data();
    const char* last = first + value.text.size();
    const auto parsed = std::from_chars(first, last, result);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        throw std::invalid_argument(
            std::string(context) + " integer out of range"
        );
    }
    return result;
}

std::optional<std::int64_t> require_nullable_nonnegative(
    const JsonValue& value,
    const char* context) {
    if (value.kind == JsonValue::Kind::Null) return std::nullopt;
    if (value.kind != JsonValue::Kind::Number
        || value.text.empty()
        || value.text.front() == '-'
        || value.text.find_first_of(".eE") != std::string::npos) {
        throw std::invalid_argument(
            std::string(context) + " must be nonnegative integer or null"
        );
    }
    std::int64_t result = 0;
    const char* first = value.text.data();
    const char* last = first + value.text.size();
    const auto parsed = std::from_chars(first, last, result);
    if (parsed.ec != std::errc{} || parsed.ptr != last || result < 0) {
        throw std::invalid_argument(
            std::string(context) + " invalid integer"
        );
    }
    return result;
}

void validate_schema(
    const JsonValue& root,
    std::string_view expected) {
    require_object(root, "JSON root");
    if (require_string(require_field(root, "schema"), "schema")
        != expected) {
        throw std::invalid_argument("unexpected schema");
    }
    if (require_uint64(require_field(root, "version"), "version")
        != kGameHistorySchemaVersion) {
        throw std::invalid_argument("unsupported schema version");
    }
}

void append_json_string(std::string& out, std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    out.push_back('"');
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (ch < 0x20U) {
                out += "\\u00";
                out.push_back(hex[(ch >> 4U) & 0xFU]);
                out.push_back(hex[ch & 0xFU]);
            } else {
                out.push_back(static_cast<char>(ch));
            }
        }
    }
    out.push_back('"');
}

void append_nullable_integer(
    std::string& out,
    const std::optional<std::int64_t>& value) {
    if (!value.has_value()) {
        out += "null";
        return;
    }
    if (*value < 0) {
        throw std::invalid_argument(
            "history clock values must be nonnegative"
        );
    }
    out += std::to_string(*value);
}

const char* color_name(Color color) noexcept {
    return color == Color::Black ? "black" : "white";
}

Color parse_color(std::string_view value) {
    if (value == "black") return Color::Black;
    if (value == "white") return Color::White;
    throw std::invalid_argument("color must be black or white");
}

const char* result_name(GameResult result) {
    switch (result) {
    case GameResult::BlackWin: return "black_win";
    case GameResult::WhiteWin: return "white_win";
    case GameResult::Draw: return "draw";
    case GameResult::ReplayRequired: return "replay_required";
    case GameResult::Unresolved: return "unresolved";
    }
    throw std::invalid_argument("unknown GameResult");
}

GameResult parse_result(std::string_view value) {
    if (value == "black_win") return GameResult::BlackWin;
    if (value == "white_win") return GameResult::WhiteWin;
    if (value == "draw") return GameResult::Draw;
    if (value == "replay_required") return GameResult::ReplayRequired;
    if (value == "unresolved") return GameResult::Unresolved;
    throw std::invalid_argument("unknown terminal result");
}

const char* reason_name(GameEndReason reason) {
    switch (reason) {
    case GameEndReason::Checkmate: return "checkmate";
    case GameEndReason::Repetition: return "repetition";
    case GameEndReason::PerpetualCheckViolation:
        return "perpetual_check_violation";
    case GameEndReason::Resignation: return "resignation";
    case GameEndReason::TimeForfeit: return "time_forfeit";
    case GameEndReason::Impasse: return "impasse";
    case GameEndReason::NoLegalMoves: return "no_legal_moves";
    case GameEndReason::EngineAttemptLimit:
        return "engine_attempt_limit";
    case GameEndReason::PlyLimit: return "ply_limit";
    }
    throw std::invalid_argument("unknown GameEndReason");
}

GameEndReason parse_reason(std::string_view value) {
    if (value == "checkmate") return GameEndReason::Checkmate;
    if (value == "repetition") return GameEndReason::Repetition;
    if (value == "perpetual_check_violation") {
        return GameEndReason::PerpetualCheckViolation;
    }
    if (value == "resignation") return GameEndReason::Resignation;
    if (value == "time_forfeit") return GameEndReason::TimeForfeit;
    if (value == "impasse") return GameEndReason::Impasse;
    if (value == "no_legal_moves") return GameEndReason::NoLegalMoves;
    if (value == "engine_attempt_limit") {
        return GameEndReason::EngineAttemptLimit;
    }
    if (value == "ply_limit") return GameEndReason::PlyLimit;
    throw std::invalid_argument("unknown terminal reason");
}

GameOutcome make_parsed_outcome(
    GameResult result,
    GameEndReason reason) {
    if (result == GameResult::BlackWin) {
        return make_win_outcome(Color::Black, reason);
    }
    if (result == GameResult::WhiteWin) {
        return make_win_outcome(Color::White, reason);
    }
    return GameOutcome{
        result,
        reason,
        std::nullopt,
        std::nullopt,
    };
}

void validate_game_id(std::string_view game_id) {
    if (!is_valid_ulid(game_id)) {
        throw std::invalid_argument("game_id must be a canonical ULID");
    }
}

void append_history_clock(
    std::string& out,
    const HistoryClock& clock) {
    out += "{\"black_main_ms\":";
    append_nullable_integer(out, clock.black_main_ms);
    out += ",\"white_main_ms\":";
    append_nullable_integer(out, clock.white_main_ms);
    out += ",\"black_byoyomi_ms\":";
    append_nullable_integer(out, clock.black_byoyomi_ms);
    out += ",\"white_byoyomi_ms\":";
    append_nullable_integer(out, clock.white_byoyomi_ms);
    out += ",\"black_increment_ms\":";
    append_nullable_integer(out, clock.black_increment_ms);
    out += ",\"white_increment_ms\":";
    append_nullable_integer(out, clock.white_increment_ms);
    out += ",\"per_move_limit_ms\":";
    append_nullable_integer(out, clock.per_move_limit_ms);
    out += '}';
}

HistoryClock parse_history_clock(const JsonValue& value) {
    require_object(value, "clock");
    HistoryClock clock;
    clock.black_main_ms = require_nullable_nonnegative(
        require_field(value, "black_main_ms"),
        "black_main_ms"
    );
    clock.white_main_ms = require_nullable_nonnegative(
        require_field(value, "white_main_ms"),
        "white_main_ms"
    );
    clock.black_byoyomi_ms = require_nullable_nonnegative(
        require_field(value, "black_byoyomi_ms"),
        "black_byoyomi_ms"
    );
    clock.white_byoyomi_ms = require_nullable_nonnegative(
        require_field(value, "white_byoyomi_ms"),
        "white_byoyomi_ms"
    );
    clock.black_increment_ms = require_nullable_nonnegative(
        require_field(value, "black_increment_ms"),
        "black_increment_ms"
    );
    clock.white_increment_ms = require_nullable_nonnegative(
        require_field(value, "white_increment_ms"),
        "white_increment_ms"
    );
    clock.per_move_limit_ms = require_nullable_nonnegative(
        require_field(value, "per_move_limit_ms"),
        "per_move_limit_ms"
    );
    return clock;
}

void append_action(
    std::string& out,
    const PlayerAction& action) {
    out += "{\"type\":";
    if (action.type == PlayerActionType::Resign) {
        append_json_string(out, "resign");
        out += '}';
        return;
    }
    if (!action.move.has_value()) {
        throw std::invalid_argument("move action is missing move");
    }
    append_json_string(out, "move");
    out += ",\"move\":";
    append_json_string(out, move_to_usi(*action.move));
    out += '}';
}

PlayerAction parse_action(const JsonValue& value) {
    require_object(value, "action");
    const std::string type =
        require_string(require_field(value, "type"), "action.type");
    if (type == "resign") return PlayerAction::resign_action();
    if (type == "move") {
        return PlayerAction::move_action(
            move_from_usi(require_string(
                require_field(value, "move"),
                "action.move"
            ))
        );
    }
    throw std::invalid_argument("unsupported action type");
}

void append_action_result(
    std::string& out,
    const ActionResult& result) {
    out += "{\"status\":";
    if (result.status == ActionResultStatus::Accepted) {
        append_json_string(out, "accepted");
    } else {
        append_json_string(out, "illegal");
    }
    out += ",\"reason\":";
    if (result.reason.has_value()) {
        append_json_string(out, *result.reason);
    } else {
        out += "null";
    }
    out += '}';
}

ActionResult parse_action_result(const JsonValue& value) {
    require_object(value, "result");
    const std::string status =
        require_string(require_field(value, "status"), "result.status");

    const JsonValue& reason_value = require_field(value, "reason");
    std::optional<std::string> reason;
    if (reason_value.kind != JsonValue::Kind::Null) {
        reason = require_string(reason_value, "result.reason");
    }

    if (status == "accepted") {
        return ActionResult{
            ActionResultStatus::Accepted,
            std::move(reason),
        };
    }
    if (status == "illegal") {
        if (!reason.has_value() || reason->empty()) {
            throw std::invalid_argument(
                "illegal action result requires reason"
            );
        }
        return ActionResult{
            ActionResultStatus::Illegal,
            std::move(reason),
        };
    }
    throw std::invalid_argument("unsupported action result status");
}

void append_terminal(
    std::string& out,
    const GameOutcome& outcome) {
    out += "{\"result\":";
    append_json_string(out, result_name(outcome.result));
    out += ",\"reason\":";
    append_json_string(out, reason_name(outcome.reason));
    out += '}';
}

GameOutcome parse_terminal(const JsonValue& value) {
    require_object(value, "terminal");
    return make_parsed_outcome(
        parse_result(require_string(
            require_field(value, "result"),
            "terminal.result"
        )),
        parse_reason(require_string(
            require_field(value, "reason"),
            "terminal.reason"
        ))
    );
}

std::vector<std::string_view> jsonl_lines(std::string_view jsonl) {
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= jsonl.size()) {
        const std::size_t newline = jsonl.find('\n', start);
        const std::size_t end =
            newline == std::string_view::npos
                ? jsonl.size()
                : newline;

        std::string_view line = jsonl.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        const bool only_ws = std::all_of(
            line.begin(),
            line.end(),
            [](char ch) {
                return std::isspace(
                    static_cast<unsigned char>(ch)
                ) != 0;
            }
        );
        if (!line.empty() && !only_ws) lines.push_back(line);

        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    return lines;
}

} // namespace

std::string make_ulid(
    std::uint64_t timestamp_ms,
    const std::array<std::uint8_t, 10>& entropy) {
    if (timestamp_ms >= (1ULL << 48U)) {
        throw std::invalid_argument(
            "ULID timestamp exceeds 48-bit range"
        );
    }

    std::string out(26, '0');
    std::uint64_t timestamp = timestamp_ms;
    for (int index = 9; index >= 0; --index) {
        out[static_cast<std::size_t>(index)] =
            kUlidAlphabet[static_cast<std::size_t>(
                timestamp & 0x1FU
            )];
        timestamp >>= 5U;
    }

    std::uint32_t buffer = 0;
    unsigned bits = 0;
    std::size_t out_index = 10;
    for (const std::uint8_t byte : entropy) {
        buffer = (buffer << 8U) | byte;
        bits += 8U;
        while (bits >= 5U) {
            bits -= 5U;
            out[out_index++] =
                kUlidAlphabet[(buffer >> bits) & 0x1FU];
            if (bits == 0U) {
                buffer = 0;
            } else {
                buffer &= (1U << bits) - 1U;
            }
        }
    }

    if (out_index != out.size() || bits != 0U) {
        throw std::logic_error("internal ULID encoding error");
    }
    return out;
}

std::string generate_ulid() {
    const auto now = std::chrono::time_point_cast<
        std::chrono::milliseconds
    >(std::chrono::system_clock::now());
    const std::uint64_t timestamp_ms =
        static_cast<std::uint64_t>(
            now.time_since_epoch().count()
        );

    std::random_device random;
    std::array<std::uint8_t, 10> entropy{};
    for (std::uint8_t& byte : entropy) {
        byte = static_cast<std::uint8_t>(random() & 0xFFU);
    }
    return make_ulid(timestamp_ms, entropy);
}

bool is_valid_ulid(std::string_view value) noexcept {
    if (value.size() != 26 || value.front() > '7') return false;
    return std::all_of(
        value.begin(),
        value.end(),
        [](char ch) {
            return kUlidAlphabet.find(ch)
                != std::string_view::npos;
        }
    );
}

BoardStateRecord make_board_state_record(
    std::string game_id,
    std::uint64_t ply,
    const Position& position,
    HistoryClock clock) {
    validate_game_id(game_id);
    return BoardStateRecord{
        std::move(game_id),
        ply,
        position.to_sfen(),
        position.side_to_move(),
        std::move(clock),
    };
}

std::string serialize_board_state_record(
    const BoardStateRecord& record) {
    validate_game_id(record.game_id);
    const Position parsed = Position::from_sfen(record.sfen);
    if (parsed.side_to_move() != record.side_to_move) {
        throw std::invalid_argument(
            "BoardState side_to_move disagrees with SFEN"
        );
    }

    std::string out =
        "{\"schema\":\"kadoka.board_state\",\"version\":1,\"game_id\":";
    append_json_string(out, record.game_id);
    out += ",\"ply\":";
    out += std::to_string(record.ply);
    out += ",\"sfen\":";
    append_json_string(out, record.sfen);
    out += ",\"side_to_move\":";
    append_json_string(out, color_name(record.side_to_move));
    out += ",\"clock\":";
    append_history_clock(out, record.clock);
    out += '}';
    return out;
}

BoardStateRecord deserialize_board_state_record(
    std::string_view json) {
    const JsonValue root = JsonParser(json).parse();
    validate_schema(root, "kadoka.board_state");

    BoardStateRecord record;
    record.game_id =
        require_string(require_field(root, "game_id"), "game_id");
    validate_game_id(record.game_id);
    record.ply = require_uint64(require_field(root, "ply"), "ply");
    record.sfen =
        require_string(require_field(root, "sfen"), "sfen");
    record.side_to_move = parse_color(require_string(
        require_field(root, "side_to_move"),
        "side_to_move"
    ));
    record.clock = parse_history_clock(require_field(root, "clock"));

    const Position parsed = Position::from_sfen(record.sfen);
    if (parsed.side_to_move() != record.side_to_move) {
        throw std::invalid_argument(
            "BoardState side_to_move disagrees with SFEN"
        );
    }
    return record;
}

std::string serialize_game_aux_record(
    const GameAuxRecord& record) {
    validate_game_id(record.game_id);

    if (record.event_type == GameAuxEventType::Action) {
        if (!record.actor.has_value()
            || !record.action.has_value()
            || !record.result.has_value()
            || record.terminal.has_value()) {
            throw std::invalid_argument(
                "action event requires actor/action/result only"
            );
        }
    } else {
        if (record.actor.has_value()
            || record.action.has_value()
            || record.result.has_value()
            || !record.terminal.has_value()) {
            throw std::invalid_argument(
                "terminal event requires terminal only"
            );
        }
    }

    std::string out =
        "{\"schema\":\"kadoka.game_aux\",\"version\":1,\"game_id\":";
    append_json_string(out, record.game_id);
    out += ",\"ply\":";
    out += std::to_string(record.ply);
    out += ",\"event_index\":";
    out += std::to_string(record.event_index);
    out += ",\"event_type\":";
    append_json_string(
        out,
        record.event_type == GameAuxEventType::Action
            ? "action"
            : "terminal"
    );

    out += ",\"actor\":";
    if (record.actor.has_value()) {
        append_json_string(out, color_name(*record.actor));
    } else {
        out += "null";
    }

    out += ",\"action\":";
    if (record.action.has_value()) {
        append_action(out, *record.action);
    } else {
        out += "null";
    }

    out += ",\"result\":";
    if (record.result.has_value()) {
        append_action_result(out, *record.result);
    } else {
        out += "null";
    }

    out += ",\"terminal\":";
    if (record.terminal.has_value()) {
        append_terminal(out, *record.terminal);
    } else {
        out += "null";
    }
    out += '}';
    return out;
}

GameAuxRecord deserialize_game_aux_record(
    std::string_view json) {
    const JsonValue root = JsonParser(json).parse();
    validate_schema(root, "kadoka.game_aux");

    GameAuxRecord record;
    record.game_id =
        require_string(require_field(root, "game_id"), "game_id");
    validate_game_id(record.game_id);
    record.ply = require_uint64(require_field(root, "ply"), "ply");
    record.event_index = require_uint64(
        require_field(root, "event_index"),
        "event_index"
    );

    const std::string event_type = require_string(
        require_field(root, "event_type"),
        "event_type"
    );
    if (event_type == "action") {
        record.event_type = GameAuxEventType::Action;
    } else if (event_type == "terminal") {
        record.event_type = GameAuxEventType::Terminal;
    } else {
        throw std::invalid_argument("unsupported event_type");
    }

    const JsonValue& actor = require_field(root, "actor");
    if (actor.kind != JsonValue::Kind::Null) {
        record.actor = parse_color(require_string(actor, "actor"));
    }

    const JsonValue& action = require_field(root, "action");
    if (action.kind != JsonValue::Kind::Null) {
        record.action = parse_action(action);
    }

    const JsonValue& result = require_field(root, "result");
    if (result.kind != JsonValue::Kind::Null) {
        record.result = parse_action_result(result);
    }

    const JsonValue& terminal = require_field(root, "terminal");
    if (terminal.kind != JsonValue::Kind::Null) {
        record.terminal = parse_terminal(terminal);
    }

    if (record.event_type == GameAuxEventType::Action) {
        if (!record.actor.has_value()
            || !record.action.has_value()
            || !record.result.has_value()
            || record.terminal.has_value()) {
            throw std::invalid_argument(
                "invalid action event shape"
            );
        }
    } else if (record.actor.has_value()
               || record.action.has_value()
               || record.result.has_value()
               || !record.terminal.has_value()) {
        throw std::invalid_argument(
            "invalid terminal event shape"
        );
    }

    return record;
}

std::string serialize_board_state_jsonl(
    std::span<const BoardStateRecord> records) {
    std::string out;
    for (const BoardStateRecord& record : records) {
        out += serialize_board_state_record(record);
        out.push_back('\n');
    }
    return out;
}

std::vector<BoardStateRecord> deserialize_board_state_jsonl(
    std::string_view jsonl) {
    std::vector<BoardStateRecord> records;
    for (const std::string_view line : jsonl_lines(jsonl)) {
        records.push_back(deserialize_board_state_record(line));
    }
    return records;
}

std::string serialize_game_aux_jsonl(
    std::span<const GameAuxRecord> records) {
    std::string out;
    for (const GameAuxRecord& record : records) {
        out += serialize_game_aux_record(record);
        out.push_back('\n');
    }
    return out;
}

std::vector<GameAuxRecord> deserialize_game_aux_jsonl(
    std::string_view jsonl) {
    std::vector<GameAuxRecord> records;
    for (const std::string_view line : jsonl_lines(jsonl)) {
        records.push_back(deserialize_game_aux_record(line));
    }
    return records;
}

GameHistoryRecorder::GameHistoryRecorder(
    Position initial_position,
    HistoryClock initial_clock,
    std::string game_id)
    : game_id_(
          game_id.empty() ? generate_ulid() : std::move(game_id)
      ),
      position_(std::move(initial_position)) {
    validate_game_id(game_id_);
    board_states_.push_back(make_board_state_record(
        game_id_,
        0,
        position_,
        std::move(initial_clock)
    ));
}

ActionResult GameHistoryRecorder::apply_and_record(
    Color actor,
    const PlayerAction& action,
    HistoryClock clock_after) {
    if (terminal_recorded_) {
        throw std::logic_error("cannot record action after terminal");
    }
    if (terminal_pending_) {
        throw std::logic_error(
            "terminal event required before another action"
        );
    }
    if (actor != position_.side_to_move()) {
        throw std::invalid_argument(
            "action actor does not match side to move"
        );
    }

    const NativeActionApplication application =
        apply_player_action(position_, action);

    events_.push_back(GameAuxRecord{
        game_id_,
        ply_,
        next_event_index_++,
        GameAuxEventType::Action,
        actor,
        action,
        application.result,
        std::nullopt,
    });

    if (application.resigned) {
        terminal_pending_ = true;
        return application.result;
    }

    if (application.result.status == ActionResultStatus::Accepted) {
        if (!application.next_position.has_value()) {
            throw std::logic_error(
                "accepted move missing next position"
            );
        }
        position_ = *application.next_position;
        ++ply_;
        board_states_.push_back(make_board_state_record(
            game_id_,
            ply_,
            position_,
            std::move(clock_after)
        ));
    }

    return application.result;
}

void GameHistoryRecorder::record_terminal(
    const GameOutcome& outcome) {
    if (terminal_recorded_) {
        throw std::logic_error("terminal already recorded");
    }

    events_.push_back(GameAuxRecord{
        game_id_,
        ply_,
        next_event_index_++,
        GameAuxEventType::Terminal,
        std::nullopt,
        std::nullopt,
        std::nullopt,
        outcome,
    });
    terminal_recorded_ = true;
    terminal_pending_ = false;
}

} // namespace kadoka::shogi::runtime
