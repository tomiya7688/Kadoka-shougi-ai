#include "kadoka/training/checkpoint_manager.hpp"

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

// {
//   責務: [main: latest・periodic・best管理、retention、再起動後の復元を検証する],
//   処理: [1: checkpointを保存しlatest pointerを追跡する] [2: periodicを上限数に整理する]
//         [3: best metric改善とdirectory再読込を検証する],
//   引数: [],
//   戻り値: [int: 成功なら0、失敗なら1]
// }
int main() {
    using kadoka::training::CheckpointManager;
    using kadoka::training::CheckpointState;
    using kadoka::training::load_checkpoint;

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("kadoka-checkpoint-manager-" + std::to_string(unique));
    try {
        CheckpointManager manager(directory, 2);
        if (manager.latest_path() || manager.best_path())
            throw std::runtime_error("new manager unexpectedly has pointers");

        CheckpointState state;
        state.weights = {1, 2, 3};
        state.training_config_hash = "config-v1";
        state.dataset_version = "dataset-v1";
        state.epoch = 1;
        state.step = 1;
        const auto latest = manager.save_latest(state);
        if (manager.latest_path() != latest || load_checkpoint(latest).step != 1)
            throw std::runtime_error("latest checkpoint was not published correctly");

        state.step = 2;
        manager.save_periodic(state);
        state.step = 3;
        manager.save_periodic(state);
        state.step = 4;
        const auto newest_periodic = manager.save_periodic(state);
        const auto periodic = manager.periodic_paths();
        if (periodic.size() != 2 || periodic.front() != newest_periodic || manager.latest_path() != newest_periodic)
            throw std::runtime_error("periodic retention or latest pointer is incorrect");
        if (!std::filesystem::exists(periodic[1]))
            throw std::runtime_error("retained periodic checkpoint is missing");

        if (!manager.consider_best(state, "validation_loss", 0.5, true))
            throw std::runtime_error("first best checkpoint was not accepted");
        state.step = 5;
        if (manager.consider_best(state, "validation_loss", 0.6, true))
            throw std::runtime_error("worse best metric replaced the best checkpoint");
        if (!manager.consider_best(state, "validation_loss", 0.4, true))
            throw std::runtime_error("improved best metric did not replace the best checkpoint");
        const auto best = manager.best_path();
        if (!best || load_checkpoint(*best).step != 5)
            throw std::runtime_error("best pointer does not name the improved checkpoint");

        CheckpointManager reopened(directory, 2);
        if (reopened.latest_path() != newest_periodic || reopened.best_path() != best)
            throw std::runtime_error("published pointers were not recovered after reopening");

        bool rejected = false;
        try {
            (void)reopened.consider_best(state, "validation_loss", 0.3, false);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("changing best metric direction was accepted");

        std::filesystem::remove_all(directory);
        return 0;
    } catch (...) {
        std::filesystem::remove_all(directory);
        throw;
    }
}
