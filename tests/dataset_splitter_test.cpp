#include "kadoka/training/dataset_splitter.hpp"

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace kadoka::shogi::training;

int main() {
    std::vector<ParsedRecord> records;
    for (int game = 0; game < 20; ++game) {
        for (int ply = 0; ply < 3; ++ply) {
            ParsedRecord record;
            record.game_id = "game-" + std::to_string(game);
            record.ply = static_cast<std::uint64_t>(ply);
            records.push_back(std::move(record));
        }
    }

    const DatasetSplitConfig config{1234, {0.7, 0.2, 0.1}};
    const DatasetSplitResult first = split_training_records(records, config);
    const DatasetSplitResult second = split_training_records(records, config);
    assert(first.manifest.train_game_ids == second.manifest.train_game_ids);
    assert(first.manifest.validation_game_ids == second.manifest.validation_game_ids);
    assert(first.manifest.test_game_ids == second.manifest.test_game_ids);
    assert(first.train.size() == 42);
    assert(first.validation.size() == 12);
    assert(first.test.size() == 6);
    assert(serialize_dataset_split_manifest(first.manifest)
        == serialize_dataset_split_manifest(second.manifest));

    for (int game = 0; game < 20; ++game) {
        const std::string id = "game-" + std::to_string(game);
        const auto count = [&id](const auto& split) {
            return std::count_if(split.begin(), split.end(), [&id](const ParsedRecord& item) {
                return item.game_id == id;
            });
        };
        const int train_count = static_cast<int>(count(first.train));
        const int validation_count = static_cast<int>(count(first.validation));
        const int test_count = static_cast<int>(count(first.test));
        assert((train_count == 3) + (validation_count == 3) + (test_count == 3) == 1);
        assert(train_count + validation_count + test_count == 3);
    }

    const std::string manifest = serialize_dataset_split_manifest(first.manifest);
    assert(manifest.find("kadoka.training_dataset_split") != std::string::npos);
    assert(manifest.find("\"seed\":1234") != std::string::npos);

    bool missing_id_rejected = false;
    try {
        (void)split_training_records({ParsedRecord{}}, config);
    } catch (const std::invalid_argument&) {
        missing_id_rejected = true;
    }
    assert(missing_id_rejected);

    bool invalid_ratios_rejected = false;
    try {
        (void)split_training_records(records, DatasetSplitConfig{1, {0.5, 0.5, 0.5}});
    } catch (const std::invalid_argument&) {
        invalid_ratios_rejected = true;
    }
    assert(invalid_ratios_rejected);
}
