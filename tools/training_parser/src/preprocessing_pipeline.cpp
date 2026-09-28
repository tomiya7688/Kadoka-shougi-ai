#include "kadoka/training/preprocessing_pipeline.hpp"

#include "kadoka/position.hpp"
#include "kadoka/runtime/player_api.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace kadoka::shogi::training {
namespace {

char piece_letter(PieceType type) {
    switch (type) {
    case PieceType::Pawn:
    case PieceType::ProPawn: return 'P';
    case PieceType::Lance:
    case PieceType::ProLance: return 'L';
    case PieceType::Knight:
    case PieceType::ProKnight: return 'N';
    case PieceType::Silver:
    case PieceType::ProSilver: return 'S';
    case PieceType::Gold: return 'G';
    case PieceType::Bishop:
    case PieceType::Horse: return 'B';
    case PieceType::Rook:
    case PieceType::Dragon: return 'R';
    case PieceType::King: return 'K';
    case PieceType::None: break;
    }
    throw std::invalid_argument("empty piece has no SFEN letter");
}

bool promoted(PieceType type) noexcept {
    return type == PieceType::ProPawn
        || type == PieceType::ProLance
        || type == PieceType::ProKnight
        || type == PieceType::ProSilver
        || type == PieceType::Horse
        || type == PieceType::Dragon;
}

void append_hand(
    std::string& output,
    const std::array<std::uint8_t, 7>& hand,
    bool lowercase) {
    constexpr std::array<PieceType, 7> order{
        PieceType::Rook,
        PieceType::Bishop,
        PieceType::Gold,
        PieceType::Silver,
        PieceType::Knight,
        PieceType::Lance,
        PieceType::Pawn,
    };
    constexpr std::array<std::size_t, 7> indices{6, 5, 4, 3, 2, 1, 0};
    for (std::size_t i = 0; i < order.size(); ++i) {
        const std::uint8_t count = hand[indices[i]];
        if (count == 0) continue;
        if (count > 1) output += std::to_string(count);
        char letter = piece_letter(order[i]);
        if (lowercase) {
            letter = static_cast<char>(
                std::tolower(static_cast<unsigned char>(letter))
            );
        }
        output += letter;
    }
}

std::string transform_sfen(
    const Position& position,
    bool rotate_and_swap,
    bool mirror_files) {
    std::string output;
    for (std::uint8_t rank = 1; rank <= 9; ++rank) {
        unsigned empty = 0;
        for (int file = 9; file >= 1; --file) {
            const std::uint8_t target_file =
                static_cast<std::uint8_t>(file);
            const Square source_square{
                static_cast<std::uint8_t>(
                    (rotate_and_swap || mirror_files)
                        ? 10 - target_file
                        : target_file
                ),
                static_cast<std::uint8_t>(
                    rotate_and_swap ? 10 - rank : rank
                ),
            };
            Piece piece = position.at(source_square);
            if (piece.empty()) {
                ++empty;
                continue;
            }
            if (empty != 0) {
                output += static_cast<char>('0' + empty);
                empty = 0;
            }
            if (promoted(piece.type)) output += '+';
            char letter = piece_letter(piece.type);
            if (rotate_and_swap) piece.color = opposite(piece.color);
            if (piece.color == Color::White) {
                letter = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(letter))
                );
            }
            output += letter;
        }
        if (empty != 0) output += static_cast<char>('0' + empty);
        if (rank != 9) output += '/';
    }

    const Color side_to_move = rotate_and_swap
        ? opposite(position.side_to_move())
        : position.side_to_move();
    output += side_to_move == Color::Black ? " b " : " w ";

    const Hands& hands = position.hands();
    const std::array<std::uint8_t, 7>& black_hand = rotate_and_swap
        ? hands.white
        : hands.black;
    const std::array<std::uint8_t, 7>& white_hand = rotate_and_swap
        ? hands.black
        : hands.white;
    const std::size_t hand_start = output.size();
    append_hand(output, black_hand, false);
    append_hand(output, white_hand, true);
    if (output.size() == hand_start) output += '-';
    output += ' ';
    output += std::to_string(position.ply());
    return output;
}

Square transform_square(
    Square square,
    bool rotate_and_swap,
    bool mirror_files) noexcept {
    if (rotate_and_swap || mirror_files) {
        square.file = static_cast<std::uint8_t>(10 - square.file);
    }
    if (rotate_and_swap) {
        square.rank = static_cast<std::uint8_t>(10 - square.rank);
    }
    return square;
}

void transform_events(
    ParsedRecord& record,
    bool rotate_and_swap,
    bool mirror_files) {
    for (ParsedEvent& event : record.events) {
        if (rotate_and_swap && event.actor.has_value()) {
            event.actor = opposite(*event.actor);
        }
        if (event.action.has_value()
            && event.action->move.has_value()) {
            Move& move = *event.action->move;
            if (move.from.has_value()) {
                move.from = transform_square(
                    *move.from,
                    rotate_and_swap,
                    mirror_files
                );
            }
            move.to = transform_square(
                move.to,
                rotate_and_swap,
                mirror_files
            );
        }
        if (!rotate_and_swap || !event.terminal.has_value()) continue;

        runtime::GameOutcome& outcome = *event.terminal;
        if (outcome.result == runtime::GameResult::BlackWin) {
            outcome.result = runtime::GameResult::WhiteWin;
        } else if (outcome.result == runtime::GameResult::WhiteWin) {
            outcome.result = runtime::GameResult::BlackWin;
        }
        if (outcome.winner.has_value()) {
            outcome.winner = opposite(*outcome.winner);
        }
        if (outcome.loser.has_value()) {
            outcome.loser = opposite(*outcome.loser);
        }
    }
}

ParsedRecord transformed_record(
    const ParsedRecord& input,
    bool rotate_and_swap,
    bool mirror_files) {
    const Position position = Position::from_sfen(input.sfen);
    ParsedRecord output = input;
    output.sfen = transform_sfen(
        position,
        rotate_and_swap,
        mirror_files
    );
    const Position transformed = Position::from_sfen(output.sfen);
    output.side_to_move = transformed.side_to_move();
    transform_events(output, rotate_and_swap, mirror_files);

    if (rotate_and_swap) {
        std::swap(output.clock.black_main_ms, output.clock.white_main_ms);
        std::swap(output.clock.black_byoyomi_ms, output.clock.white_byoyomi_ms);
        std::swap(output.clock.black_increment_ms, output.clock.white_increment_ms);
    }
    return output;
}

std::string append_field(std::string_view field) {
    return std::to_string(field.size()) + ':' + std::string(field) + ';';
}

std::string record_key(const ParsedRecord& record) {
    std::string canonical_sfen = record.sfen;
    try {
        canonical_sfen = Position::from_sfen(record.sfen).to_sfen();
    } catch (const std::invalid_argument&) {
        // Keep malformed records distinct; validation stages can reject them.
    }
    std::string key = append_field(canonical_sfen);
    const auto append_clock = [&key](
        const std::optional<std::int64_t>& value) {
        key += value.has_value() ? std::to_string(*value) : "-";
        key += ':';
    };
    append_clock(record.clock.black_main_ms);
    append_clock(record.clock.white_main_ms);
    append_clock(record.clock.black_byoyomi_ms);
    append_clock(record.clock.white_byoyomi_ms);
    append_clock(record.clock.black_increment_ms);
    append_clock(record.clock.white_increment_ms);
    append_clock(record.clock.per_move_limit_ms);
    for (const ParsedEvent& event : record.events) {
        key += std::to_string(static_cast<unsigned>(event.type)) + ':';
        key += event.actor.has_value()
            ? std::to_string(static_cast<unsigned>(*event.actor))
            : "-";
        key += ':';

        if (event.action.has_value()) {
            key += std::to_string(
                static_cast<unsigned>(event.action->type)
            );
            key += ':';
            if (event.action->move.has_value()) {
                key += append_field(
                    runtime::move_to_usi(*event.action->move)
                );
            } else {
                key += "-;";
            }
        } else {
            key += "-:-;";
        }

        if (event.result.has_value()) {
            key += std::to_string(
                static_cast<unsigned>(event.result->status)
            );
            key += ':';
            key += event.result->reason.has_value()
                ? append_field(*event.result->reason)
                : "-;";
        } else {
            key += "-:-;";
        }

        if (event.terminal.has_value()) {
            key += std::to_string(
                static_cast<unsigned>(event.terminal->result)
            );
            key += ':';
            key += std::to_string(
                static_cast<unsigned>(event.terminal->reason)
            );
            key += ':';
            key += event.terminal->winner.has_value()
                ? std::to_string(
                    static_cast<unsigned>(*event.terminal->winner)
                )
                : "-";
            key += ':';
            key += event.terminal->loser.has_value()
                ? std::to_string(
                    static_cast<unsigned>(*event.terminal->loser)
                )
                : "-";
            key += ';';
        } else {
            key += "-;-;-;-;";
        }
    }
    return key;
}

class ExcludeInvalid final : public Preprocessor {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return kExcludeInvalidPreprocessorId;
    }

    void apply(
        const ParsedRecord& input,
        std::vector<ParsedRecord>& output,
        PreprocessorContext& context) const override {
        try {
            const Position position = Position::from_sfen(input.sfen);
            if (position.side_to_move() != input.side_to_move) {
                ++context.summary->invalid_records;
                return;
            }
            ParsedRecord canonical = input;
            canonical.sfen = position.to_sfen();
            canonical.side_to_move = position.side_to_move();
            output.push_back(std::move(canonical));
        } catch (const std::invalid_argument&) {
            ++context.summary->invalid_records;
        }
    }
};

class NormalizeSideToMove final : public Preprocessor {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return kNormalizeSidePreprocessorId;
    }

    void apply(
        const ParsedRecord& input,
        std::vector<ParsedRecord>& output,
        PreprocessorContext&) const override {
        if (input.side_to_move == Color::Black) {
            output.push_back(input);
        } else {
            output.push_back(transformed_record(input, true, false));
        }
    }
};

class ExpandFileMirror final : public Preprocessor {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return kExpandFileMirrorPreprocessorId;
    }

    void apply(
        const ParsedRecord& input,
        std::vector<ParsedRecord>& output,
        PreprocessorContext& context) const override {
        output.push_back(input);
        output.push_back(transformed_record(input, false, true));
        ++context.summary->expanded_records;
    }
};

class Deduplicate final : public Preprocessor {
public:
    [[nodiscard]] std::string_view id() const noexcept override {
        return kDeduplicatePreprocessorId;
    }

    void apply(
        const ParsedRecord& input,
        std::vector<ParsedRecord>& output,
        PreprocessorContext& context) const override {
        if (!context.seen_records.insert(record_key(input)).second) {
            ++context.summary->duplicate_records;
            return;
        }
        output.push_back(input);
    }
};

class JsonReader {
public:
    explicit JsonReader(std::string_view input) : input_(input) {}

    void expect(char expected) {
        skip_space();
        if (position_ >= input_.size() || input_[position_] != expected) {
            throw std::invalid_argument("invalid preprocessing JSON");
        }
        ++position_;
    }

    [[nodiscard]] std::string string() {
        skip_space();
        expect('"');
        std::string value;
        while (position_ < input_.size()) {
            const char ch = input_[position_++];
            if (ch == '"') return value;
            if (static_cast<unsigned char>(ch) < 0x20U) {
                throw std::invalid_argument(
                    "control character in preprocessing JSON string"
                );
            }
            if (ch != '\\') {
                value += ch;
                continue;
            }
            if (position_ >= input_.size()) {
                throw std::invalid_argument(
                    "unterminated preprocessing JSON escape"
                );
            }
            switch (input_[position_++]) {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            case 'u': {
                if (position_ + 4 > input_.size()) {
                    throw std::invalid_argument(
                        "short unicode escape in preprocessing JSON"
                    );
                }
                unsigned codepoint = 0;
                for (unsigned i = 0; i < 4; ++i) {
                    const char digit = input_[position_++];
                    codepoint <<= 4U;
                    if (digit >= '0' && digit <= '9') {
                        codepoint += static_cast<unsigned>(digit - '0');
                    } else if (digit >= 'a' && digit <= 'f') {
                        codepoint += static_cast<unsigned>(digit - 'a' + 10);
                    } else if (digit >= 'A' && digit <= 'F') {
                        codepoint += static_cast<unsigned>(digit - 'A' + 10);
                    } else {
                        throw std::invalid_argument(
                            "invalid unicode escape in preprocessing JSON"
                        );
                    }
                }
                if (codepoint > 0x7FU) {
                    throw std::invalid_argument(
                        "non-ASCII preprocessing IDs are unsupported"
                    );
                }
                value += static_cast<char>(codepoint);
                break;
            }
            default:
                throw std::invalid_argument(
                    "invalid escape in preprocessing JSON"
                );
            }
        }
        throw std::invalid_argument(
            "unterminated preprocessing JSON string"
        );
    }

    [[nodiscard]] std::uint32_t number() {
        skip_space();
        if (position_ >= input_.size()
            || !std::isdigit(static_cast<unsigned char>(input_[position_]))) {
            throw std::invalid_argument("invalid preprocessing JSON number");
        }
        if (input_[position_] == '0') {
            ++position_;
            if (position_ < input_.size()
                && std::isdigit(
                    static_cast<unsigned char>(input_[position_])
                )) {
                throw std::invalid_argument(
                    "leading zero in preprocessing JSON number"
                );
            }
            return 0;
        }
        std::uint32_t value = 0;
        while (position_ < input_.size()
               && std::isdigit(
                   static_cast<unsigned char>(input_[position_])
               )) {
            const unsigned digit =
                static_cast<unsigned>(input_[position_++] - '0');
            if (value > (UINT32_MAX - digit) / 10U) {
                throw std::invalid_argument(
                    "preprocessing JSON number is too large"
                );
            }
            value = value * 10U + digit;
        }
        return value;
    }

    void string_array(std::vector<std::string>& output) {
        expect('[');
        skip_space();
        if (consume(']')) return;
        while (true) {
            output.push_back(string());
            skip_space();
            if (consume(']')) return;
            expect(',');
        }
    }

    [[nodiscard]] bool consume(char value) {
        skip_space();
        if (position_ >= input_.size() || input_[position_] != value) {
            return false;
        }
        ++position_;
        return true;
    }

    void finish() {
        skip_space();
        if (position_ != input_.size()) {
            throw std::invalid_argument(
                "trailing content in preprocessing JSON"
            );
        }
    }

private:
    void skip_space() {
        while (position_ < input_.size()
               && std::isspace(
                   static_cast<unsigned char>(input_[position_])
               )) {
            ++position_;
        }
    }

    std::string_view input_{};
    std::size_t position_{0};
};

void append_json_string(std::string& output, std::string_view value) {
    output += '"';
    for (char ch : value) {
        switch (ch) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (static_cast<unsigned char>(ch) < 0x20U) {
                constexpr char kHex[] = "0123456789abcdef";
                output += "\\u00";
                output += kHex[(static_cast<unsigned char>(ch) >> 4U) & 0xfU];
                output += kHex[static_cast<unsigned char>(ch) & 0xfU];
            } else {
                output += ch;
            }
        }
    }
    output += '"';
}

} // namespace

void PreprocessorRegistry::add(std::string id, Factory factory) {
    if (id.empty()) {
        throw std::invalid_argument("preprocessor id must not be empty");
    }
    if (!factory) {
        throw std::invalid_argument("preprocessor factory must not be empty");
    }
    if (!factories_.emplace(std::move(id), std::move(factory)).second) {
        throw std::invalid_argument("duplicate preprocessor id");
    }
}

std::shared_ptr<const Preprocessor> PreprocessorRegistry::create(
    std::string_view id) const {
    const auto found = factories_.find(std::string(id));
    if (found == factories_.end()) return {};
    std::shared_ptr<const Preprocessor> preprocessor = found->second();
    if (!preprocessor || preprocessor->id() != id) {
        throw std::logic_error("preprocessor factory returned an invalid stage");
    }
    return preprocessor;
}

PreprocessorRegistry make_reference_preprocessor_registry() {
    PreprocessorRegistry registry;
    registry.add(
        std::string(kExcludeInvalidPreprocessorId),
        [] { return std::make_shared<ExcludeInvalid>(); }
    );
    registry.add(
        std::string(kNormalizeSidePreprocessorId),
        [] { return std::make_shared<NormalizeSideToMove>(); }
    );
    registry.add(
        std::string(kExpandFileMirrorPreprocessorId),
        [] { return std::make_shared<ExpandFileMirror>(); }
    );
    registry.add(
        std::string(kDeduplicatePreprocessorId),
        [] { return std::make_shared<Deduplicate>(); }
    );
    return registry;
}

PreprocessingPipelineConfig make_reference_preprocessing_pipeline_config() {
    return PreprocessingPipelineConfig{{
        std::string(kExcludeInvalidPreprocessorId),
        std::string(kNormalizeSidePreprocessorId),
        std::string(kExpandFileMirrorPreprocessorId),
        std::string(kDeduplicatePreprocessorId),
    }};
}

std::string serialize_preprocessing_pipeline_config(
    const PreprocessingPipelineConfig& config) {
    std::string output = "{\"schema\":";
    append_json_string(output, kPreprocessingConfigSchema);
    output += ",\"version\":"
        + std::to_string(kPreprocessingConfigVersion)
        + ",\"steps\":[";
    for (std::size_t i = 0; i < config.steps.size(); ++i) {
        if (i != 0) output += ',';
        append_json_string(output, config.steps[i]);
    }
    output += "]}";
    return output;
}

PreprocessingPipelineConfig deserialize_preprocessing_pipeline_config(
    std::string_view json) {
    JsonReader reader{json};
    reader.expect('{');

    std::optional<std::string> schema;
    std::optional<std::uint32_t> version;
    std::optional<std::vector<std::string>> steps;
    if (!reader.consume('}')) {
        while (true) {
            const std::string key = reader.string();
            reader.expect(':');
            if (key == "schema") {
                if (schema.has_value()) {
                    throw std::invalid_argument("duplicate schema field");
                }
                schema = reader.string();
            } else if (key == "version") {
                if (version.has_value()) {
                    throw std::invalid_argument("duplicate version field");
                }
                version = reader.number();
            } else if (key == "steps") {
                if (steps.has_value()) {
                    throw std::invalid_argument("duplicate steps field");
                }
                steps.emplace();
                reader.string_array(*steps);
            } else {
                throw std::invalid_argument(
                    "unknown preprocessing config field: " + key
                );
            }

            if (reader.consume('}')) break;
            reader.expect(',');
        }
    }
    reader.finish();

    if (!schema.has_value() || *schema != kPreprocessingConfigSchema) {
        throw std::invalid_argument(
            "unexpected preprocessing config schema"
        );
    }
    if (!version.has_value() || *version != kPreprocessingConfigVersion) {
        throw std::invalid_argument(
            "unsupported preprocessing config version"
        );
    }
    if (!steps.has_value()) {
        throw std::invalid_argument(
            "preprocessing config is missing steps"
        );
    }
    for (const std::string& step : *steps) {
        if (step.empty()) {
            throw std::invalid_argument(
                "preprocessing step id must not be empty"
            );
        }
    }
    return PreprocessingPipelineConfig{std::move(*steps)};
}

PreprocessingPipeline::PreprocessingPipeline(
    PreprocessingPipelineConfig config,
    const PreprocessorRegistry& registry,
    ParsedRecordSink& downstream)
    : downstream_(&downstream) {
    steps_.reserve(config.steps.size());
    for (const std::string& id : config.steps) {
        std::shared_ptr<const Preprocessor> step = registry.create(id);
        if (!step) {
            throw std::invalid_argument(
                "unknown preprocessor id: " + id
            );
        }
        steps_.push_back(std::move(step));
    }
    context_.summary = &summary_;
}

void PreprocessingPipeline::on_record(ParsedRecord record) {
    ++summary_.input_records;
    std::vector<ParsedRecord> current;
    current.push_back(std::move(record));
    std::vector<ParsedRecord> next;

    for (const auto& step : steps_) {
        next.clear();
        for (const ParsedRecord& item : current) {
            step->apply(item, next, context_);
        }
        current.swap(next);
        if (current.empty()) break;
    }

    for (ParsedRecord& item : current) {
        downstream_->on_record(std::move(item));
        ++summary_.emitted_records;
    }
}

void PreprocessingPipeline::on_error(ParseError error) {
    ++summary_.forwarded_errors;
    downstream_->on_error(std::move(error));
}

void PreprocessingPipeline::reset() {
    summary_ = {};
    context_.seen_records.clear();
}

} // namespace kadoka::shogi::training
