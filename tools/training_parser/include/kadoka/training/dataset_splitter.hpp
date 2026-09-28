#pragma once

#include "kadoka/training/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace kadoka::shogi::training {

enum class DatasetSplit : std::uint8_t { Train, Validation, Test };

struct DatasetSplitRatios {
    double train{0.8};
    double validation{0.1};
    double test{0.1};
};

struct DatasetSplitConfig {
    std::uint64_t seed{0};
    DatasetSplitRatios ratios{};
};

struct DatasetSplitManifest {
    std::uint64_t seed{0};
    DatasetSplitRatios ratios{};
    std::vector<std::string> train_game_ids{};
    std::vector<std::string> validation_game_ids{};
    std::vector<std::string> test_game_ids{};
};

struct DatasetSplitResult {
    std::vector<ParsedRecord> train{};
    std::vector<ParsedRecord> validation{};
    std::vector<ParsedRecord> test{};
    DatasetSplitManifest manifest{};
};

[[nodiscard]] DatasetSplitResult split_training_records(
    const std::vector<ParsedRecord>& records,
    const DatasetSplitConfig& config
);

[[nodiscard]] std::string serialize_dataset_split_manifest(
    const DatasetSplitManifest& manifest
);

} // namespace kadoka::shogi::training
