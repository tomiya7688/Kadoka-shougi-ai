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

// {
//   責務: [JsonValue: JSON tokenを型と値に分けて保持する]
//   フィールド: [kind: JSON型 / scalar: scalar値 / object: keyと値 / array: 要素]
// }
struct JsonValue {
    JsonKind kind{JsonKind::Null};
    std::string scalar{};
    std::map<std::string, JsonValue> object{};
    std::vector<JsonValue> array{};
};

// {
//   責務: [JsonParser: JSON文字列をrecipe用の値木へ解析する]
//   フィールド: [input_: 入力view / position_: 読み取り位置]
// }
class JsonParser {
public:
// {
//   責務: [JsonParser: 入力JSONを解析対象として受け取る]
//   処理: [入力viewを保存する]
//   引数: [input: 読み取るJSON]
//   戻り値: [なし]
// }
    explicit JsonParser(std::string_view input) : input_(input) {}

// {
//   責務: [parse: JSON全体を一つの値として解析する]
//   処理: [空白を除去し、値を解析し、末尾の余分な文字を拒否する]
//   引数: [なし]
//   戻り値: [JsonValue。不正入力はinvalid_argument]
// }
    JsonValue parse() {
        skip_space();
        JsonValue result = parse_value();
        skip_space();
        if (position_ != input_.size()) fail("trailing data");
        return result;
    }

private:
// {
//   責務: [fail: 解析失敗を統一した例外として通知する]
//   処理: [recipe JSONの文脈を付けて例外を送出する]
//   引数: [message: 失敗理由]
//   戻り値: [戻らない]
// }
    [[noreturn]] static void fail(const char* message) {
        throw std::invalid_argument(std::string("training recipe JSON: ") + message);
    }

// {
//   責務: [skip_space: JSONで許可される空白を読み飛ばす]
//   処理: [空白が続く範囲まで位置を進める]
//   引数: [なし]
//   戻り値: [なし]
// }
    void skip_space() {
        while (position_ < input_.size()) {
            const char ch = input_[position_];
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') break;
            ++position_;
        }
    }

// {
//   責務: [take: 次の入力文字を読み取る]
//   処理: [末尾を確認して1文字進める]
//   引数: [なし]
//   戻り値: [読み取ったchar。末尾ならinvalid_argument]
// }
    char take() {
        if (position_ >= input_.size()) fail("unexpected end of input");
        return input_[position_++];
    }

// {
//   責務: [consume: 次の文字が指定文字なら消費する]
//   処理: [一致時だけ位置を進める]
//   引数: [expected: 期待する文字]
//   戻り値: [一致したか]
// }
    bool consume(char expected) {
        if (position_ < input_.size() && input_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }

// {
//   責務: [expect: 次の入力文字が指定値か確認する]
//   処理: [consume失敗時は解析エラーにする]
//   引数: [expected: 期待する文字]
//   戻り値: [なし。不一致ならinvalid_argument]
// }
    void expect(char expected) {
        if (!consume(expected)) fail("unexpected token");
    }

// {
//   責務: [parse_value: 次のJSON値を型に応じて解析する]
//   処理: [先頭文字からstring/object/array/literal/numberへ分岐する]
//   引数: [なし]
//   戻り値: [JsonValue。不正入力ならinvalid_argument]
// }
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

// {
//   責務: [parse_literal: JSON固定literalを読み取る]
//   処理: [各文字を照合して指定型を返す]
//   引数: [literal: 期待する語 / kind: 結果型]
//   戻り値: [JsonValue。不一致ならinvalid_argument]
// }
    JsonValue parse_literal(const char* literal, JsonKind kind) {
        for (const char* cursor = literal; *cursor != '\0'; ++cursor) {
            if (take() != *cursor) fail("invalid literal");
        }
        JsonValue result;
        result.kind = kind;
        result.scalar = literal;
        return result;
    }

// {
//   責務: [append_utf8: Unicode code pointをUTF-8へ追加する]
//   処理: [値の範囲に応じた1〜4 byteを出力する]
//   引数: [output: 出力先 / codepoint: 追加値]
//   戻り値: [なし]
// }
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

// {
//   責務: [parse_hex_quad: 4桁のUnicode escapeを整数として読む]
//   処理: [各hex桁を検証しながら累積する]
//   引数: [なし]
//   戻り値: [code point。不正桁ならinvalid_argument]
// }
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

// {
//   責務: [parse_string: JSON文字列をdecodeする]
//   処理: [escapeを処理しUnicodeをUTF-8へ変換する]
//   引数: [なし]
//   戻り値: [string。不正escapeならinvalid_argument]
// }
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
                // A high surrogate is valid only with a following low surrogate.
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

// {
//   責務: [parse_string_value: 文字列をJSON値として包む]
//   処理: [文字列をString型へ格納する]
//   引数: [なし]
//   戻り値: [String型JsonValue]
// }
    JsonValue parse_string_value() {
        JsonValue result;
        result.kind = JsonKind::String;
        result.scalar = parse_string();
        return result;
    }

// {
//   責務: [parse_number: JSON number tokenを文法検証して読む]
//   処理: [符号、小数部、指数部を走査する]
//   引数: [なし]
//   戻り値: [元表記を保持したNumber型JsonValue]
// }
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

// {
//   責務: [parse_object: JSON objectを重複key検査付きで解析する]
//   処理: [key/value組を読み構文と重複を検査する]
//   引数: [なし]
//   戻り値: [Object型JsonValue]
// }
    JsonValue parse_object() {
        expect('{');
        JsonValue result;
        result.kind = JsonKind::Object;
        skip_space();
        if (consume('}')) return result;
        while (true) {
            skip_space();
            if (position_ >= input_.size() || input_[position_] != '"') fail("object key must be a string");
            // Duplicate JSON keys are rejected instead of silently shadowed.
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

// {
//   責務: [parse_array: JSON arrayを順序を保って解析する]
//   処理: [値を閉じ括弧まで順に読む]
//   引数: [なし]
//   戻り値: [Array型JsonValue]
// }
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

// {
//   責務: [require_field: objectから必須fieldを取得する]
//   処理: [object型とfield存在を確認する]
//   引数: [object: 親値 / name: field名]
//   戻り値: [fieldへの参照。不正時invalid_argument]
// }
const JsonValue& require_field(const JsonValue& object, const char* name) {
    if (object.kind != JsonKind::Object) throw std::invalid_argument("training recipe must be a JSON object");
    const auto found = object.object.find(name);
    if (found == object.object.end()) {
        throw std::invalid_argument(std::string("training recipe missing field: ") + name);
    }
    return found->second;
}

// {
//   責務: [require_string: JSON値がstringであることを確認する]
//   処理: [型を確認してscalarを返す]
//   引数: [value: JSON値 / name: field名]
//   戻り値: [string。不一致ならinvalid_argument]
// }
std::string require_string(const JsonValue& value, const char* name) {
    if (value.kind != JsonKind::String) {
        throw std::invalid_argument(std::string("training recipe field must be a string: ") + name);
    }
    return value.scalar;
}

// {
//   責務: [require_number: JSON値をdoubleとして検証・変換する]
//   処理: [number型と完全な数値変換を確認する]
//   引数: [value: JSON値 / name: field名]
//   戻り値: [double。不正ならinvalid_argument]
// }
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

// {
//   責務: [require_unsigned_integer: JSON値をuint64整数として検証・変換する]
//   処理: [小数、指数、負数を拒否し範囲内へ変換する]
//   引数: [value: JSON値 / name: field名]
//   戻り値: [uint64_t。不正や範囲外ならinvalid_argument]
// }
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

// {
//   責務: [optional_string: nullまたはstringのoptional値を読む]
//   処理: [nullを空値、それ以外をstring検証する]
//   引数: [value: JSON値 / name: field名]
//   戻り値: [optional string。不正型ならinvalid_argument]
// }
std::optional<std::string> optional_string(const JsonValue& value, const char* name) {
    if (value.kind == JsonKind::Null) return std::nullopt;
    return require_string(value, name);
}

// {
//   責務: [append_json_string: 文字列をJSON stringとして出力へ追加する]
//   処理: [quoteと制御文字をescapeする]
//   引数: [output: 出力先 / value: 元文字列]
//   戻り値: [なし]
// }
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

// {
//   責務: [append_json_value: JSON値をcompactなcanonical表現で出力する]
//   処理: [object keyを辞書順、arrayを入力順、stringを規定escapeで出力する]
//   引数: [output: 出力先 / value: JSON値]
//   戻り値: [void]
// }
void append_json_value(std::string& output, const JsonValue& value) {
    switch (value.kind) {
    case JsonKind::Null:
    case JsonKind::Boolean:
    case JsonKind::Number:
        output += value.scalar;
        return;
    case JsonKind::String:
        append_json_string(output, value.scalar);
        return;
    case JsonKind::Object: {
        output.push_back('{');
        bool first = true;
        for (const auto& [key, item] : value.object) {
            if (!first) output.push_back(',');
            first = false;
            append_json_string(output, key);
            output.push_back(':');
            append_json_value(output, item);
        }
        output.push_back('}');
        return;
    }
    case JsonKind::Array: {
        output.push_back('[');
        for (std::size_t index = 0; index < value.array.size(); ++index) {
            if (index != 0) output.push_back(',');
            append_json_value(output, value.array[index]);
        }
        output.push_back(']');
        return;
    }
    }
}

// {
//   責務: [canonicalize_effective_config: 有効設定JSONをcanonical objectへ変換する]
//   処理: [objectとしてparseし、compactな辞書順表現へ再出力する]
//   引数: [json: 有効設定JSON]
//   戻り値: [canonical JSON。不正またはobject以外ならinvalid_argument]
// }
std::string canonicalize_effective_config(std::string_view json) {
    const JsonValue value = JsonParser(json).parse();
    if (value.kind != JsonKind::Object) {
        throw std::invalid_argument("effective_config must be a JSON object");
    }
    std::string canonical;
    append_json_value(canonical, value);
    return canonical;
}

// {
//   責務: [valid_config_hash: SHA-256文字列の書式を検証する]
//   処理: [prefix、長さ、小文字hex文字を確認する]
//   引数: [value: hash文字列]
//   戻り値: [正しい形式ならtrue]
// }
bool valid_config_hash(std::string_view value) {
    if (value.size() != 71 || value.substr(0, 7) != "sha256:") return false;
    return std::all_of(value.begin() + 7, value.end(), [](char ch) {
        return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    });
}

// {
//   責務: [append_number: doubleをlocale非依存のJSON numberとして追加する]
//   処理: [to_charsで変換する]
//   引数: [output: 出力先 / value: 数値]
//   戻り値: [なし。変換失敗ならinvalid_argument]
// }
void append_number(std::string& output, double value) {
    std::array<char, 64> buffer{};
    const auto formatted = std::to_chars(
        buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general
    );
    if (formatted.ec != std::errc{}) throw std::invalid_argument("could not format training recipe weight");
    output.append(buffer.data(), formatted.ptr);
}

// {
//   責務: [blank: 文字列が空または空白だけか判定する]
//   処理: [空白以外の文字の有無を確認する]
//   引数: [text: 判定対象]
//   戻り値: [空白だけならtrue]
// }
bool blank(std::string_view text) {
    return text.empty() || std::all_of(text.begin(), text.end(), [](unsigned char ch) {
        return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
    });
}

// {
//   責務: [require_nonempty: 必須文字列が空でないことを確認する]
//   処理: [空ならfield名付き例外を送出する]
//   引数: [value: 検証値 / field: 項目名]
//   戻り値: [なし]
// }
void require_nonempty(const std::string& value, const char* field) {
    if (blank(value)) throw std::invalid_argument(std::string("training recipe field is empty: ") + field);
}

// {
//   責務: [validate_weight: 重みが有限かつ許容範囲内か確認する]
//   処理: [[0, 1]範囲と有限性を検証する]
//   引数: [weight: 重み / field: 項目名]
//   戻り値: [なし。不正ならinvalid_argument]
// }
void validate_weight(double weight, const char* field) {
    if (!std::isfinite(weight) || weight < 0.0 || weight > 1.0) {
        throw std::invalid_argument(std::string("training recipe weight must be finite and in [0, 1]: ") + field);
    }
}

// {
//   責務: [parse_source_weight: source_weightsから指定重みを読む]
//   処理: [fieldを取得してnumber検証する]
//   引数: [source: source object / name: 重みfield]
//   戻り値: [double。不正ならinvalid_argument]
// }
double parse_source_weight(const JsonValue& source, const char* name) {
    return require_number(require_field(source, name), name);
}

} // namespace

void validate_training_recipe(const OfficialTrainingRecipe& recipe) {
    if (recipe.schema_version != 1) throw std::invalid_argument("unsupported training recipe schema version");
    if (recipe.datasets.empty()) throw std::invalid_argument("training recipe requires at least one dataset");
    require_nonempty(recipe.architecture_id, "architecture_id");
    require_nonempty(recipe.architecture_version, "architecture_version");
    require_nonempty(recipe.effective_config_json, "effective_config_json");
    if (canonicalize_effective_config(recipe.effective_config_json) != recipe.effective_config_json) {
        throw std::invalid_argument("effective_config must use canonical JSON encoding");
    }
    require_nonempty(recipe.config_hash, "config_hash");
    if (!valid_config_hash(recipe.config_hash)) {
        throw std::invalid_argument("config_hash must be sha256: followed by 64 lowercase hex digits");
    }

    std::vector<std::string_view> ids;
    ids.reserve(recipe.datasets.size());
    for (const auto& dataset : recipe.datasets) {
        require_nonempty(dataset.dataset_id, "dataset_id");
        require_nonempty(dataset.dataset_revision, "dataset_revision");
        if (!std::isfinite(dataset.weight) || dataset.weight <= 0.0) {
            throw std::invalid_argument("dataset weight must be finite and greater than zero");
        }
        ids.push_back(dataset.dataset_id);
    }
    // Sorting makes duplicate detection independent of dataset input order.
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
    std::span<const DatasetRegistryReference> registered_datasets
) {
    validate_training_recipe(recipe);
    for (const auto& dataset : recipe.datasets) {
        const bool found = std::any_of(
            registered_datasets.begin(), registered_datasets.end(), [&dataset](const auto& registered) {
                return registered.dataset_id == dataset.dataset_id
                    && registered.dataset_revision == dataset.dataset_revision;
            }
        );
        if (!found) {
            throw std::invalid_argument(
                "unknown dataset revision: " + dataset.dataset_id + "@" + dataset.dataset_revision
            );
        }
    }
}

std::string serialize_training_recipe(const OfficialTrainingRecipe& recipe) {
    validate_training_recipe(recipe);
    // Canonical dataset order keeps equivalent recipes byte-for-byte stable.
    std::vector<DatasetRecipeWeight> datasets = recipe.datasets;
    std::sort(datasets.begin(), datasets.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.dataset_id < rhs.dataset_id;
    });

    std::string output = "{\"schema_version\":1,\"datasets\":[";
    for (std::size_t index = 0; index < datasets.size(); ++index) {
        if (index != 0) output.push_back(',');
        output += "{\"dataset_id\":";
        append_json_string(output, datasets[index].dataset_id);
        output += ",\"dataset_revision\":";
        append_json_string(output, datasets[index].dataset_revision);
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
    output += ",\"effective_config\":";
    output += recipe.effective_config_json;
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
        dataset.dataset_revision = require_string(
            require_field(item, "dataset_revision"), "dataset_revision"
        );
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
    std::string canonical_effective_config;
    append_json_value(canonical_effective_config, require_field(root, "effective_config"));
    recipe.effective_config_json = std::move(canonical_effective_config);
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
    std::span<const DatasetRegistryReference> registered_datasets
) {
    OfficialTrainingRecipe recipe = deserialize_training_recipe(json);
    validate_training_recipe(recipe, registered_datasets);
    return recipe;
}

} // namespace kadoka::shogi::training
