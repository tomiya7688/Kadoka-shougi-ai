#include "kadoka/model_registry/model_registry.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <variant>

namespace kadoka::model_registry {
namespace {

constexpr std::string_view schema = "kadoka.model_manifest.v1";
constexpr std::uintmax_t max_manifest_bytes = std::uintmax_t{1024} * 1024U;

// {
//   責務: [valid_key: ファイル名にも使うID・版を安全な文字へ限定する],
//   処理: [1: 長さ・先頭・許可文字・連続dotを確認する],
//   引数: [value: IDまたは版],
//   戻り値: [valid: 利用可能ならtrue]
// }
bool valid_key(const std::string_view value) {
    if (value.empty() || value.size() > 128 || value.find("..") != std::string_view::npos)
        return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char c = value[index];
        const bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        const bool digit = c >= '0' && c <= '9';
        if (!letter && !digit && (index == 0 || (c != '.' && c != '_' && c != '-')))
            return false;
    }
    return true;
}

// {
//   責務: [valid_utf8: JSON文字列の不正なUTF-8符号化を拒否する],
//   引数: [value: 検証するbyte列],
//   戻り値: [valid: Unicode scalar valueだけならtrue]
// }
bool valid_utf8(const std::string_view value) {
    for (std::size_t offset = 0; offset < value.size();) {
        const auto lead = static_cast<unsigned char>(value[offset]);
        if (lead < 0x80U) {
            ++offset;
            continue;
        }
        const std::size_t followers = lead >= 0xC2U && lead <= 0xDFU   ? 1
                                      : lead >= 0xE0U && lead <= 0xEFU ? 2
                                      : lead >= 0xF0U && lead <= 0xF4U ? 3
                                                                       : 0;
        if (followers == 0 || followers >= value.size() - offset)
            return false;
        const auto second = static_cast<unsigned char>(value[offset + 1]);
        if (second < 0x80U || second > 0xBFU || (lead == 0xE0U && second < 0xA0U) ||
            (lead == 0xEDU && second > 0x9FU) || (lead == 0xF0U && second < 0x90U) || (lead == 0xF4U && second > 0x8FU))
            return false;
        for (std::size_t index = 2; index <= followers; ++index) {
            const auto continuation = static_cast<unsigned char>(value[offset + index]);
            if (continuation < 0x80U || continuation > 0xBFU)
                return false;
        }
        offset += followers + 1;
    }
    return true;
}

// {
//   責務: [append_utf8: JSONのUnicode scalar valueをUTF-8へ変換する],
//   処理: [1: 値域に応じて1～4 byteを追加する],
//   引数: [output: 文字列] [codepoint: Unicode値],
//   戻り値: []
// }
void append_utf8(std::string& output, const std::uint32_t codepoint) {
    if (codepoint <= 0x7FU) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (codepoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (codepoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        output.push_back(static_cast<char>(0xF0U | (codepoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codepoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

// {
//   責務: [JsonReader: v1の平坦なJSON objectを型と重複を保って読む],
//   フィールド: [input_: 入力JSON] [position_: 次の読取位置]
// }
class JsonReader {
  public:
    // {
    //   責務: [JsonReader: 入力文字列と読取位置を保持する],
    //   処理: [1: 初期位置を0にする],
    //   引数: [input: JSON文字列],
    //   戻り値: [reader: 読取器]
    // }
    explicit JsonReader(const std::string_view input) : input_(input) {}

    // {
    //   責務: [parse: 1個の平坦なJSON objectを最後まで読む],
    //   処理: [1: keyと値を読み重複を拒否する] [2: 末尾の余剰文字を拒否する],
    //   引数: [],
    //   戻り値: [fields: 文字列または非負整数の全フィールド]
    // }
    [[nodiscard]] std::map<std::string, std::variant<std::string, std::uint64_t>> parse() {
        std::map<std::string, std::variant<std::string, std::uint64_t>> fields;
        expect('{');
        if (!consume('}')) {
            do {
                const auto key = read_string();
                expect(':');
                skip_space();
                const auto value = position_ < input_.size() && input_[position_] == '"'
                                       ? std::variant<std::string, std::uint64_t>(read_string())
                                       : std::variant<std::string, std::uint64_t>(read_number());
                if (!fields.emplace(key, value).second)
                    throw std::invalid_argument("duplicate model manifest field");
                if (consume('}'))
                    break;
                expect(',');
            } while (true);
        }
        skip_space();
        if (position_ != input_.size())
            throw std::invalid_argument("trailing model manifest data");
        return fields;
    }

  private:
    // {
    //   責務: [skip_space: JSONで認める空白だけを飛ばす],
    //   処理: [1: space、tab、CR、LFを読み飛ばす],
    //   引数: [],
    //   戻り値: []
    // }
    void skip_space() {
        while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\t' ||
                                             input_[position_] == '\r' || input_[position_] == '\n'))
            ++position_;
    }

    // {
    //   責務: [consume: 期待文字があれば1文字読む],
    //   処理: [1: 空白後の文字を照合する],
    //   引数: [symbol: 期待文字],
    //   戻り値: [matched: 一致したか]
    // }
    bool consume(const char symbol) {
        skip_space();
        if (position_ >= input_.size() || input_[position_] != symbol)
            return false;
        ++position_;
        return true;
    }

    // {
    //   責務: [expect: JSON区切り文字を必須として読む],
    //   処理: [1: 不一致なら例外を投げる],
    //   引数: [symbol: 必須文字],
    //   戻り値: []
    // }
    void expect(const char symbol) {
        if (!consume(symbol))
            throw std::invalid_argument("invalid model manifest JSON punctuation");
    }

    // {
    //   責務: [read_hex: Unicode escapeの4桁を読む],
    //   処理: [1: 4桁の16進値を検証する],
    //   引数: [],
    //   戻り値: [value: code unit]
    // }
    std::uint32_t read_hex() {
        if (input_.size() - position_ < 4)
            throw std::invalid_argument("truncated Unicode escape");
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const char c = input_[position_++];
            value <<= 4U;
            if (c >= '0' && c <= '9')
                value |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                value |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                value |= static_cast<std::uint32_t>(c - 'A' + 10);
            else
                throw std::invalid_argument("invalid Unicode escape");
        }
        return value;
    }

    // {
    //   責務: [read_string: JSON stringをescapeとsurrogateを含めて復元する],
    //   処理: [1: 制御文字を拒否する] [2: escapeを復号する],
    //   引数: [],
    //   戻り値: [value: UTF-8文字列]
    // }
    std::string read_string() {
        expect('"');
        std::string value;
        while (position_ < input_.size()) {
            const char c = input_[position_++];
            if (c == '"') {
                if (!valid_utf8(value))
                    throw std::invalid_argument("invalid model manifest UTF-8");
                return value;
            }
            if (static_cast<unsigned char>(c) < 0x20U)
                throw std::invalid_argument("unescaped control character");
            if (c != '\\') {
                value.push_back(c);
                continue;
            }
            if (position_ >= input_.size())
                throw std::invalid_argument("truncated JSON escape");
            const char escaped = input_[position_++];
            if (escaped == 'u') {
                std::uint32_t codepoint = read_hex();
                if (codepoint >= 0xD800U && codepoint <= 0xDBFFU) {
                    if (input_.size() - position_ < 2)
                        throw std::invalid_argument("missing low surrogate");
                    const char slash = input_[position_];
                    const char marker = input_[position_ + 1];
                    position_ += 2;
                    if (slash != '\\' || marker != 'u')
                        throw std::invalid_argument("missing low surrogate");
                    const auto low = read_hex();
                    if (low < 0xDC00U || low > 0xDFFFU)
                        throw std::invalid_argument("invalid low surrogate");
                    codepoint = 0x10000U + ((codepoint - 0xD800U) << 10U) + (low - 0xDC00U);
                } else if (codepoint >= 0xDC00U && codepoint <= 0xDFFFU) {
                    throw std::invalid_argument("unpaired low surrogate");
                }
                append_utf8(value, codepoint);
            } else if (escaped == '"' || escaped == '\\' || escaped == '/') {
                value.push_back(escaped);
            } else if (escaped == 'n') {
                value.push_back('\n');
            } else if (escaped == 'r') {
                value.push_back('\r');
            } else if (escaped == 't') {
                value.push_back('\t');
            } else if (escaped == 'b') {
                value.push_back('\b');
            } else if (escaped == 'f') {
                value.push_back('\f');
            } else {
                throw std::invalid_argument("invalid JSON escape");
            }
        }
        throw std::invalid_argument("unterminated JSON string");
    }

    // {
    //   責務: [read_number: 非負10進整数だけを読む],
    //   処理: [1: 桁を確認する] [2: 64bit範囲を検証する],
    //   引数: [],
    //   戻り値: [value: 読み取った整数]
    // }
    std::uint64_t read_number() {
        skip_space();
        const auto start = position_;
        if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
            throw std::invalid_argument("model manifest value must be a string or nonnegative integer");
        if (input_[position_] == '0' && position_ + 1 < input_.size() && input_[position_ + 1] >= '0' &&
            input_[position_ + 1] <= '9')
            throw std::invalid_argument("leading zero in JSON integer");
        while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9')
            ++position_;
        std::uint64_t value = 0;
        const auto result = std::from_chars(input_.data() + start, input_.data() + position_, value);
        if (result.ec != std::errc{})
            throw std::invalid_argument("model manifest integer out of range");
        return value;
    }

    std::string_view input_;
    std::size_t position_ = 0;
};

using Fields = std::map<std::string, std::variant<std::string, std::uint64_t>>;

// {
//   責務: [required_string: 必須JSON stringを取り出す],
//   処理: [1: keyと型を確認する],
//   引数: [fields: JSON object] [key: 必須key],
//   戻り値: [value: 文字列]
// }
std::string required_string(const Fields& fields, const char* key) {
    const auto it = fields.find(key);
    if (it == fields.end() || !std::holds_alternative<std::string>(it->second))
        throw std::invalid_argument(std::string("missing or invalid model manifest field: ") + key);
    return std::get<std::string>(it->second);
}

// {
//   責務: [required_number: 必須JSON整数を取り出す],
//   処理: [1: keyと型を確認する],
//   引数: [fields: JSON object] [key: 必須key],
//   戻り値: [value: 非負整数]
// }
std::uint64_t required_number(const Fields& fields, const char* key) {
    const auto it = fields.find(key);
    if (it == fields.end() || !std::holds_alternative<std::uint64_t>(it->second))
        throw std::invalid_argument(std::string("missing or invalid model manifest field: ") + key);
    return std::get<std::uint64_t>(it->second);
}

// {
//   責務: [json_string: 任意UTF-8文字列をJSON stringへescapeする],
//   処理: [1: quote、backslash、制御文字をescapeする],
//   引数: [value: 元文字列],
//   戻り値: [json: quoted JSON文字列]
// }
std::string json_string(const std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string output{"\""};
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') {
            output.push_back('\\');
            output.push_back(static_cast<char>(c));
        } else if (c < 0x20U) {
            output += "\\u00";
            output.push_back(hex[c >> 4U]);
            output.push_back(hex[c & 0x0FU]);
        } else {
            output.push_back(static_cast<char>(c));
        }
    }
    output.push_back('"');
    return output;
}

// {
//   責務: [manifest_path: 安全なIDと版を1ファイル名へ変換する],
//   処理: [1: IDと版を検証する] [2: 固定拡張子を付ける],
//   引数: [directory: registry保存先] [model_id: ID] [model_version: 版],
//   戻り値: [path: manifest保存先]
// }
std::filesystem::path manifest_path(const std::filesystem::path& directory, const std::string_view model_id,
                                    const std::string_view model_version) {
    if (!valid_key(model_id) || !valid_key(model_version))
        throw std::invalid_argument("invalid model registry key");
    return directory / (std::string(model_id) + "@" + std::string(model_version) + ".json");
}

// {
//   責務: [read_manifest_file: サイズ上限付きで保存済みmanifestを読む],
//   処理: [1: 上限を検証する] [2: 全内容を読む] [3: manifestを解析する],
//   引数: [path: ファイルパス],
//   戻り値: [manifest: 復元結果]
// }
ModelManifest read_manifest_file(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size > max_manifest_bytes)
        throw std::runtime_error("model manifest file exceeds size limit");
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open model manifest");
    std::string json(static_cast<std::size_t>(size), '\0');
    if (!input.read(json.data(), static_cast<std::streamsize>(size)))
        throw std::runtime_error("cannot read complete model manifest");
    return manifest_from_json(json);
}

} // namespace

// {
//   責務: [validate_manifest: manifest v1の識別子と必須メタデータを検証する],
//   処理: [1: 安全なID・版を確認する] [2: 配布元・license・hash・資源を確認する],
//   引数: [manifest: 登録対象],
//   戻り値: []
// }
void validate_manifest(const ModelManifest& manifest) {
    if (!valid_key(manifest.model_id) || !valid_key(manifest.model_version) || !valid_key(manifest.model_type) ||
        !valid_key(manifest.artifact_format) || !valid_key(manifest.runtime_id) ||
        manifest.artifact_format_version == 0 || manifest.api_version == 0 || manifest.min_threads == 0 ||
        manifest.source_reference.empty() || manifest.source_reference.size() > 4096 ||
        !valid_utf8(manifest.source_reference) || manifest.source_reference.find('\0') != std::string::npos ||
        manifest.license.empty() || manifest.license.size() > 4096 || !valid_utf8(manifest.license) ||
        manifest.license.find('\0') != std::string::npos)
        throw std::invalid_argument("invalid model manifest required field");
    if (manifest.source_kind != "bundled" && manifest.source_kind != "downloaded" &&
        manifest.source_kind != "user_trained" && manifest.source_kind != "external")
        throw std::invalid_argument("unsupported model source kind");
    if (manifest.artifact_sha256.size() != 64 ||
        !std::all_of(manifest.artifact_sha256.begin(), manifest.artifact_sha256.end(),
                     [](const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        throw std::invalid_argument("artifact SHA-256 must be 64 lowercase hex digits");
}

// {
//   責務: [manifest_to_json: 検証済みmanifestを固定順JSONにする],
//   処理: [1: 値を検証する] [2: 全フィールドを固定順でescapeして書く],
//   引数: [manifest: 元データ],
//   戻り値: [json: manifest v1 JSON]
// }
std::string manifest_to_json(const ModelManifest& manifest) {
    validate_manifest(manifest);
    return "{\n  \"schema\": " + json_string(schema) + ",\n  \"model_id\": " + json_string(manifest.model_id) +
           ",\n  \"model_version\": " + json_string(manifest.model_version) +
           ",\n  \"model_type\": " + json_string(manifest.model_type) +
           ",\n  \"artifact_format\": " + json_string(manifest.artifact_format) +
           ",\n  \"artifact_format_version\": " + std::to_string(manifest.artifact_format_version) +
           ",\n  \"api_version\": " + std::to_string(manifest.api_version) +
           ",\n  \"artifact_sha256\": " + json_string(manifest.artifact_sha256) +
           ",\n  \"source_kind\": " + json_string(manifest.source_kind) +
           ",\n  \"source_reference\": " + json_string(manifest.source_reference) +
           ",\n  \"license\": " + json_string(manifest.license) +
           ",\n  \"runtime_id\": " + json_string(manifest.runtime_id) +
           ",\n  \"min_memory_mb\": " + std::to_string(manifest.min_memory_mb) +
           ",\n  \"min_threads\": " + std::to_string(manifest.min_threads) + "\n}\n";
}

// {
//   責務: [manifest_from_json: 厳格なschema v1 JSONをmanifestへ変換する],
//   処理: [1: JSONと14フィールドを解析する] [2: 型と値を検証する],
//   引数: [json: 元JSON],
//   戻り値: [manifest: 復元値]
// }
ModelManifest manifest_from_json(const std::string_view json) {
    const auto fields = JsonReader(json).parse();
    if (fields.size() != 14 || required_string(fields, "schema") != schema)
        throw std::invalid_argument("model manifest schema or field count mismatch");
    ModelManifest manifest;
    manifest.model_id = required_string(fields, "model_id");
    manifest.model_version = required_string(fields, "model_version");
    manifest.model_type = required_string(fields, "model_type");
    manifest.artifact_format = required_string(fields, "artifact_format");
    const auto format_version = required_number(fields, "artifact_format_version");
    const auto api_version = required_number(fields, "api_version");
    const auto min_threads = required_number(fields, "min_threads");
    if (format_version > std::numeric_limits<std::uint32_t>::max() ||
        api_version > std::numeric_limits<std::uint32_t>::max() ||
        min_threads > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("model manifest version or threads out of range");
    manifest.artifact_format_version = static_cast<std::uint32_t>(format_version);
    manifest.api_version = static_cast<std::uint32_t>(api_version);
    manifest.min_threads = static_cast<std::uint32_t>(min_threads);
    manifest.artifact_sha256 = required_string(fields, "artifact_sha256");
    manifest.source_kind = required_string(fields, "source_kind");
    manifest.source_reference = required_string(fields, "source_reference");
    manifest.license = required_string(fields, "license");
    manifest.runtime_id = required_string(fields, "runtime_id");
    manifest.min_memory_mb = required_number(fields, "min_memory_mb");
    validate_manifest(manifest);
    return manifest;
}

// {
//   責務: [check_compatibility: API・artifact形式・runtime・資源を順に照合する],
//   処理: [1: manifestを検証する] [2: 最初の不一致理由を返す],
//   引数: [manifest: 対象model] [target: 使用環境],
//   戻り値: [result: 互換判定]
// }
CompatibilityResult check_compatibility(const ModelManifest& manifest, const CompatibilityTarget& target) {
    validate_manifest(manifest);
    if (target.api_version != manifest.api_version)
        return {false, "api_version"};
    if (!target.formats.contains({manifest.artifact_format, manifest.artifact_format_version}))
        return {false, "artifact_format"};
    if (target.runtime_id != manifest.runtime_id)
        return {false, "runtime_id"};
    if (target.memory_mb < manifest.min_memory_mb)
        return {false, "min_memory_mb"};
    if (target.threads < manifest.min_threads)
        return {false, "min_threads"};
    return {true, {}};
}

// {
//   責務: [LocalModelRegistry: 保存先を準備する],
//   処理: [1: directoryを作る],
//   引数: [directory: registry専用保存先],
//   戻り値: [registry: 保存先]
// }
LocalModelRegistry::LocalModelRegistry(std::filesystem::path directory) : directory_(std::move(directory)) {
    std::filesystem::create_directories(directory_);
}

// {
//   責務: [add: 重複ID・版を拒否しmanifestを保存する],
//   処理: [1: JSONを生成する] [2: 一時ファイルを書き確定名へrenameする],
//   引数: [manifest: 登録対象],
//   戻り値: []
// }
void LocalModelRegistry::add(const ModelManifest& manifest) {
    const auto json = manifest_to_json(manifest);
    const auto path = manifest_path(directory_, manifest.model_id, manifest.model_version);
    if (std::filesystem::exists(std::filesystem::symlink_status(path)))
        throw std::invalid_argument("model version is already registered");
    const auto temporary = std::filesystem::path(path.string() + ".tmp");
    // {
    //   責務: [単一writerの前回異常終了で残った一時ファイルを回収する],
    //   処理: [シンボリックリンクならリンク自体を削除する]
    // }
    std::filesystem::remove(temporary);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("cannot create model manifest temporary file");
        output.write(json.data(), static_cast<std::streamsize>(json.size()));
        output.close();
        if (!output)
            throw std::runtime_error("cannot write model manifest");
        std::filesystem::rename(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

// {
//   責務: [remove: 指定版の保存ファイルを削除する],
//   処理: [1: 安全なkeyを生成する] [2: 対象を削除する],
//   引数: [model_id: ID] [model_version: 版],
//   戻り値: [removed: 削除したか]
// }
bool LocalModelRegistry::remove(const std::string_view model_id, const std::string_view model_version) {
    return std::filesystem::remove(manifest_path(directory_, model_id, model_version));
}

// {
//   責務: [get: 指定したID・版のmanifestを復元して照合する],
//   処理: [1: ファイルを読む] [2: 内容の識別子をkeyと比較する],
//   引数: [model_id: ID] [model_version: 版],
//   戻り値: [manifest: 見つかった場合の内容]
// }
std::optional<ModelManifest> LocalModelRegistry::get(const std::string_view model_id,
                                                     const std::string_view model_version) const {
    const auto path = manifest_path(directory_, model_id, model_version);
    if (!std::filesystem::exists(path))
        return std::nullopt;
    auto manifest = read_manifest_file(path);
    if (manifest.model_id != model_id || manifest.model_version != model_version)
        throw std::runtime_error("model manifest filename and identity differ");
    return manifest;
}

// {
//   責務: [list: 保存済みmanifestをID・版順で返す],
//   処理: [1: JSONファイルだけを読む] [2: 内容と名前を照合する] [3: 並べ替える],
//   引数: [],
//   戻り値: [manifests: 登録済み全件]
// }
std::vector<ModelManifest> LocalModelRegistry::list() const {
    std::vector<ModelManifest> manifests;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (entry.path().extension() != ".json")
            continue;
        auto manifest = read_manifest_file(entry.path());
        if (manifest_path(directory_, manifest.model_id, manifest.model_version) != entry.path())
            throw std::runtime_error("model manifest filename and identity differ");
        manifests.push_back(std::move(manifest));
    }
    std::sort(manifests.begin(), manifests.end(), [](const ModelManifest& left, const ModelManifest& right) {
        return std::tie(left.model_id, left.model_version) < std::tie(right.model_id, right.model_version);
    });
    return manifests;
}

} // namespace kadoka::model_registry
