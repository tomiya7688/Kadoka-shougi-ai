#include "kadoka/training/training_recipe.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace kadoka::shogi::training {
namespace {

enum class JsonKind { Null, Boolean, Number, String, Object, Array };

struct JsonValue {
    JsonKind kind{JsonKind::Null};
    std::string scalar{};
    std::map<std::string, JsonValue> object{};
    std::vector<JsonValue> array{};
};

class JsonParser {
public:
    explicit JsonParser(std::string_view input) : input_(input) {}

    JsonValue parse() {
        skip_space();
        JsonValue result = parse_value();
        skip_space();
        if (position_ != input_.size()) fail("trailing data");
        return result;
    }

private:
    [[noreturn]] static void fail(const char* message) {
        throw std::invalid_argument(std::string("training recipe JSON: ") + message);
    }

    void skip_space() {
        while (position_ < input_.size()) {
            const char ch = input_[position_];
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') break;
            ++position_;
        }
    }

    char take() {
        if (position_ >= input_.size()) fail("unexpected end of input");
        return input_[position_++];
    }

    bool consume(char expected) {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

    void expect(char expected) {
        if (!consume(expected)) fail("unexpected token");
    }

    JsonValue parse_value() {
        skip_space();
        if (position_ >= input_.size()) fail("missing value");
        const char ch = input_[position_];
        if (ch == '"') return parse_string_value();
        if (ch == '{') return parse_object();
        if (ch == '[') return parse_array();
        if (ch == 'n') return parse_literal("null", JsonKind::Null);
        if (ch == 't') return parse_literal("true", JsonKind::Boolean);
        if (ch == 'f') return parse_literal("false", JsonKind::Boolean);
        if (ch == '-' || (ch >= '0' && ch <= '9')) return parse_number();
        fail("invalid value");
    }

    JsonValue parse_literal(const char* literal, JsonKind kind) {
        for (const char* cursor = literal; *cursor != '\0'; ++cursor) {
            if (take() != *cursor) fail("invalid literal");
        }
        JsonValue result;
        result.kind = kind;
        return result;
    }

    static void append_utf8(std::string& output, std::uint32_t codepoint) {
        if (codepoint <= 0x7fU) {
            output.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ffU) {
            output.push_back(static_cast<char>(0xc0U | (codepoint >> 6U)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        } else if (codepoint <= 0xffffU) {
            output.push_back(static_cast<char>(0xe0U | (codepoint >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        } else {
            output.push_back(static_cast<char>(0xf0U | (codepoint >> 18U)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (codepoint & 0x3fU)));
        }
    }

    std::uint32_t parse_hex_quad() {
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const char ch = take();
            value <<= 4U;
            if (ch >= '0' && ch <= '9') value |= static_cast<std::uint32_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f') value |= static_cast<std::uint32_t>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F') value |= static_cast<std::uint32_t>(ch - 'A' + 10);
            else fail("invalid unicode escape");
        }
        return value;
    }

    std::string parse_string() {
        expect('"');
        std::string result;
        while (true) {
            const unsigned char ch = static_cast<unsigned char>(take());
            if (ch == '"') return result;
            if (ch < 0x20U) fail("unescaped control character");
            if (ch != '\\') {
                result.push_back(static_cast<char>(ch));
                continue;
            }
            switch (take()) {
            case '"': result.push_back('"'); break;
            case '\\': result.push_back('\\'); break;
            case '/': result.push_back('/'); break;
            case 'b': result.push_back('\b'); break;
            case 'f': result.push_back('\f'); break;
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case 'u': {
                std::uint32_t codepoint = parse_hex_quad();
                if (codepoint >= 0xd800U && codepoint <= 0xdbffU) {
                    if (take() != '\\' || take() != 'u') fail("invalid surrogate pair");
                    const std::uint32_t low = parse_hex_quad();
                    if (low < 0xdc00U || low > 0xdfffU) fail("invalid surrogate pair");
                    codepoint = 0x10000U + ((codepoint - 0xd800U) << 10U) + (low - 0xdc00U);
                } else if (codepoint >= 0xdc00U && codepoint <= 0xdfffU) {
                    fail("unpaired low surrogate");
                }
                append_utf8(result, codepoint);
                break;
            }
            default: fail("invalid string escape");
            }
        }
    }

    JsonValue parse_string_value() {
        JsonValue result;
        result.kind = JsonKind::String;
        result.scalar = parse_string();
        return result;
    }

    JsonValue parse_number() {
        const std::size_t start = position_;
        consume('-');
        if (!consume('0')) {
            if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9') {
                fail("invalid number");
            }
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }
        if (consume('.')) {
            const std::size_t fraction_start = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
            if (fraction_start == position_) fail("invalid fraction");
        }
        if (consume('e') || consume('E')) {
            if (!consume('+')) consume('-');
            const std::size_t exponent_start = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
            if (exponent_start == position_) fail("invalid exponent");
        }
        JsonValue result;
        result.kind = JsonKind::Number;
        result.scalar = std::string(input_.substr(start, position_ - start));
        return result;
    }

    JsonValue parse_object() {
        expect('{');
        JsonValue result;
        result.kind = JsonKind::Object;
        skip_space();
        if (consume('}')) return result;
        while (true) {
            skip_space();
            if (position_ >= input_.size() || input_[position_] != '"') fail("object key must be a string");
            std::string key = parse_string();
            skip_space();
            expect(':');
            JsonValue value = parse_value();
            if (!result.object.emplace(std::move(key), std::move(value)).second) {
                fail("duplicate object key");
            }
            skip_space();
            if (consume('}')) return result;
            expect(',');
        }
    }

    JsonValue parse_array() {
        expect('[');
        JsonValue result;
        result.kind = JsonKind::Array;
        skip_space();
        if (consume(']')) return result;
        while (true) {
            result.array.push_back(parse_value());
            skip_space();
            if (consume(']')) return result;
            expect(',');
        }
    }

    std::string_view input_;
    std::size_t position_{0};
};

const JsonValue& require_field(const JsonValue& object, const char* name) {
    if (object.kind != JsonKind::Object) throw std::invalid_argument("training recipe must be a JSON object");
    const auto found = object.object.find(name);
    if (found == object.object.end()) {
        throw std::invalid_argument(std::string("training recipe missing field: ") + name);
    }
    return found->second;
}

std::string require_string(const JsonValue& value, const char* name) {
    if (value.kind != JsonKind::String) {
        throw std::invalid_argument(std::string("training recipe field must be a string: ") + name);
    }
    return value.scalar;
}

double require_number(const JsonValue& value, const char* name) {
    if (value.kind != JsonKind::Number) {
        throw std::invalid_argument(std::string("training recipe field must be a number: ") + name);
    }
    double result = 0.0;
    const auto parsed = std::from_chars(
        value.scalar.data(), value.scalar.data() + value.scalar.size(), result,
        std::chars_format::general
    );
    if (parsed.ec != std::errc{} || parsed.ptr != value.scalar.data() + value.scalar.size()) {
        throw std::invalid_argument(std::string("invalid number in training recipe field: ") + name);
    }
    return result;
}

std::uint64_t require_unsigned_integer(const JsonValue& value, const char* name) {
    if (value.kind != JsonKind::Number || value.scalar.find_first_of(".eE-") != std::string::npos) {
        throw std::invalid_argument(std::string("training recipe field must be an unsigned integer: ") + name);
    }
    std::uint64_t result = 0;
    const auto parsed = std::from_chars(
        value.scalar.data(), value.scalar.data() + value.scalar.size(), result
    );
    if (parsed.ec != std::errc{} || parsed.ptr != value.scalar.data() + value.scalar.size()) {
        throw std::invalid_argument(std::string("invalid integer in training recipe field: ") + name);
    }
    return result;
}

std::optional<std::string> optional_string(const JsonValue& value, const char* name) {
    if (value.kind == JsonKind::Null) return std::nullopt;
    return require_string(value, name);
}

void append_json_string(std::string& output, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (ch < 0x20U) {
                output += "\\u00";
                output.push_back(hex[(ch >> 4U) & 0x0fU]);
                output.push_back(hex[ch & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(ch));
            }
        }
    }
    output.push_back('"');
}

void append_number(std::string& output, double value) {
    std::array<char, 64> buffer{};
    const auto formatted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general
    );
    if (formatted.ec != std::errc{}) throw std::invalid_argument("could not format training recipe weight");
    output.append(buffer.data(), formatted.ptr);
}

bool blank(std::string_view text) {
    return text.empty() || std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    });
}

void require_nonempty(const std::string& value, const char* field) {
    if (blank(value)) throw std::invalid_argument(std::string("training recipe field is empty: ") + field);
}

void validate_weight(double weight, const char* field) {
    if (!std::isfinite(weight) || weight < 0.0 || weight > 1.0) {
        throw std::invalid_argument(std::string("training recipe weight must be finite and in [0, 1]: ") + field);
    }
}

double parse_source_weight(const JsonValue& source, const char* name) {
    return require_number(require_field(source, name), name);
}

} // namespace

void validate_training_recipe(const OfficialTrainingRecipe& recipe) {
    if (recipe.schema_version != 1) throw std::invalid_argument("unsupported training recipe schema version");
    if (recipe.datasets.empty()) throw std::invalid_argument("training recipe requires at least one dataset");
    require_nonempty(recipe.architecture_id, "architecture_id");
    require_nonempty(recipe.architecture_version, "architecture_version");
    require_nonempty(recipe.config_hash, "config_hash");

    std::vector<std::string_view> ids;
    ids.reserve(recipe.datasets.size());
    for (const auto& dataset : recipe.datasets) {
        require_nonempty(dataset.dataset_id, "dataset_id");
        if (!std::isfinite(dataset.weight) || dataset.weight <= 0.0) {
            throw std::invalid_argument("dataset weight must be finite and greater than zero");
        }
        ids.push_back(dataset.dataset_id);
    }
    std::sort(ids.begin(), ids.end());
    if (std::adjacent_find(ids.begin(), ids.end()) != ids.end()) {
        throw std::invalid_argument("duplicate dataset id in training recipe");
    }

    const auto& weights = recipe.source_weights;
    validate_weight(weights.external, "source_weights.external");
    validate_weight(weights.human, "source_weights.human");
    validate_weight(weights.league, "source_weights.league");
    validate_weight(weights.self, "source_weights.self");
    const double total = weights.external + weights.human + weights.league + weights.self;
    if (std::abs(total - 1.0) > 1e-9) {
        throw std::invalid_argument("training recipe source weights must sum to one");
    }
    if (recipe.autotune_config_id) require_nonempty(*recipe.autotune_config_id, "autotune_config_id");
    if (recipe.evaluation_result_id) require_nonempty(*recipe.evaluation_result_id, "evaluation_result_id");
}

void validate_training_recipe(
    const OfficialTrainingRecipe& recipe,
    std::span<const std::string> registered_dataset_ids
) {
    validate_training_recipe(recipe);
    for (const auto& dataset : recipe.datasets) {
        const bool found = std::find(
            registered_dataset_ids.begin(), registered_dataset_ids.end(), dataset.dataset_id
        ) != registered_dataset_ids.end();
        if (!found) throw std::invalid_argument("unknown dataset reference: " + dataset.dataset_id);
    }
}

std::string serialize_training_recipe(const OfficialTrainingRecipe& recipe) {
    validate_training_recipe(recipe);
    std::vector<DatasetRecipeWeight> datasets = recipe.datasets;
    std::sort(datasets.begin(), datasets.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.dataset_id < rhs.dataset_id;
    });

    std::string output = "{\"schema_version\":1,\"datasets\":[";
    for (std::size_t index = 0; index < datasets.size(); ++index) {
        if (index != 0) output.push_back(',');
        output += "{\"dataset_id\":";
        append_json_string(output, datasets[index].dataset_id);
        output += ",\"weight\":";
        append_number(output, datasets[index].weight);
        output.push_back('}');
    }
    output += "],\"source_weights\":{\"external\":";
    append_number(output, recipe.source_weights.external);
    output += ",\"human\":";
    append_number(output, recipe.source_weights.human);
    output += ",\"league\":";
    append_number(output, recipe.source_weights.league);
    output += ",\"self\":";
    append_number(output, recipe.source_weights.self);
    output += "},\"architecture_id\":";
    append_json_string(output, recipe.architecture_id);
    output += ",\"architecture_version\":";
    append_json_string(output, recipe.architecture_version);
    output += ",\"config_hash\":";
    append_json_string(output, recipe.config_hash);
    output += ",\"champion_generation\":" + std::to_string(recipe.champion_generation);
    output += ",\"autotune_config_id\":";
    if (recipe.autotune_config_id) append_json_string(output, *recipe.autotune_config_id);
    else output += "null";
    output += ",\"evaluation_result_id\":";
    if (recipe.evaluation_result_id) append_json_string(output, *recipe.evaluation_result_id);
    else output += "null";
    output.push_back('}');
    return output;
}

OfficialTrainingRecipe deserialize_training_recipe(std::string_view json) {
    const JsonValue root = JsonParser(json).parse();
    OfficialTrainingRecipe recipe;
    const auto schema_version = require_unsigned_integer(require_field(root, "schema_version"), "schema_version");
    if (schema_version > UINT32_MAX) throw std::invalid_argument("training recipe schema version is out of range");
    recipe.schema_version = static_cast<std::uint32_t>(schema_version);

    const JsonValue& datasets = require_field(root, "datasets");
    if (datasets.kind != JsonKind::Array) throw std::invalid_argument("training recipe datasets must be an array");
    recipe.datasets.reserve(datasets.array.size());
    for (const auto& item : datasets.array) {
        DatasetRecipeWeight dataset;
        dataset.dataset_id = require_string(require_field(item, "dataset_id"), "dataset_id");
        dataset.weight = require_number(require_field(item, "weight"), "weight");
        recipe.datasets.push_back(std::move(dataset));
    }

    const JsonValue& source = require_field(root, "source_weights");
    recipe.source_weights.external = parse_source_weight(source, "external");
    recipe.source_weights.human = parse_source_weight(source, "human");
    recipe.source_weights.league = parse_source_weight(source, "league");
    recipe.source_weights.self = parse_source_weight(source, "self");
    recipe.architecture_id = require_string(require_field(root, "architecture_id"), "architecture_id");
    recipe.architecture_version = require_string(
        require_field(root, "architecture_version"), "architecture_version"
    );
    recipe.config_hash = require_string(require_field(root, "config_hash"), "config_hash");
    recipe.champion_generation = require_unsigned_integer(
        require_field(root, "champion_generation"), "champion_generation"
    );
    recipe.autotune_config_id = optional_string(require_field(root, "autotune_config_id"), "autotune_config_id");
    recipe.evaluation_result_id = optional_string(
        require_field(root, "evaluation_result_id"), "evaluation_result_id"
    );
    validate_training_recipe(recipe);
    return recipe;
}

OfficialTrainingRecipe deserialize_training_recipe(
    std::string_view json,
    std::span<const std::string> registered_dataset_ids
) {
    OfficialTrainingRecipe recipe = deserialize_training_recipe(json);
    validate_training_recipe(recipe, registered_dataset_ids);
    return recipe;
}

} // namespace kadoka::shogi::training
