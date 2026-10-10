#include "kadoka/best/manifest.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <iomanip>
#include <initializer_list>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace kadoka::best {
namespace {

// {
//   責務: [JsonValue: JSON値を型を保って解析・検証する]
// }
struct JsonValue {
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;
    // {
    //   責務: [Number: JSON numberの構文を解析後も文字列表記で保持する]
    // }
    struct Number {
        std::string spelling;
    };
    using Value = std::variant<std::nullptr_t, bool, Number, std::string, Array, Object>;

    Value value;
};

// {
//   責務: [JsonParser: v1 manifestのJSONを完全に読み取り重複keyや不正構文を拒否する]
// }
class JsonParser {
public:
    // {
    //   責務: [JsonParser: 入力JSONのviewと初期位置を保持する]
    //   引数: [input: 解析対象のJSON]
    // }
    explicit JsonParser(std::string_view input) : input_(input) {}

    // {
    //   責務: [parse: JSON値を解析し末尾以降の余分な文字を拒否する]
    //   戻り値: [解析済みJSON値]
    // }
    JsonValue parse() {
        skip_space();
        JsonValue result = parse_value();
        skip_space();
        if (position_ != input_.size()) {
            fail("trailing characters after JSON value");
        }
        return result;
    }

private:
    // {
    //   責務: [skip_space: JSONで許可された空白を読み飛ばす]
    // }
    void skip_space() {
        while (position_ < input_.size()) {
            const char value = input_[position_];
            if (value != ' ' && value != '\t' && value != '\n' && value != '\r') {
                break;
            }
            ++position_;
        }
    }

    // {
    //   責務: [fail: 現在位置を含むJSON解析エラーを送出する]
    //   引数: [message: エラー理由]
    // }
    [[noreturn]] void fail(std::string_view message) const {
        throw std::runtime_error(
            "manifest JSON at byte " + std::to_string(position_) + ": " +
            std::string(message)
        );
    }

    // {
    //   責務: [consume: 指定文字が次にあるときだけ入力位置を進める]
    //   引数: [expected: 確認する文字]
    //   戻り値: [文字を消費した場合true]
    // }
    bool consume(char expected) {
        if (position_ >= input_.size() || input_[position_] != expected) {
            return false;
        }
        ++position_;
        return true;
    }

    // {
    //   責務: [require: 指定された構文文字が無い場合に解析を停止する]
    //   引数: [expected: 必須の構文文字]
    // }
    void require(char expected) {
        if (!consume(expected)) {
            fail(std::string("expected '") + expected + "'");
        }
    }

    // {
    //   責務: [append_utf8: Unicode code pointをUTF-8文字列へ追加する]
    //   引数: [output: 追加先文字列] [code_point: 追加する文字]
    // }
    void append_utf8(std::string& output, std::uint32_t code_point) {
        if (code_point <= 0x7fU) {
            output.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7ffU) {
            output.push_back(static_cast<char>(0xc0U | (code_point >> 6U)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
        } else if (code_point <= 0xffffU) {
            output.push_back(static_cast<char>(0xe0U | (code_point >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
        } else if (code_point <= 0x10ffffU) {
            output.push_back(static_cast<char>(0xf0U | (code_point >> 18U)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3fU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3fU)));
        } else {
            fail("unicode code point is out of range");
        }
    }

    // {
    //   責務: [read_hex4: JSONのunicode escapeから16bit値を読む]
    //   戻り値: [読み取った16bit値]
    // }
    std::uint32_t read_hex4() {
        std::uint32_t result = 0;
        for (int index = 0; index < 4; ++index) {
            if (position_ >= input_.size()) {
                fail("incomplete unicode escape");
            }
            const char digit = input_[position_++];
            result <<= 4U;
            if (digit >= '0' && digit <= '9') {
                result |= static_cast<std::uint32_t>(digit - '0');
            } else if (digit >= 'a' && digit <= 'f') {
                result |= static_cast<std::uint32_t>(digit - 'a' + 10);
            } else if (digit >= 'A' && digit <= 'F') {
                result |= static_cast<std::uint32_t>(digit - 'A' + 10);
            } else {
                fail("invalid unicode escape");
            }
        }
        return result;
    }

    // {
    //   責務: [parse_string: escapeとUTF-8を検査しJSON文字列を読む]
    //   戻り値: [復号した文字列]
    // }
    std::string parse_string() {
        require('"');
        std::string output;
        while (position_ < input_.size()) {
            const unsigned char current = static_cast<unsigned char>(input_[position_++]);
            if (current == '"') {
                return output;
            }
            if (current < 0x20U) {
                fail("unescaped control character in string");
            }
            if (current != '\\') {
                output.push_back(static_cast<char>(current));
                continue;
            }
            if (position_ >= input_.size()) {
                fail("incomplete string escape");
            }
            const char escape = input_[position_++];
            switch (escape) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    std::uint32_t code_point = read_hex4();
                    if (code_point >= 0xd800U && code_point <= 0xdbffU) {
                        if (position_ + 2 > input_.size() || input_[position_] != '\\' ||
                            input_[position_ + 1] != 'u') {
                            fail("high surrogate must be followed by low surrogate");
                        }
                        position_ += 2;
                        const std::uint32_t low = read_hex4();
                        if (low < 0xdc00U || low > 0xdfffU) {
                            fail("invalid low surrogate");
                        }
                        code_point = 0x10000U + ((code_point - 0xd800U) << 10U) +
                                     (low - 0xdc00U);
                    } else if (code_point >= 0xdc00U && code_point <= 0xdfffU) {
                        fail("unexpected low surrogate");
                    }
                    append_utf8(output, code_point);
                    break;
                }
                default: fail("invalid string escape");
            }
        }
        fail("unterminated string");
    }

    // {
    //   責務: [parse_object: 重複keyを拒否しJSON objectを読み取る]
    //   戻り値: [keyと値のmap]
    // }
    JsonValue::Object parse_object() {
        require('{');
        skip_space();
        JsonValue::Object result;
        if (consume('}')) {
            return result;
        }
        while (true) {
            skip_space();
            if (position_ >= input_.size() || input_[position_] != '"') {
                fail("object key must be a string");
            }
            std::string key = parse_string();
            skip_space();
            require(':');
            skip_space();
            if (!result.emplace(std::move(key), parse_value()).second) {
                fail("duplicate object key");
            }
            skip_space();
            if (consume('}')) {
                return result;
            }
            require(',');
        }
    }

    // {
    //   責務: [parse_array: JSON arrayの要素を順番どおりに読み取る]
    //   戻り値: [解析した要素一覧]
    // }
    JsonValue::Array parse_array() {
        require('[');
        skip_space();
        JsonValue::Array result;
        if (consume(']')) {
            return result;
        }
        while (true) {
            skip_space();
            result.push_back(parse_value());
            skip_space();
            if (consume(']')) {
                return result;
            }
            require(',');
        }
    }

    // {
    //   責務: [parse_number: JSON numberの文法を検査して数値表記を保持する]
    //   戻り値: [解析済み数値表記]
    // }
    JsonValue parse_number() {
        const std::size_t start = position_;
        consume('-');
        if (consume('0')) {
            if (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                fail("leading zero in number");
            }
        } else {
            if (position_ >= input_.size() || input_[position_] < '1' || input_[position_] > '9') {
                fail("invalid number");
            }
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
        }
        if (consume('.')) {
            const std::size_t fraction = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
            if (fraction == position_) fail("fraction has no digits");
        }
        if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
            ++position_;
            if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) {
                ++position_;
            }
            const std::size_t exponent = position_;
            while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
                ++position_;
            }
            if (exponent == position_) fail("exponent has no digits");
        }
        return JsonValue{JsonValue::Number{std::string(input_.substr(start, position_ - start))}};
    }

    // {
    //   責務: [parse_value: JSONのobject、array、scalar値を再帰的に解析する]
    //   戻り値: [解析済みJSON値]
    // }
    JsonValue parse_value() {
        if (position_ >= input_.size()) fail("expected JSON value");
        const char current = input_[position_];
        if (current == '{') return JsonValue{parse_object()};
        if (current == '[') return JsonValue{parse_array()};
        if (current == '"') return JsonValue{parse_string()};
        if (current == '-' || (current >= '0' && current <= '9')) return parse_number();
        if (input_.substr(position_, 4) == "true") {
            position_ += 4;
            return JsonValue{true};
        }
        if (input_.substr(position_, 5) == "false") {
            position_ += 5;
            return JsonValue{false};
        }
        if (input_.substr(position_, 4) == "null") {
            position_ += 4;
            return JsonValue{nullptr};
        }
        fail("invalid JSON value");
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

// {
//   責務: [escape_json: JSON文字列の特殊文字を安全にescapeする]
//   引数: [value: escape対象文字列]
//   戻り値: [JSON文字列リテラル]
// }
std::string escape_json(std::string_view value) {
    std::string output;
    output.reserve(value.size() + 2);
    output.push_back('"');
    constexpr char digits[] = "0123456789abcdef";
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20U) {
                    output += "\\u00";
                    output.push_back(digits[character >> 4U]);
                    output.push_back(digits[character & 0x0fU]);
                } else {
                    output.push_back(static_cast<char>(character));
                }
        }
    }
    output.push_back('"');
    return output;
}

// {
//   責務: [require_object: JSON値がobjectであることを確認する]
//   引数: [value: 型を確認するJSON値] [field: error表示用field名]
//   戻り値: [objectへの参照]
// }
const JsonValue::Object& require_object(const JsonValue& value, std::string_view field) {
    const auto* result = std::get_if<JsonValue::Object>(&value.value);
    if (result == nullptr) throw std::runtime_error(std::string(field) + " must be an object");
    return *result;
}

// {
//   責務: [require_array: JSON値がarrayであることを確認する]
//   引数: [value: 型を確認するJSON値] [field: error表示用field名]
//   戻り値: [arrayへの参照]
// }
const JsonValue::Array& require_array(const JsonValue& value, std::string_view field) {
    const auto* result = std::get_if<JsonValue::Array>(&value.value);
    if (result == nullptr) throw std::runtime_error(std::string(field) + " must be an array");
    return *result;
}

// {
//   責務: [require_string: JSON値がstringであることを確認する]
//   引数: [value: 型を確認するJSON値] [field: error表示用field名]
//   戻り値: [文字列への参照]
// }
const std::string& require_string(const JsonValue& value, std::string_view field) {
    const auto* result = std::get_if<std::string>(&value.value);
    if (result == nullptr) throw std::runtime_error(std::string(field) + " must be a string");
    return *result;
}

// {
//   責務: [require_field: object内の必須fieldを取得する]
//   引数: [object: 検索するobject] [name: 必須field名]
//   戻り値: [field値への参照]
// }
const JsonValue& require_field(const JsonValue::Object& object, std::string_view name) {
    const auto found = object.find(std::string(name));
    if (found == object.end()) throw std::runtime_error("missing manifest field: " + std::string(name));
    return found->second;
}

// {
//   責務: [reject_unknown_fields: schema外のfieldによるsilent data lossを防ぐ]
//   引数: [object: 検査するobject] [allowed: 許可field名一覧]
// }
void reject_unknown_fields(
    const JsonValue::Object& object,
    std::initializer_list<std::string_view> allowed
) {
    for (const auto& [key, value] : object) {
        (void)value;
        bool known = false;
        for (const std::string_view name : allowed) known = known || key == name;
        if (!known) throw std::runtime_error("unknown manifest field: " + key);
    }
}

// {
//   責務: [parse_layer: manifestのlayer文字列をLayerTypeへ変換する]
//   引数: [value: layer名]
//   戻り値: [変換した層]
// }
LayerType parse_layer(std::string_view value) {
    if (value == "raw") return LayerType::raw;
    if (value == "integrated") return LayerType::integrated;
    if (value == "full") return LayerType::full;
    throw std::runtime_error("layer must be raw, integrated, or full");
}

// {
//   責務: [parse_role: manifestのcomponent roleをenumへ変換する]
//   引数: [value: role名]
//   戻り値: [変換した役割]
// }
ComponentRole parse_role(std::string_view value) {
    if (value == "evaluation") return ComponentRole::evaluation;
    if (value == "search") return ComponentRole::search;
    if (value == "opening") return ComponentRole::opening;
    if (value == "endgame") return ComponentRole::endgame;
    if (value == "orchestrator") return ComponentRole::orchestrator;
    throw std::runtime_error("unknown component role: " + std::string(value));
}

// {
//   責務: [parse_artifact: IDとversionを持つartifact objectを復元する]
//   引数: [value: artifact JSON値] [field: error表示用field名]
//   戻り値: [復元したartifact参照]
// }
ArtifactReference parse_artifact(const JsonValue& value, std::string_view field) {
    const auto& object = require_object(value, field);
    reject_unknown_fields(object, {"id", "version"});
    return {
        require_string(require_field(object, "id"), "artifact.id"),
        require_string(require_field(object, "version"), "artifact.version")
    };
}

// {
//   責務: [canonical_config: config mapを安定key順のJSON objectにする]
//   引数: [config: 正規化する設定map]
//   戻り値: [canonical JSON]
// }
std::string canonical_config(const std::map<std::string, std::string>& config) {
    std::string output = "{";
    bool first = true;
    for (const auto& [key, value] : config) {
        if (!first) output.push_back(',');
        first = false;
        output += escape_json(key);
        output.push_back(':');
        output += escape_json(value);
    }
    output.push_back('}');
    return output;
}

// {
//   責務: [sha256: canonical configのbyte列をSHA-256で要約する]
//   引数: [input: hash対象byte列]
//   戻り値: [小文字hex 64桁]
// }
std::string sha256(std::string_view input) {
    constexpr std::array<std::uint32_t, 64> constants = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    std::array<std::uint32_t, 8> state = {
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };
    if (input.size() > (std::numeric_limits<std::uint64_t>::max() / 8U)) {
        throw std::length_error("config is too large for SHA-256");
    }
    std::vector<std::uint8_t> bytes(input.begin(), input.end());
    const std::uint64_t bit_length = static_cast<std::uint64_t>(bytes.size()) * 8U;
    bytes.push_back(0x80U);
    while ((bytes.size() % 64U) != 56U) bytes.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    }
    for (std::size_t offset = 0; offset < bytes.size(); offset += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index = 0; index < 16; ++index) {
            const std::size_t at = offset + index * 4U;
            words[index] = (static_cast<std::uint32_t>(bytes[at]) << 24U) |
                           (static_cast<std::uint32_t>(bytes[at + 1]) << 16U) |
                           (static_cast<std::uint32_t>(bytes[at + 2]) << 8U) |
                           static_cast<std::uint32_t>(bytes[at + 3]);
        }
        for (std::size_t index = 16; index < words.size(); ++index) {
            const std::uint32_t s0 = std::rotr(words[index - 15], 7) ^
                                     std::rotr(words[index - 15], 18) ^ (words[index - 15] >> 3U);
            const std::uint32_t s1 = std::rotr(words[index - 2], 17) ^
                                     std::rotr(words[index - 2], 19) ^ (words[index - 2] >> 10U);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::uint32_t a=state[0], b=state[1], c=state[2], d=state[3];
        std::uint32_t e=state[4], f=state[5], g=state[6], h=state[7];
        for (std::size_t index = 0; index < words.size(); ++index) {
            const std::uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temp1 = h + s1 + choice + constants[index] + words[index];
            const std::uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;
            h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const std::uint32_t word : state) output << std::setw(8) << word;
    return output.str();
}

// {
//   責務: [append_artifact: artifact参照を安定順序のJSONへ追加する]
//   引数: [output: JSON出力先] [artifact: 追加する参照]
// }
void append_artifact(std::string& output, const ArtifactReference& artifact) {
    output += "{\"id\":" + escape_json(artifact.id) + ",\"version\":" +
              escape_json(artifact.version) + "}";
}

// {
//   責務: [parse_manifest_object: JSON objectをmanifest構造へ復元する]
//   引数: [root: manifest root object]
//   戻り値: [復元したmanifest]
// }
KadokaBestManifest parse_manifest_object(const JsonValue::Object& root) {
    reject_unknown_fields(root, {"format", "layer", "package_version", "model", "checkpoint",
                                 "source_layer", "target_layer", "components", "config", "config_hash"});
    KadokaBestManifest result;
    result.format = require_string(require_field(root, "format"), "format");
    result.layer = parse_layer(require_string(require_field(root, "layer"), "layer"));
    result.package_version = require_string(require_field(root, "package_version"), "package_version");
    const JsonValue& model = require_field(root, "model");
    if (!std::holds_alternative<std::nullptr_t>(model.value)) result.model = parse_artifact(model, "model");
    const JsonValue& checkpoint = require_field(root, "checkpoint");
    if (!std::holds_alternative<std::nullptr_t>(checkpoint.value)) {
        result.checkpoint = parse_artifact(checkpoint, "checkpoint");
    }
    const JsonValue& source = require_field(root, "source_layer");
    if (!std::holds_alternative<std::nullptr_t>(source.value)) {
        result.source_layer = parse_layer(require_string(source, "source_layer"));
    }
    const JsonValue& target = require_field(root, "target_layer");
    if (!std::holds_alternative<std::nullptr_t>(target.value)) {
        result.target_layer = parse_layer(require_string(target, "target_layer"));
    }
    for (const JsonValue& item : require_array(require_field(root, "components"), "components")) {
        const auto& component = require_object(item, "component");
        reject_unknown_fields(component, {"role", "id", "version"});
        result.components.push_back({
            parse_role(require_string(require_field(component, "role"), "component.role")),
            require_string(require_field(component, "id"), "component.id"),
            require_string(require_field(component, "version"), "component.version")
        });
    }
    for (const auto& [key, item] : require_object(require_field(root, "config"), "config")) {
        result.config.emplace(key, require_string(item, "config value"));
    }
    result.config_hash = require_string(require_field(root, "config_hash"), "config_hash");
    return result;
}

}  // namespace

std::string_view layer_type_name(LayerType layer) noexcept {
    switch (layer) {
        case LayerType::raw: return "raw";
        case LayerType::integrated: return "integrated";
        case LayerType::full: return "full";
    }
    return "invalid";
}

std::string_view component_role_name(ComponentRole role) noexcept {
    switch (role) {
        case ComponentRole::evaluation: return "evaluation";
        case ComponentRole::search: return "search";
        case ComponentRole::opening: return "opening";
        case ComponentRole::endgame: return "endgame";
        case ComponentRole::orchestrator: return "orchestrator";
    }
    return "invalid";
}

std::string compute_config_hash(const std::map<std::string, std::string>& config) {
    return "sha256:" + sha256(canonical_config(config));
}

void refresh_config_hash(KadokaBestManifest& manifest) {
    manifest.config_hash = compute_config_hash(manifest.config);
}

std::vector<std::string> validate_manifest(const KadokaBestManifest& manifest) {
    std::vector<std::string> errors;
    if (manifest.format != "kadoka.best_manifest.v1") errors.emplace_back("format must be kadoka.best_manifest.v1");
    if (layer_type_name(manifest.layer) == "invalid") errors.emplace_back("layer is invalid");
    if (manifest.package_version.empty()) errors.emplace_back("package_version must not be empty");
    if (manifest.model && (manifest.model->id.empty() || manifest.model->version.empty())) {
        errors.emplace_back("model id and version must not be empty");
    }
    if (manifest.checkpoint && (manifest.checkpoint->id.empty() || manifest.checkpoint->version.empty())) {
        errors.emplace_back("checkpoint id and version must not be empty");
    }
    if (manifest.checkpoint && !manifest.model) errors.emplace_back("checkpoint requires a model reference");
    if (manifest.layer == LayerType::raw && !manifest.model) errors.emplace_back("raw layer requires a model reference");
    if (manifest.layer == LayerType::integrated && !manifest.model && manifest.components.empty()) {
        errors.emplace_back("integrated layer requires a model or component reference");
    }
    if (manifest.layer == LayerType::full) {
        bool has_orchestrator = false;
        bool has_engine_component = false;
        for (const auto& component : manifest.components) {
            has_orchestrator = has_orchestrator || component.role == ComponentRole::orchestrator;
            has_engine_component = has_engine_component || component.role == ComponentRole::search ||
                                   component.role == ComponentRole::evaluation;
        }
        if (!has_orchestrator) errors.emplace_back("full layer requires an orchestrator component");
        if (!has_engine_component) errors.emplace_back("full layer requires evaluation or search component");
    }
    if (manifest.source_layer.has_value() != manifest.target_layer.has_value()) {
        errors.emplace_back("source_layer and target_layer must be provided together");
    }
    if ((manifest.source_layer && layer_type_name(*manifest.source_layer) == "invalid") ||
        (manifest.target_layer && layer_type_name(*manifest.target_layer) == "invalid")) {
        errors.emplace_back("source_layer and target_layer must name a known layer");
    }
    std::array<bool, 5> roles{};
    for (const auto& component : manifest.components) {
        if (component.component_id.empty()) errors.emplace_back("component id must not be empty");
        if (component.version.empty()) errors.emplace_back("component version must not be empty");
        const std::string_view name = component_role_name(component.role);
        if (name == "invalid") {
            errors.emplace_back("component role is invalid");
        } else {
            const std::size_t index = static_cast<std::size_t>(component.role);
            if (roles[index]) errors.emplace_back("component roles must be unique");
            roles[index] = true;
        }
    }
    for (const auto& [key, value] : manifest.config) {
        if (key.empty()) errors.emplace_back("config key must not be empty");
        (void)value;
    }
    if (manifest.config_hash != compute_config_hash(manifest.config)) {
        errors.emplace_back("config_hash does not match canonical config");
    }
    return errors;
}

std::string serialize_manifest(const KadokaBestManifest& manifest) {
    const std::vector<std::string> errors = validate_manifest(manifest);
    if (!errors.empty()) throw std::invalid_argument("invalid manifest: " + errors.front());
    std::string output = "{\"format\":" + escape_json(manifest.format) +
                         ",\"layer\":" + escape_json(layer_type_name(manifest.layer)) +
                         ",\"package_version\":" + escape_json(manifest.package_version) +
                         ",\"model\":";
    if (manifest.model) append_artifact(output, *manifest.model); else output += "null";
    output += ",\"checkpoint\":";
    if (manifest.checkpoint) append_artifact(output, *manifest.checkpoint); else output += "null";
    output += ",\"source_layer\":";
    if (manifest.source_layer) output += escape_json(layer_type_name(*manifest.source_layer)); else output += "null";
    output += ",\"target_layer\":";
    if (manifest.target_layer) output += escape_json(layer_type_name(*manifest.target_layer)); else output += "null";
    output += ",\"components\":[";
    std::vector<ComponentReference> components = manifest.components;
    std::sort(components.begin(), components.end(), [](const auto& left, const auto& right) {
        if (left.role != right.role) return left.role < right.role;
        if (left.component_id != right.component_id) return left.component_id < right.component_id;
        return left.version < right.version;
    });
    for (std::size_t index = 0; index < components.size(); ++index) {
        if (index != 0) output.push_back(',');
        output += "{\"role\":" + escape_json(component_role_name(components[index].role)) +
                  ",\"id\":" + escape_json(components[index].component_id) +
                  ",\"version\":" + escape_json(components[index].version) + "}";
    }
    output += "],\"config\":" + canonical_config(manifest.config) +
              ",\"config_hash\":" + escape_json(manifest.config_hash) + "}";
    return output;
}

KadokaBestManifest deserialize_manifest(std::string_view json) {
    JsonValue root = JsonParser(json).parse();
    KadokaBestManifest result = parse_manifest_object(require_object(root, "manifest"));
    const std::vector<std::string> errors = validate_manifest(result);
    if (!errors.empty()) throw std::invalid_argument("invalid manifest: " + errors.front());
    return result;
}

}  // namespace kadoka::best

