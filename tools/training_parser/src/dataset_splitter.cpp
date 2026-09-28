#include "kadoka/training/dataset_splitter.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace kadoka::shogi::training {
namespace {

void validate_config(const DatasetSplitConfig& config) {
    const auto valid = [](double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    };
    if (!valid(config.ratios.train)
        || !valid(config.ratios.validation)
        || !valid(config.ratios.test)
        || std::abs(config.ratios.train + config.ratios.validation
            + config.ratios.test - 1.0) > 1e-9) {
        throw std::invalid_argument("dataset split ratios must be in [0,1] and sum to 1");
    }
}

std::string json_escape(const std::string& value) {
    std::string result;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 0x20) {
                constexpr char digits[] = "0123456789abcdef";
                result += "\\u00";
                result += digits[ch >> 4];
                result += digits[ch & 0x0f];
            } else result += static_cast<char>(ch);
        }
    }
    return result;
}

std::uint64_t assignment_key(std::string_view id, std::uint64_t seed) noexcept {
    std::uint64_t hash = 14695981039346656037ULL ^ seed;
    for (const unsigned char ch : id) {
        hash ^= ch;
        hash *= 1099511628211ULL;
    }
    hash += 0x9e3779b97f4a7c15ULL;
    hash = (hash ^ (hash >> 30)) * 0xbf58476d1ce4e5b9ULL;
    hash = (hash ^ (hash >> 27)) * 0x94d049bb133111ebULL;
    return hash ^ (hash >> 31);
}

void append_ids(std::ostringstream& out, const std::vector<std::string>& ids) {
    out << '[';
    for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i != 0) out << ',';
        out << '"' << json_escape(ids[i]) << '"';
    }
    out << ']';
}

} // namespace

DatasetSplitResult split_training_records(
    const std::vector<ParsedRecord>& records,
    const DatasetSplitConfig& config) {
    validate_config(config);
    std::set<std::string> unique_ids;
    for (const ParsedRecord& record : records) {
        if (!record.game_id || record.game_id->empty()) {
            throw std::invalid_argument("every record must have a game_id to prevent split leakage");
        }
        unique_ids.insert(*record.game_id);
    }

    std::vector<std::string> shuffled(unique_ids.begin(), unique_ids.end());
    std::sort(shuffled.begin(), shuffled.end(), [&config](const auto& left, const auto& right) {
        const std::uint64_t left_key = assignment_key(left, config.seed);
        const std::uint64_t right_key = assignment_key(right, config.seed);
        return left_key == right_key ? left < right : left_key < right_key;
    });

    const std::size_t game_count = shuffled.size();
    std::size_t train_count = static_cast<std::size_t>(std::floor(game_count * config.ratios.train));
    std::size_t validation_count = static_cast<std::size_t>(std::floor(game_count * config.ratios.validation));
    std::size_t test_count = static_cast<std::size_t>(std::floor(game_count * config.ratios.test));
    // Allocate fractional remainders by largest remainder, with stable split-order ties.
    struct Remainder { double value; std::size_t split; };
    std::vector<Remainder> remainders{
        {game_count * config.ratios.train - train_count, 0},
        {game_count * config.ratios.validation - validation_count, 1},
        {game_count * config.ratios.test - test_count, 2},
    };
    std::stable_sort(remainders.begin(), remainders.end(), [](auto a, auto b) {
        return a.value > b.value;
    });
    std::size_t allocated = train_count + validation_count + test_count;
    for (const auto& remainder : remainders) {
        if (allocated++ >= game_count) break;
        if (remainder.split == 0) ++train_count;
        else if (remainder.split == 1) ++validation_count;
        else ++test_count;
    }

    DatasetSplitResult result;
    result.manifest.seed = config.seed;
    result.manifest.ratios = config.ratios;
    result.manifest.train_game_ids.assign(shuffled.begin(), shuffled.begin() + train_count);
    result.manifest.validation_game_ids.assign(
        shuffled.begin() + train_count, shuffled.begin() + train_count + validation_count);
    result.manifest.test_game_ids.assign(
        shuffled.begin() + train_count + validation_count, shuffled.end());

    std::unordered_map<std::string, DatasetSplit> assignments;
    for (const auto& id : result.manifest.train_game_ids) assignments.emplace(id, DatasetSplit::Train);
    for (const auto& id : result.manifest.validation_game_ids) assignments.emplace(id, DatasetSplit::Validation);
    for (const auto& id : result.manifest.test_game_ids) assignments.emplace(id, DatasetSplit::Test);
    for (const ParsedRecord& record : records) {
        switch (assignments.at(*record.game_id)) {
        case DatasetSplit::Train: result.train.push_back(record); break;
        case DatasetSplit::Validation: result.validation.push_back(record); break;
        case DatasetSplit::Test: result.test.push_back(record); break;
        }
    }
    return result;
}

std::string serialize_dataset_split_manifest(const DatasetSplitManifest& manifest) {
    std::ostringstream out;
    out << std::setprecision(17)
        << "{\"schema\":\"kadoka.training_dataset_split\",\"version\":1,\"seed\":"
        << manifest.seed << ",\"ratios\":{\"train\":" << manifest.ratios.train
        << ",\"validation\":" << manifest.ratios.validation
        << ",\"test\":" << manifest.ratios.test << "},\"games\":{\"train\":";
    append_ids(out, manifest.train_game_ids);
    out << ",\"validation\":";
    append_ids(out, manifest.validation_game_ids);
    out << ",\"test\":";
    append_ids(out, manifest.test_game_ids);
    out << "}}";
    return out.str();
}

} // namespace kadoka::shogi::training
