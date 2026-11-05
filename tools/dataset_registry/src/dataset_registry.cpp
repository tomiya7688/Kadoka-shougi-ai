#include "kadoka/dataset/dataset_registry.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>

namespace kadoka::dataset {
namespace {

constexpr std::string_view kMagic = "kadoka.dataset_registry.v1\n";
constexpr std::uintmax_t kMaxRecordBytes = 1024ULL * 1024ULL;

// {
//   責務: [valid_id: 大小文字の衝突とWindows予約名を避けてファイル名を限定する],
//   引数: [value: 候補],
//   戻り値: [valid: 使用できる場合true]
// }
bool valid_id(std::string_view value) {
    if (value.empty() || value.size() > 128 || value == "." || value == "..") {
        return false;
    }
    if (value == "con" || value == "prn" || value == "aux" || value == "nul" ||
        (value.size() == 4 && (value.substr(0, 3) == "com" || value.substr(0, 3) == "lpt") && value[3] >= '1' &&
         value[3] <= '9')) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '_' ||
               character == '-';
    });
}

// {
//   責務: [valid_utf8_text: 不正なUTF-8符号化と制御文字を拒否する],
//   引数: [value: 検証するbyte列],
//   戻り値: [valid: 正しいUTF-8の表示用文字列ならtrue]
// }
bool valid_utf8_text(std::string_view value) {
    for (std::size_t offset = 0; offset < value.size();) {
        const auto lead = static_cast<unsigned char>(value[offset]);
        if (lead < 0x80) {
            if (lead < 0x20 || lead == 0x7f) {
                return false;
            }
            ++offset;
            continue;
        }
        const std::size_t followers = lead >= 0xc2 && lead <= 0xdf   ? 1
                                      : lead >= 0xe0 && lead <= 0xef ? 2
                                      : lead >= 0xf0 && lead <= 0xf4 ? 3
                                                                     : 0;
        if (followers == 0 || followers >= value.size() - offset) {
            return false;
        }
        const auto second = static_cast<unsigned char>(value[offset + 1]);
        if ((lead == 0xc2 && second < 0xa0) || (lead == 0xe0 && second < 0xa0) || (lead == 0xed && second > 0x9f) ||
            (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second > 0x8f)) {
            return false;
        }
        for (std::size_t index = 1; index <= followers; ++index) {
            const auto continuation = static_cast<unsigned char>(value[offset + index]);
            if (continuation < 0x80 || continuation > 0xbf) {
                return false;
            }
        }
        offset += followers + 1;
    }
    return true;
}

// {
//   責務: [valid_text: 自由記述欄の空値・UTF-8・過大入力を検証する],
//   引数: [value: 候補] [required: 空欄を許すか],
//   戻り値: [valid: 使用できる場合true]
// }
bool valid_text(std::string_view value, bool required) {
    return (!required || !value.empty()) && value.size() <= 4096 && valid_utf8_text(value);
}

// {
//   責務: [valid_hash: 空欄または小文字64桁SHA-256を認める],
//   引数: [hash: 設定hash],
//   戻り値: [valid: 形式が正しい場合true]
// }
bool valid_hash(std::string_view hash) {
    return hash.empty() || (hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](char digit) {
                                return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
                            }));
}

// {
//   責務: [valid_timestamp: 固定長UTC日時の数値と範囲を確認する],
//   引数: [value: YYYY-MM-DDTHH:MM:SSZ],
//   戻り値: [valid: 範囲内の場合true]
// }
bool valid_timestamp(std::string_view value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || value[19] != 'Z') {
        return false;
    }
    const std::array<std::size_t, 14> digits{0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    if (!std::all_of(digits.begin(), digits.end(),
                     [value](std::size_t pos) { return value[pos] >= '0' && value[pos] <= '9'; })) {
        return false;
    }
    const auto number = [value](std::size_t pos) { return 10 * (value[pos] - '0') + (value[pos + 1] - '0'); };
    const int month = number(5);
    const int day = number(8);
    const int year = 1000 * (value[0] - '0') + 100 * (value[1] - '0') + number(2);
    if (year == 0 || month < 1 || month > 12 || day < 1 || number(11) > 23 || number(14) > 59 || number(17) > 59) {
        return false;
    }
    const std::array<int, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    return day <= days[static_cast<std::size_t>(month - 1)] + (month == 2 && leap ? 1 : 0);
}

// {
//   責務: [role_name: 用途を永続形式の固定文字列へ変換する],
//   引数: [role: 用途],
//   戻り値: [name: v1の用途名]
// }
std::string_view role_name(DatasetRole role) {
    switch (role) {
    case DatasetRole::training:
        return "training";
    case DatasetRole::validation:
        return "validation";
    case DatasetRole::benchmark:
        return "benchmark";
    case DatasetRole::league_eval:
        return "league_eval";
    }
    throw std::invalid_argument("invalid dataset role");
}

// {
//   責務: [parse_role: 保存済み用途名を厳格に復元する],
//   引数: [name: 用途名],
//   戻り値: [role: 対応する用途]
// }
DatasetRole parse_role(std::string_view name) {
    if (name == "training")
        return DatasetRole::training;
    if (name == "validation")
        return DatasetRole::validation;
    if (name == "benchmark")
        return DatasetRole::benchmark;
    if (name == "league_eval")
        return DatasetRole::league_eval;
    throw std::invalid_argument("invalid dataset role");
}

// {
//   責務: [append_field: 長さ付きfieldを曖昧さなく保存する],
//   引数: [record: 書き込み先] [value: 保存値],
//   戻り値: []
// }
void append_field(std::string& record, std::string_view value) {
    record += std::to_string(value.size());
    record += ':';
    record.append(value);
}

// {
//   責務: [read_field: 長さ付きfieldを境界検査して読み取る],
//   引数: [record: 保存内容] [offset: 次の読み取り位置],
//   戻り値: [field: 読み取った値]
// }
std::string read_field(std::string_view record, std::size_t& offset) {
    const auto colon = record.find(':', offset);
    if (colon == std::string_view::npos || colon == offset || colon - offset > 6) {
        throw std::invalid_argument("invalid dataset field length");
    }
    std::size_t length = 0;
    const auto* start = record.data() + offset;
    const auto [end, error] = std::from_chars(start, record.data() + colon, length);
    if (error != std::errc{} || end != record.data() + colon || length > 4096 || length > record.size() - colon - 1) {
        throw std::invalid_argument("invalid dataset field length");
    }
    offset = colon + 1 + length;
    return std::string(record.substr(colon + 1, length));
}

// {
//   責務: [encode: 13項目を固定順のv1 recordへ変換する],
//   引数: [metadata: 保存対象],
//   戻り値: [record: バージョン付きrecord]
// }
std::string encode(const DatasetMetadata& metadata) {
    std::string record(kMagic);
    append_field(record, metadata.dataset_id);
    append_field(record, metadata.source_type);
    append_field(record, metadata.generator_model_id);
    append_field(record, metadata.generator_model_version);
    append_field(record, metadata.opponent_model_id);
    append_field(record, metadata.opponent_model_version);
    append_field(record, metadata.parent_dataset_id);
    append_field(record, std::to_string(metadata.generation));
    append_field(record, metadata.search_config_hash);
    append_field(record, metadata.training_config_hash);
    append_field(record, metadata.created_at);
    append_field(record, metadata.license);
    append_field(record, role_name(metadata.role));
    return record;
}

// {
//   責務: [decode: v1 recordの全項目と終端を検証して復元する],
//   引数: [record: 保存内容],
//   戻り値: [metadata: 復元したDataset]
// }
DatasetMetadata decode(std::string_view record) {
    if (!record.starts_with(kMagic)) {
        throw std::invalid_argument("unsupported dataset record version");
    }
    std::size_t offset = kMagic.size();
    DatasetMetadata metadata;
    metadata.dataset_id = read_field(record, offset);
    metadata.source_type = read_field(record, offset);
    metadata.generator_model_id = read_field(record, offset);
    metadata.generator_model_version = read_field(record, offset);
    metadata.opponent_model_id = read_field(record, offset);
    metadata.opponent_model_version = read_field(record, offset);
    metadata.parent_dataset_id = read_field(record, offset);
    const auto generation = read_field(record, offset);
    const auto [end, error] =
        std::from_chars(generation.data(), generation.data() + generation.size(), metadata.generation);
    if (error != std::errc{} || end != generation.data() + generation.size() || generation.empty()) {
        throw std::invalid_argument("invalid dataset generation");
    }
    metadata.search_config_hash = read_field(record, offset);
    metadata.training_config_hash = read_field(record, offset);
    metadata.created_at = read_field(record, offset);
    metadata.license = read_field(record, offset);
    metadata.role = parse_role(read_field(record, offset));
    if (offset != record.size()) {
        throw std::invalid_argument("trailing dataset record data");
    }
    validate_metadata(metadata);
    return metadata;
}

// {
//   責務: [record_path: 検証済みIDをregistry内のファイル名にする],
//   引数: [directory: 保存先] [id: Dataset ID],
//   戻り値: [path: 対応record]
// }
std::filesystem::path record_path(const std::filesystem::path& directory, const std::string& id) {
    if (!valid_id(id)) {
        throw std::invalid_argument("invalid dataset id");
    }
    return directory / (id + ".dataset");
}

// {
//   責務: [read_record: file種別と上限を確認して保存内容を読み取る],
//   引数: [path: 読み取り対象],
//   戻り値: [record: 内容]
// }
std::string read_record(const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(std::filesystem::symlink_status(path)) ||
        std::filesystem::file_size(path) > kMaxRecordBytes) {
        throw std::invalid_argument("invalid dataset record file");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open dataset record");
    }
    std::string contents(std::istreambuf_iterator<char>{input}, {});
    if (!input.eof() && input.fail()) {
        throw std::runtime_error("cannot read dataset record");
    }
    return contents;
}

} // namespace

// {
//   責務: [validate_metadata: Datasetの出自と保存可能な値域を検証する],
//   引数: [metadata: 検証対象],
//   戻り値: []
// }
void validate_metadata(const DatasetMetadata& metadata) {
    if (!valid_id(metadata.dataset_id) ||
        (metadata.source_type != "self_play" && metadata.source_type != "imported" &&
         metadata.source_type != "derived" && metadata.source_type != "external") ||
        !valid_text(metadata.generator_model_id, false) || !valid_text(metadata.generator_model_version, false) ||
        !valid_text(metadata.opponent_model_id, false) || !valid_text(metadata.opponent_model_version, false) ||
        metadata.generator_model_id.empty() != metadata.generator_model_version.empty() ||
        metadata.opponent_model_id.empty() != metadata.opponent_model_version.empty() ||
        (!metadata.parent_dataset_id.empty() && !valid_id(metadata.parent_dataset_id)) ||
        metadata.parent_dataset_id == metadata.dataset_id ||
        (metadata.source_type == "derived") != !metadata.parent_dataset_id.empty() ||
        !valid_hash(metadata.search_config_hash) || !valid_hash(metadata.training_config_hash) ||
        !valid_timestamp(metadata.created_at) || !valid_text(metadata.license, true)) {
        throw std::invalid_argument("invalid dataset metadata");
    }
    static_cast<void>(role_name(metadata.role));
    if (metadata.parent_dataset_id.empty() && metadata.generation != 0) {
        throw std::invalid_argument("root dataset generation must be zero");
    }
}

// {
//   責務: [DatasetRegistry: 保存先を作り以降の操作を限定する],
//   引数: [directory: registry専用保存先],
//   戻り値: [registry: 利用可能な保存先]
// }
DatasetRegistry::DatasetRegistry(std::filesystem::path directory) : directory_(std::move(directory)) {
    if (directory_.empty()) {
        throw std::invalid_argument("empty dataset registry directory");
    }
    std::filesystem::create_directories(directory_);
}

// {
//   責務: [add: 親と世代を照合し一時ファイルからrecordを公開する],
//   引数: [metadata: 登録対象],
//   戻り値: [added: 新規登録時true]
// }
bool DatasetRegistry::add(const DatasetMetadata& metadata) {
    validate_metadata(metadata);
    const auto destination = record_path(directory_, metadata.dataset_id);
    if (std::filesystem::exists(std::filesystem::symlink_status(destination))) {
        static_cast<void>(get(metadata.dataset_id));
        return false;
    }
    if (!metadata.parent_dataset_id.empty()) {
        const auto parent = get(metadata.parent_dataset_id);
        if (!parent || parent->generation == std::numeric_limits<std::uint32_t>::max() ||
            metadata.generation != parent->generation + 1) {
            throw std::invalid_argument("missing parent or invalid dataset generation");
        }
    }
    const auto temporary = directory_ / (metadata.dataset_id + ".tmp");
    // {
    //   責務: [前回の異常終了で残った一時ファイルを単一writerで回収する],
    //   処理: [シンボリックリンクなら参照先ではなくリンク自体を削除する]
    // }
    std::filesystem::remove(temporary);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create dataset record");
        }
        const auto record = encode(metadata);
        output.write(record.data(), static_cast<std::streamsize>(record.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("cannot write dataset record");
        }
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
    return true;
}

// {
//   責務: [remove: 子の参照を保護し存在するrecordを削除する],
//   引数: [dataset_id: 対象ID],
//   戻り値: [removed: 削除した場合true]
// }
bool DatasetRegistry::remove(const std::string& dataset_id) {
    if (!get(dataset_id)) {
        return false;
    }
    for (const auto& metadata : list()) {
        if (metadata.parent_dataset_id == dataset_id) {
            throw std::invalid_argument("dataset has children");
        }
    }
    return std::filesystem::remove(record_path(directory_, dataset_id));
}

// {
//   責務: [get: IDに対応するrecordのファイル名・祖先参照・世代を照合する],
//   引数: [dataset_id: 対象ID],
//   戻り値: [metadata: 存在する場合の内容]
// }
std::optional<DatasetMetadata> DatasetRegistry::get(const std::string& dataset_id) const {
    const auto path = record_path(directory_, dataset_id);
    if (!std::filesystem::exists(std::filesystem::symlink_status(path))) {
        return std::nullopt;
    }
    auto metadata = decode(read_record(path));
    if (metadata.dataset_id != dataset_id) {
        throw std::invalid_argument("dataset filename and content mismatch");
    }
    DatasetMetadata descendant = metadata;
    while (!descendant.parent_dataset_id.empty()) {
        const auto parent_path = record_path(directory_, descendant.parent_dataset_id);
        if (!std::filesystem::exists(std::filesystem::symlink_status(parent_path))) {
            throw std::invalid_argument("orphaned dataset record");
        }
        auto parent = decode(read_record(parent_path));
        if (parent.dataset_id != descendant.parent_dataset_id ||
            parent.generation == std::numeric_limits<std::uint32_t>::max() ||
            descendant.generation != parent.generation + 1) {
            throw std::invalid_argument("orphaned dataset record");
        }
        descendant = std::move(parent);
    }
    return metadata;
}

// {
//   責務: [list: v1 recordだけを読みID順に返す],
//   戻り値: [datasets: 登録済みDataset]
// }
std::vector<DatasetMetadata> DatasetRegistry::list() const {
    std::vector<DatasetMetadata> result;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        if (entry.path().extension() == ".dataset") {
            const auto id = entry.path().stem().string();
            auto metadata = decode(read_record(entry.path()));
            if (metadata.dataset_id != id) {
                throw std::invalid_argument("dataset filename and content mismatch");
            }
            result.push_back(std::move(metadata));
        }
    }
    std::sort(result.begin(), result.end(),
              [](const auto& left, const auto& right) { return left.dataset_id < right.dataset_id; });
    for (const auto& metadata : result) {
        if (metadata.parent_dataset_id.empty()) {
            continue;
        }
        const auto parent =
            std::lower_bound(result.begin(), result.end(), metadata.parent_dataset_id,
                             [](const auto& candidate, const auto& id) { return candidate.dataset_id < id; });
        if (parent == result.end() || parent->dataset_id != metadata.parent_dataset_id ||
            parent->generation == std::numeric_limits<std::uint32_t>::max() ||
            metadata.generation != parent->generation + 1) {
            throw std::invalid_argument("orphaned dataset record");
        }
    }
    return result;
}

// {
//   責務: [list_by_role: 他用途を混ぜず指定用途だけ返す],
//   引数: [role: 用途],
//   戻り値: [datasets: 指定用途のDataset]
// }
std::vector<DatasetMetadata> DatasetRegistry::list_by_role(DatasetRole role) const {
    static_cast<void>(role_name(role));
    auto result = list();
    std::erase_if(result, [role](const auto& metadata) { return metadata.role != role; });
    return result;
}

} // namespace kadoka::dataset
