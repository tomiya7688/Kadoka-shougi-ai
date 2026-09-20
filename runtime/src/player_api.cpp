#include "kadoka/runtime/player_api.hpp"

#include "kadoka/movegen.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kadoka::shogi::runtime {
namespace {

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
        while (pos_ < input_.size()) {
            const unsigned char ch =
                static_cast<unsigned char>(input_[pos_]);
            if (!std::isspace(ch)) break;
            ++pos_;
        }
    }

    char take() {
        if (pos_ >= input_.size()) fail("unexpected end of input");
        return input_[pos_++];
    }

    bool consume(char expected) {
        if (pos_ < input_.size() && input_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
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
            if (input_[pos_] == '-' || std::isdigit(
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
        std::string result;
        while (true) {
            if (pos_ >= input_.size()) fail("unterminated string");
            const unsigned char ch =
                static_cast<unsigned char>(take());
            if (ch == '"') break;
            if (ch < 0x20U) fail("control character in string");

            if (ch != '\\') {
                result.push_back(static_cast<char>(ch));
                continue;
            }

            const char escaped = take();
            switch (escaped) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
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
                append_utf8(result, cp);
                break;
            }
            default:
                fail("invalid string escape");
            }
        }
        return result;
    }

    JsonValue parse_number() {
        const std::size_t start = pos_;
        consume('-');

        if (consume('0')) {
            if (pos_ < input_.size()
                && std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
                fail("leading zero in number");
            }
        } else {
            if (pos_ >= input_.size()
                || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
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
                || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
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
                || !std::isdigit(static_cast<unsigned char>(input_[pos_]))) {
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
                fail("object key must be a string");
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

const JsonValue& require_object(const JsonValue& value, const char* context) {
    if (value.kind != JsonValue::Kind::Object) {
        throw std::invalid_argument(std::string(context) + " must be object");
    }
    return value;
}

const JsonValue& require_field(
    const JsonValue& object,
    std::string_view key) {
    require_object(object, "JSON root");
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

std::string require_string(const JsonValue& value, const char* context) {
    if (value.kind != JsonValue::Kind::String) {
        throw std::invalid_argument(std::string(context) + " must be string");
    }
    return value.text;
}

std::int64_t require_integer(const JsonValue& value, const char* context) {
    if (value.kind != JsonValue::Kind::Number
        || value.text.find_first_of(".eE") != std::string::npos) {
        throw std::invalid_argument(std::string(context) + " must be integer");
    }

    std::int64_t result = 0;
    const char* first = value.text.data();
    const char* last = first + value.text.size();
    const auto parsed = std::from_chars(first, last, result);
    if (parsed.ec != std::errc{} || parsed.ptr != last) {
        throw std::invalid_argument(std::string(context) + " integer out of range");
    }
    return result;
}

std::optional<std::int64_t> require_nullable_nonnegative_integer(
    const JsonValue& value,
    const char* context) {
    if (value.kind == JsonValue::Kind::Null) return std::nullopt;
    const std::int64_t number = require_integer(value, context);
    if (number < 0) {
        throw std::invalid_argument(
            std::string(context) + " must be nonnegative or null"
        );
    }
    return number;
}

void validate_schema(
    const JsonValue& root,
    std::string_view expected_schema) {
    require_object(root, "JSON root");
    const std::string schema =
        require_string(require_field(root, "schema"), "schema");
    if (schema != expected_schema) {
        throw std::invalid_argument("unexpected schema: " + schema);
    }
    const std::int64_t version =
        require_integer(require_field(root, "version"), "version");
    if (version != static_cast<std::int64_t>(kPlayerApiSchemaVersion)) {
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

void append_optional_integer(
    std::string& out,
    const std::optional<std::int64_t>& value) {
    if (value.has_value()) {
        if (*value < 0) {
            throw std::invalid_argument("clock values must be nonnegative");
        }
        out += std::to_string(*value);
    } else {
        out += "null";
    }
}

const char* color_name(Color color) noexcept {
    return color == Color::Black ? "black" : "white";
}

Color parse_color(std::string_view value) {
    if (value == "black") return Color::Black;
    if (value == "white") return Color::White;
    throw std::invalid_argument("side_to_move must be black or white");
}

char rank_to_usi(std::uint8_t rank) {
    if (rank < 1 || rank > 9) {
        throw std::invalid_argument("USI rank out of range");
    }
    return static_cast<char>('a' + rank - 1);
}

std::uint8_t rank_from_usi(char rank) {
    if (rank < 'a' || rank > 'i') {
        throw std::invalid_argument("USI rank must be a-i");
    }
    return static_cast<std::uint8_t>(rank - 'a' + 1);
}

std::uint8_t file_from_usi(char file) {
    if (file < '1' || file > '9') {
        throw std::invalid_argument("USI file must be 1-9");
    }
    return static_cast<std::uint8_t>(file - '0');
}

char drop_piece_to_usi(PieceType type) {
    switch (type) {
    case PieceType::Pawn: return 'P';
    case PieceType::Lance: return 'L';
    case PieceType::Knight: return 'N';
    case PieceType::Silver: return 'S';
    case PieceType::Gold: return 'G';
    case PieceType::Bishop: return 'B';
    case PieceType::Rook: return 'R';
    default:
        throw std::invalid_argument("piece cannot be dropped in USI");
    }
}

PieceType drop_piece_from_usi(char code) {
    switch (code) {
    case 'P': return PieceType::Pawn;
    case 'L': return PieceType::Lance;
    case 'N': return PieceType::Knight;
    case 'S': return PieceType::Silver;
    case 'G': return PieceType::Gold;
    case 'B': return PieceType::Bishop;
    case 'R': return PieceType::Rook;
    default:
        throw std::invalid_argument("invalid USI drop piece");
    }
}

bool same_move(const Move& lhs, const Move& rhs) {
    return lhs.from == rhs.from
        && lhs.to == rhs.to
        && lhs.drop_piece == rhs.drop_piece
        && lhs.promote == rhs.promote;
}

PlayerClockSide parse_clock_side(
    const JsonValue& root,
    const char* context) {
    require_object(root, context);
    PlayerClockSide side;
    side.main_ms = require_nullable_nonnegative_integer(
        require_field(root, "main_ms"),
        "main_ms"
    );
    side.byoyomi_ms = require_nullable_nonnegative_integer(
        require_field(root, "byoyomi_ms"),
        "byoyomi_ms"
    );
    side.increment_ms = require_nullable_nonnegative_integer(
        require_field(root, "increment_ms"),
        "increment_ms"
    );
    return side;
}

void append_clock_side(std::string& out, const PlayerClockSide& side) {
    out += "{\"main_ms\":";
    append_optional_integer(out, side.main_ms);
    out += ",\"byoyomi_ms\":";
    append_optional_integer(out, side.byoyomi_ms);
    out += ",\"increment_ms\":";
    append_optional_integer(out, side.increment_ms);
    out += '}';
}

} // namespace

PlayerAction PlayerAction::move_action(Move value) {
    return PlayerAction{PlayerActionType::Move, std::move(value)};
}

PlayerAction PlayerAction::resign_action() {
    return PlayerAction{PlayerActionType::Resign, std::nullopt};
}

PlayerObservation make_player_observation(
    const Position& position,
    PlayerClock clock) {
    return PlayerObservation{
        position.to_sfen(),
        position.side_to_move(),
        std::move(clock),
    };
}

NativeActionApplication apply_player_action(
    const Position& position,
    const PlayerAction& action) {
    if (action.type == PlayerActionType::Resign) {
        return NativeActionApplication{
            ActionResult{ActionResultStatus::Accepted, std::nullopt},
            std::nullopt,
            true,
        };
    }

    if (!action.move.has_value()) {
        throw std::invalid_argument("move action is missing move");
    }

    const std::vector<Move> legal_moves = generate_legal_moves(position);
    const bool legal = std::any_of(
        legal_moves.begin(),
        legal_moves.end(),
        [&](const Move& candidate) {
            return same_move(candidate, *action.move);
        }
    );

    if (!legal) {
        return NativeActionApplication{
            ActionResult{
                ActionResultStatus::Illegal,
                std::string("illegal_move"),
            },
            std::nullopt,
            false,
        };
    }

    return NativeActionApplication{
        ActionResult{ActionResultStatus::Accepted, std::nullopt},
        position.after_move(*action.move),
        false,
    };
}

std::string move_to_usi(const Move& move) {
    if (!move.to.valid()) {
        throw std::invalid_argument("move destination is outside board");
    }

    std::string result;
    if (move.is_drop()) {
        if (move.promote) {
            throw std::invalid_argument("drop cannot promote");
        }
        result.push_back(drop_piece_to_usi(move.drop_piece));
        result.push_back('*');
    } else {
        if (!move.from->valid()) {
            throw std::invalid_argument("move origin is outside board");
        }
        result.push_back(static_cast<char>('0' + move.from->file));
        result.push_back(rank_to_usi(move.from->rank));
    }

    result.push_back(static_cast<char>('0' + move.to.file));
    result.push_back(rank_to_usi(move.to.rank));
    if (!move.is_drop() && move.promote) result.push_back('+');
    return result;
}

Move move_from_usi(std::string_view usi) {
    if (usi.size() == 4 && usi[1] == '*') {
        return Move{
            std::nullopt,
            Square{file_from_usi(usi[2]), rank_from_usi(usi[3])},
            drop_piece_from_usi(usi[0]),
            false,
        };
    }

    if (usi.size() != 4 && usi.size() != 5) {
        throw std::invalid_argument("USI move must have 4 or 5 characters");
    }
    if (usi.size() == 5 && usi[4] != '+') {
        throw std::invalid_argument("invalid USI promotion suffix");
    }

    return Move{
        Square{file_from_usi(usi[0]), rank_from_usi(usi[1])},
        Square{file_from_usi(usi[2]), rank_from_usi(usi[3])},
        PieceType::None,
        usi.size() == 5,
    };
}

std::string serialize_player_observation(
    const PlayerObservation& observation) {
    const Position parsed = Position::from_sfen(observation.sfen);
    if (parsed.side_to_move() != observation.side_to_move) {
        throw std::invalid_argument(
            "observation side_to_move disagrees with SFEN"
        );
    }

    std::string out;
    out += "{\"schema\":\"kadoka.player_observation\",\"version\":1,\"board\":{\"sfen\":";
    append_json_string(out, observation.sfen);
    out += "},\"side_to_move\":";
    append_json_string(out, color_name(observation.side_to_move));
    out += ",\"clock\":{\"black\":";
    append_clock_side(out, observation.clock.black);
    out += ",\"white\":";
    append_clock_side(out, observation.clock.white);
    out += ",\"per_move_limit_ms\":";
    append_optional_integer(out, observation.clock.per_move_limit_ms);
    out += "}}";
    return out;
}

PlayerObservation deserialize_player_observation(std::string_view json) {
    const JsonValue root = JsonParser(json).parse();
    validate_schema(root, "kadoka.player_observation");

    const JsonValue& board = require_field(root, "board");
    require_object(board, "board");
    const std::string sfen =
        require_string(require_field(board, "sfen"), "board.sfen");

    const Color side = parse_color(
        require_string(require_field(root, "side_to_move"), "side_to_move")
    );

    const Position position = Position::from_sfen(sfen);
    if (position.side_to_move() != side) {
        throw std::invalid_argument(
            "side_to_move disagrees with SFEN"
        );
    }

    const JsonValue& clock = require_field(root, "clock");
    require_object(clock, "clock");

    PlayerClock parsed_clock;
    parsed_clock.black = parse_clock_side(
        require_field(clock, "black"),
        "clock.black"
    );
    parsed_clock.white = parse_clock_side(
        require_field(clock, "white"),
        "clock.white"
    );
    parsed_clock.per_move_limit_ms =
        require_nullable_nonnegative_integer(
            require_field(clock, "per_move_limit_ms"),
            "clock.per_move_limit_ms"
        );

    return PlayerObservation{sfen, side, std::move(parsed_clock)};
}

std::string serialize_player_action(const PlayerAction& action) {
    std::string out =
        "{\"schema\":\"kadoka.player_action\",\"version\":1,\"type\":";
    if (action.type == PlayerActionType::Resign) {
        append_json_string(out, "resign");
        out += '}';
        return out;
    }

    if (!action.move.has_value()) {
        throw std::invalid_argument("move action is missing move");
    }

    append_json_string(out, "move");
    out += ",\"move\":";
    append_json_string(out, move_to_usi(*action.move));
    out += '}';
    return out;
}

PlayerAction deserialize_player_action(std::string_view json) {
    const JsonValue root = JsonParser(json).parse();
    validate_schema(root, "kadoka.player_action");

    const std::string type =
        require_string(require_field(root, "type"), "type");
    if (type == "resign") {
        return PlayerAction::resign_action();
    }
    if (type == "move") {
        const std::string move =
            require_string(require_field(root, "move"), "move");
        return PlayerAction::move_action(move_from_usi(move));
    }
    throw std::invalid_argument("unsupported player action type");
}

std::string serialize_action_result(const ActionResult& result) {
    std::string out =
        "{\"schema\":\"kadoka.action_result\",\"version\":1,\"status\":";
    if (result.status == ActionResultStatus::Accepted) {
        append_json_string(out, "accepted");
        if (result.reason.has_value()) {
            out += ",\"reason\":";
            append_json_string(out, *result.reason);
        }
        out += '}';
        return out;
    }

    append_json_string(out, "illegal");
    if (!result.reason.has_value() || result.reason->empty()) {
        throw std::invalid_argument(
            "illegal action result requires reason"
        );
    }
    out += ",\"reason\":";
    append_json_string(out, *result.reason);
    out += '}';
    return out;
}

ActionResult deserialize_action_result(std::string_view json) {
    const JsonValue root = JsonParser(json).parse();
    validate_schema(root, "kadoka.action_result");

    const std::string status =
        require_string(require_field(root, "status"), "status");
    const JsonValue* reason_value = optional_field(root, "reason");
    std::optional<std::string> reason;
    if (reason_value != nullptr
        && reason_value->kind != JsonValue::Kind::Null) {
        reason = require_string(*reason_value, "reason");
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

} // namespace kadoka::shogi::runtime
