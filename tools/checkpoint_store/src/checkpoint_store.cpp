#include "kadoka/training/checkpoint_store.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace kadoka::training {

// {
//   責務: [checkpoint metric形式: doubleをIEEE 754 binary64として固定する],
//   引数: [double: 使用するplatformの浮動小数点型],
//   戻り値: [なし: 非対応platformをコンパイル時に拒否する]
// }
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8);

// {
//   責務: [save_checkpoint: 学習状態を検証しchecksum付きbinary checkpointとして原子的に保存する],
//   処理: [1: metadataとmetricを検証する] [2: versionとlength付きfieldを符号化する]
//         [3: sibling temporary directoryへ書き、targetが未存在ならhard linkで公開する],
//   引数: [path: 新規作成するcheckpoint file] [state: 保存する学習状態],
//   戻り値: [],
//   エラー: [pathが不正または既存なら例外を送出する] [書込み失敗時はtemporary fileを除去する]
// }
void save_checkpoint(const std::filesystem::path& path, const CheckpointState& state) {
    constexpr std::uint32_t kFormatVersion = 1;
    constexpr std::uint64_t kMaximumCheckpointBytes = 1ULL << 30;
    constexpr std::uint32_t kMaximumMetrics = 100000;
    static constexpr std::array<std::uint8_t, 8> kMagic = {'K', 'D', 'K', 'C', 'K', 'P', 'T', '1'};

    if (path.empty() || path.filename().empty())
        throw std::invalid_argument("checkpoint path must name a file");
    if (state.weights.empty())
        throw std::invalid_argument("checkpoint weights must not be empty");
    if (state.training_config_hash.empty() || state.dataset_version.empty())
        throw std::invalid_argument("checkpoint config hash and dataset version are required");
    if (state.metrics.size() > kMaximumMetrics)
        throw std::invalid_argument("checkpoint has too many metrics");
    for (const auto& [name, value] : state.metrics) {
        if (name.empty() || !std::isfinite(value))
            throw std::invalid_argument("checkpoint metric names must be nonempty and values finite");
    }

    // {
    //   責務: [account_bytes: 書込みバッファを確保する前に形式上限を検査する],
    //   引数: [count: 次のfieldが使用するbyte数],
    //   戻り値: [なし: totalを更新する],
    //   エラー: [1 GiBの上限を超える場合はinvalid_argument]
    // }
    std::uint64_t total = 8 + 4 + 8 + 8 + 4 + 8;
    const auto account_bytes = [&total](const std::uint64_t count) {
        if (count > kMaximumCheckpointBytes - total)
            throw std::invalid_argument("checkpoint exceeds the 1 GiB format limit");
        total += count;
    };
    account_bytes(4 + state.training_config_hash.size());
    account_bytes(4 + state.dataset_version.size());
    for (const auto* blob : {&state.weights, &state.optimizer_state, &state.scheduler_state, &state.rng_state,
                             &state.dataset_cursor})
        account_bytes(8 + blob->size());
    for (const auto& [name, value] : state.metrics) {
        (void)value;
        account_bytes(4 + name.size() + 8);
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(total));
    const auto append_u8 = [&bytes](const std::uint8_t value) { bytes.push_back(value); };
    const auto append_u32 = [&bytes, &append_u8](const std::uint32_t value) {
        for (unsigned shift = 0; shift < 32; shift += 8)
            append_u8(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    };
    const auto append_u64 = [&bytes, &append_u8](const std::uint64_t value) {
        for (unsigned shift = 0; shift < 64; shift += 8)
            append_u8(static_cast<std::uint8_t>((value >> shift) & 0xffULL));
    };
    const auto append_string = [&bytes, &append_u32](const std::string& value) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("checkpoint metadata field is too large");
        append_u32(static_cast<std::uint32_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    };
    const auto append_blob = [&bytes, &append_u64](const std::vector<std::uint8_t>& value) {
        append_u64(static_cast<std::uint64_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    };

    bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
    append_u32(kFormatVersion);
    append_u64(state.epoch);
    append_u64(state.step);
    append_string(state.training_config_hash);
    append_string(state.dataset_version);
    append_blob(state.weights);
    append_blob(state.optimizer_state);
    append_blob(state.scheduler_state);
    append_blob(state.rng_state);
    append_blob(state.dataset_cursor);
    append_u32(static_cast<std::uint32_t>(state.metrics.size()));
    for (const auto& [name, value] : state.metrics) {
        append_string(name);
        append_u64(std::bit_cast<std::uint64_t>(value));
    }

    // FNV-1a detects accidental corruption; it is not an authenticity signature.
    std::uint64_t checksum = 14695981039346656037ULL;
    for (const std::uint8_t byte : bytes) {
        checksum ^= byte;
        checksum *= 1099511628211ULL;
    }
    append_u64(checksum);
    if (bytes.size() > kMaximumCheckpointBytes)
        throw std::invalid_argument("checkpoint exceeds the 1 GiB format limit");

    const std::filesystem::path parent = path.has_parent_path() ? path.parent_path() : std::filesystem::current_path();
    if (!std::filesystem::is_directory(parent))
        throw std::filesystem::filesystem_error("checkpoint parent directory does not exist", parent,
                                                std::make_error_code(std::errc::no_such_file_or_directory));
    if (std::filesystem::exists(path))
        throw std::filesystem::filesystem_error("checkpoint target already exists", path,
                                                std::make_error_code(std::errc::file_exists));

    static std::atomic<std::uint64_t> sequence{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path temporary_directory;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        temporary_directory = parent / (".kadoka-checkpoint.tmp-" + std::to_string(timestamp) + "-" +
                                        std::to_string(sequence.fetch_add(1)));
        std::error_code error;
        if (std::filesystem::create_directory(temporary_directory, error))
            break;
        if (!error && attempt == 31)
            error = std::make_error_code(std::errc::file_exists);
        if (error)
            throw std::filesystem::filesystem_error("could not reserve checkpoint temporary directory",
                                                    temporary_directory, error);
    }

    try {
        const std::filesystem::path temporary_file = temporary_directory / "checkpoint.bin";
        std::ofstream output(temporary_file, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!output)
            throw std::filesystem::filesystem_error("could not open temporary checkpoint", temporary_file,
                                                    std::make_error_code(std::errc::io_error));
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output)
            throw std::filesystem::filesystem_error("could not write temporary checkpoint", temporary_file,
                                                    std::make_error_code(std::errc::io_error));
        output.close();
        if (!output)
            throw std::filesystem::filesystem_error("could not close temporary checkpoint", temporary_file,
                                                    std::make_error_code(std::errc::io_error));
        std::filesystem::create_hard_link(temporary_file, path);
        std::error_code ignored;
        std::filesystem::remove(temporary_file, ignored);
        std::filesystem::remove(temporary_directory, ignored);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(temporary_directory, ignored);
        throw;
    }
}

// {
//   責務: [load_checkpoint: binary checkpointを検証して学習再開用の状態を復元する],
//   処理: [1: file sizeとchecksumを検証する] [2: versionおよびfield境界を確認してdecodeする]
//         [3: 指定されたconfig hashとdataset版の互換性を確認する],
//   引数: [path: 読み込むcheckpoint file] [expected: 任意の互換性条件],
//   戻り値: [CheckpointState: 復元した学習状態],
//   エラー: [file破損・形式非対応・互換性不一致はruntime_error]
// }
CheckpointState load_checkpoint(const std::filesystem::path& path, const CheckpointCompatibility& expected) {
    constexpr std::uint32_t kFormatVersion = 1;
    constexpr std::uint64_t kMaximumCheckpointBytes = 1ULL << 30;
    constexpr std::uint32_t kMaximumMetrics = 100000;
    static constexpr std::array<std::uint8_t, 8> kMagic = {'K', 'D', 'K', 'C', 'K', 'P', 'T', '1'};

    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        throw std::filesystem::filesystem_error("could not open checkpoint", path,
                                                std::make_error_code(std::errc::no_such_file_or_directory));
    const std::streampos end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > kMaximumCheckpointBytes || end < 8 + 4 + 8)
        throw std::runtime_error("checkpoint size is invalid");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input)
        throw std::filesystem::filesystem_error("could not read checkpoint", path,
                                                std::make_error_code(std::errc::io_error));

    const std::size_t content_size = bytes.size() - 8;
    for (std::size_t index = 0; index < kMagic.size(); ++index) {
        if (bytes[index] != kMagic[index])
            throw std::runtime_error("checkpoint magic is invalid");
    }
    std::uint32_t version = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        version |= static_cast<std::uint32_t>(bytes[8 + shift / 8]) << shift;
    if (version != kFormatVersion)
        throw std::runtime_error("checkpoint format version is unsupported");
    const auto read_u64_at = [&bytes](const std::size_t offset) {
        std::uint64_t value = 0;
        for (unsigned shift = 0; shift < 64; shift += 8)
            value |= static_cast<std::uint64_t>(bytes[offset + shift / 8]) << shift;
        return value;
    };
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::size_t index = 0; index < content_size; ++index) {
        checksum ^= bytes[index];
        checksum *= 1099511628211ULL;
    }
    if (checksum != read_u64_at(content_size))
        throw std::runtime_error("checkpoint checksum mismatch");

    std::size_t offset = 0;
    const auto require = [&offset, content_size](const std::size_t count) {
        if (count > content_size - offset)
            throw std::runtime_error("checkpoint field extends past end of file");
    };
    const auto read_u8 = [&bytes, &offset, &require]() {
        require(1);
        return bytes[offset++];
    };
    const auto read_u32 = [&read_u8]() {
        std::uint32_t value = 0;
        for (unsigned shift = 0; shift < 32; shift += 8)
            value |= static_cast<std::uint32_t>(read_u8()) << shift;
        return value;
    };
    const auto read_u64 = [&read_u8]() {
        std::uint64_t value = 0;
        for (unsigned shift = 0; shift < 64; shift += 8)
            value |= static_cast<std::uint64_t>(read_u8()) << shift;
        return value;
    };
    const auto read_string = [&bytes, &offset, &require, &read_u32]() {
        const std::uint32_t size = read_u32();
        require(size);
        std::string value(reinterpret_cast<const char*>(bytes.data() + offset), size);
        offset += size;
        return value;
    };
    const auto read_blob = [&bytes, &offset, &require, &read_u64]() {
        const std::uint64_t size = read_u64();
        if (size > std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("checkpoint blob is too large for this platform");
        require(static_cast<std::size_t>(size));
        std::vector<std::uint8_t> value(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                        bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
        offset += static_cast<std::size_t>(size);
        return value;
    };

    for (const std::uint8_t expected_byte : kMagic) {
        if (read_u8() != expected_byte)
            throw std::runtime_error("checkpoint magic is invalid");
    }
    if (read_u32() != kFormatVersion)
        throw std::runtime_error("checkpoint format version is unsupported");

    CheckpointState state;
    state.epoch = read_u64();
    state.step = read_u64();
    state.training_config_hash = read_string();
    state.dataset_version = read_string();
    state.weights = read_blob();
    state.optimizer_state = read_blob();
    state.scheduler_state = read_blob();
    state.rng_state = read_blob();
    state.dataset_cursor = read_blob();
    const std::uint32_t metric_count = read_u32();
    if (metric_count > kMaximumMetrics)
        throw std::runtime_error("checkpoint has too many metrics");
    for (std::uint32_t index = 0; index < metric_count; ++index) {
        std::string name = read_string();
        const double value = std::bit_cast<double>(read_u64());
        if (name.empty() || !std::isfinite(value) || !state.metrics.emplace(std::move(name), value).second)
            throw std::runtime_error("checkpoint metric entry is invalid");
    }
    if (offset != content_size)
        throw std::runtime_error("checkpoint contains trailing data");
    if (state.weights.empty() || state.training_config_hash.empty() || state.dataset_version.empty())
        throw std::runtime_error("checkpoint is missing required state");
    if (!expected.training_config_hash.empty() && state.training_config_hash != expected.training_config_hash)
        throw std::runtime_error("checkpoint training config hash does not match");
    if (!expected.dataset_version.empty() && state.dataset_version != expected.dataset_version)
        throw std::runtime_error("checkpoint dataset version does not match");
    return state;
}

} // namespace kadoka::training
