#include "kadoka/training/checkpoint_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace kadoka::training {
namespace {

// {
//   責務: [pointer_files: kindに一致する確定済みpointer recordを世代順で列挙する],
//   引数: [directory: record格納先] [kind: latestまたはbest prefix],
//   戻り値: [vector<path>: file名順に並んだrecord]
// }
std::vector<std::filesystem::path> pointer_files(const std::filesystem::path& directory, const std::string& kind) {
    std::vector<std::filesystem::path> files;
    const std::string prefix = kind + ".pointer.";
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().filename().string().starts_with(prefix))
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

// {
//   責務: [read_pointer: pointer recordのcheckpoint filenameとmetadataを読み取る],
//   引数: [path: 読み込むrecord],
//   戻り値: [pair<string,string>: filenameとmetadata]
//   エラー: [破損またはfile読込失敗時はruntime_error]
// }
std::pair<std::string, std::string> read_pointer(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::string filename;
    std::string metadata;
    if (!std::getline(input, filename) || filename.empty() || filename.find_first_of("/\\") != std::string::npos)
        throw std::runtime_error("invalid checkpoint pointer: " + path.string());
    metadata.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (!metadata.empty() && metadata.back() == '\r')
        metadata.pop_back();
    return {filename, metadata};
}

// {
//   責務: [latest_checkpoint: 世代が最新のpointerが有効なcheckpointを指す時にpathを返す],
//   引数: [directory: checkpoint管理directory],
//   戻り値: [optional<path>: checkpoint pathまたは空]
// }
std::optional<std::filesystem::path> latest_checkpoint(const std::filesystem::path& directory) {
    const auto files = pointer_files(directory, "latest");
    if (files.empty())
        return std::nullopt;
    const auto [filename, metadata] = read_pointer(files.back());
    (void)metadata;
    const auto checkpoint = directory / filename;
    if (!std::filesystem::is_regular_file(checkpoint))
        return std::nullopt;
    return checkpoint;
}

// {
//   責務: [file_timestamp: 一意なsnapshot名に使う現在時刻のtickを返す],
//   引数: [],
//   戻り値: [uint64_t: monotonic clock tick]
// }
std::uint64_t file_timestamp() {
    return static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
}

} // namespace

// {
//   責務: [CheckpointManager: directoryを作成しpositive retentionを設定する],
//   引数: [directory: checkpoint保存先] [periodic_keep: 残すperiodic checkpoint数],
//   戻り値: [],
//   エラー: [retentionが0またはdirectory作成に失敗した場合は例外]
// }
CheckpointManager::CheckpointManager(std::filesystem::path directory, const std::size_t periodic_keep)
    : directory_(std::move(directory)), periodic_keep_(periodic_keep), generation_(file_timestamp()) {
    if (directory_.empty())
        throw std::invalid_argument("checkpoint manager directory must not be empty");
    if (periodic_keep_ == 0)
        throw std::invalid_argument("periodic retention must be at least one");
    std::filesystem::create_directories(directory_);
}

// {
//   責務: [save_snapshot: prefixとgenerationを使ってimmutable checkpointを作成する],
//   引数: [prefix: snapshot種類] [state: 学習状態],
//   戻り値: [path: 作成したcheckpoint]
// }
std::filesystem::path CheckpointManager::save_snapshot(const std::string& prefix, const CheckpointState& state) {
    const std::string filename = prefix + "-e" + std::to_string(state.epoch) + "-s" + std::to_string(state.step) + "-" +
                                 std::to_string(++generation_) + ".ckpt";
    const auto checkpoint = directory_ / filename;
    save_checkpoint(checkpoint, state);
    return checkpoint;
}

// {
//   責務: [publish_pointer: completeなtemporary recordをunique final nameへrenameして参照を公開する],
//   引数: [kind: pointer種別] [filename: checkpoint filename] [metadata: pointerに付随するtext],
//   戻り値: [],
//   エラー: [書込みまたはrenameに失敗した場合はfilesystem_error]
// }
void CheckpointManager::publish_pointer(const std::string& kind, const std::string& filename,
                                        const std::string& metadata) {
    const std::string generation = std::to_string(++generation_);
    const auto temporary = directory_ / (kind + ".pointer." + generation + ".tmp");
    const auto published = directory_ / (kind + ".pointer." + generation);
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!output)
            throw std::filesystem::filesystem_error("cannot open pointer temporary file", temporary,
                                                    std::make_error_code(std::errc::io_error));
        output << filename << '\n' << metadata << '\n';
        output.flush();
        if (!output)
            throw std::filesystem::filesystem_error("cannot write pointer temporary file", temporary,
                                                    std::make_error_code(std::errc::io_error));
        output.close();
        if (!output)
            throw std::filesystem::filesystem_error("cannot close pointer temporary file", temporary,
                                                    std::make_error_code(std::errc::io_error));
        std::filesystem::rename(temporary, published);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

// {
//   責務: [save_latest: immutable checkpointを保存し世代付きlatest pointerを発行する],
//   引数: [state: 保存する学習状態],
//   戻り値: [path: 保存したcheckpoint]
// }
std::filesystem::path CheckpointManager::save_latest(const CheckpointState& state) {
    const auto checkpoint = save_snapshot("checkpoint", state);
    publish_pointer("latest", checkpoint.filename().string(), "");
    return checkpoint;
}

// {
//   責務: [periodic_paths: periodic checkpointを最終更新時刻の降順で返す],
//   引数: [],
//   戻り値: [vector<path>: 新しい順のcheckpoint]
// }
std::vector<std::filesystem::path> CheckpointManager::periodic_paths() const {
    std::vector<std::pair<std::filesystem::file_time_type, std::filesystem::path>> entries;
    for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
        const std::string name = entry.path().filename().string();
        if (entry.is_regular_file() && name.starts_with("periodic-") && entry.path().extension() == ".ckpt")
            entries.emplace_back(entry.last_write_time(), entry.path());
    }
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        if (left.first != right.first)
            return left.first > right.first;
        return left.second > right.second;
    });
    std::vector<std::filesystem::path> paths;
    paths.reserve(entries.size());
    for (const auto& [time, path] : entries) {
        (void)time;
        paths.push_back(path);
    }
    return paths;
}

// {
//   責務: [save_periodic: periodic checkpointを保存しlatestを更新して古いperiodicをretention削除する],
//   引数: [state: 保存する学習状態],
//   戻り値: [path: 保存したperiodic checkpoint]
// }
std::filesystem::path CheckpointManager::save_periodic(const CheckpointState& state) {
    const auto checkpoint = save_snapshot("periodic", state);
    publish_pointer("latest", checkpoint.filename().string(), "");
    const auto paths = periodic_paths();
    for (std::size_t index = periodic_keep_; index < paths.size(); ++index)
        std::filesystem::remove(paths[index]);
    return checkpoint;
}

// {
//   責務: [latest_path: latest pointerの対象が存在する場合にそのpathを返す],
//   引数: [],
//   戻り値: [optional<path>: checkpoint pathまたは空]
// }
std::optional<std::filesystem::path> CheckpointManager::latest_path() const {
    return latest_checkpoint(directory_);
}

// {
//   責務: [best_path: best pointerの対象が存在する場合にそのpathを返す],
//   引数: [],
//   戻り値: [optional<path>: checkpoint pathまたは空]
// }
std::optional<std::filesystem::path> CheckpointManager::best_path() const {
    const auto files = pointer_files(directory_, "best");
    if (files.empty())
        return std::nullopt;
    const auto [filename, metadata] = read_pointer(files.back());
    (void)metadata;
    const auto checkpoint = directory_ / filename;
    if (!std::filesystem::is_regular_file(checkpoint))
        return std::nullopt;
    return checkpoint;
}

// {
//   責務: [consider_best: metricの向きに従い前回bestより改善した場合だけsnapshotを保存する],
//   引数: [state: 学習状態] [metric_name: 比較対象] [metric_value: finiteな値] [lower_is_better: 最小化ならtrue],
//   戻り値: [bool: 改善して保存した時true],
//   エラー: [metricが不正な場合はinvalid_argument]
// }
bool CheckpointManager::consider_best(const CheckpointState& state, const std::string& metric_name,
                                      const double metric_value, const bool lower_is_better) {
    if (metric_name.empty() || metric_name.find_first_of("\r\n") != std::string::npos || !std::isfinite(metric_value))
        throw std::invalid_argument("best checkpoint metric must have a valid name and finite value");
    const auto files = pointer_files(directory_, "best");
    if (!files.empty()) {
        const auto [filename, metadata] = read_pointer(files.back());
        (void)filename;
        std::istringstream input(metadata);
        std::string old_name;
        std::string old_value_text;
        std::string old_direction;
        double old_value = 0.0;
        if (!std::getline(input, old_name) || !std::getline(input, old_value_text) ||
            !std::getline(input, old_direction) || old_name != metric_name ||
            (old_direction != "lower" && old_direction != "higher"))
            throw std::runtime_error("best checkpoint metric configuration changed");
        try {
            std::size_t consumed = 0;
            old_value = std::stod(old_value_text, &consumed);
            if (consumed != old_value_text.size() || !std::isfinite(old_value))
                throw std::runtime_error("invalid stored best metric");
        } catch (const std::exception&) {
            throw std::runtime_error("invalid stored best metric");
        }
        if ((old_direction == "lower") != lower_is_better)
            throw std::runtime_error("best checkpoint metric direction changed");
        if (lower_is_better ? metric_value >= old_value : metric_value <= old_value)
            return false;
    }
    const auto checkpoint = save_snapshot("best", state);
    std::ostringstream value_stream;
    value_stream << std::setprecision(std::numeric_limits<double>::max_digits10) << metric_value;
    const std::string metadata = metric_name + "\n" + value_stream.str() + "\n" +
                                (lower_is_better ? "lower" : "higher");
    publish_pointer("best", checkpoint.filename().string(), metadata);
    return true;
}

} // namespace kadoka::training
