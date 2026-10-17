#include "kadoka/training/checkpoint_store.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// {
//   責務: [main: checkpoint保存・復元・互換性・破損検出の契約を検証する],
//   処理: [1: 全学習状態をround-tripする] [2: 既存targetと互換性不一致を拒否する]
//         [3: checksum破損と不正入力を検出する],
//   引数: [],
//   戻り値: [int: 成功なら0、検証失敗なら1]
// }
int main() {
    using kadoka::training::CheckpointCompatibility;
    using kadoka::training::CheckpointState;
    using kadoka::training::load_checkpoint;
    using kadoka::training::save_checkpoint;

    const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / ("kadoka-checkpoint-test-" + std::to_string(unique));
    std::filesystem::create_directory(directory);
    const std::filesystem::path checkpoint_path = directory / "epoch-12-step-340.ckpt";
    const std::filesystem::path corrupt_path = directory / "corrupt.ckpt";

    try {
        CheckpointState source;
        source.epoch = 12;
        source.step = 340;
        source.weights = {0, 1, 2, 255};
        source.optimizer_state = {3, 4, 5};
        source.scheduler_state = {6, 7};
        source.rng_state = {8, 9, 10};
        source.dataset_cursor = {11, 12};
        source.training_config_hash = "sha256:training-config-v1";
        source.dataset_version = "dataset-2026-10";
        source.metrics = {{"loss", 0.125}, {"validation_accuracy", 0.875}};

        save_checkpoint(checkpoint_path, source);
        const CheckpointState restored = load_checkpoint(
            checkpoint_path, CheckpointCompatibility{source.training_config_hash, source.dataset_version});
        if (restored.epoch != source.epoch || restored.step != source.step || restored.weights != source.weights ||
            restored.optimizer_state != source.optimizer_state || restored.scheduler_state != source.scheduler_state ||
            restored.rng_state != source.rng_state || restored.dataset_cursor != source.dataset_cursor ||
            restored.training_config_hash != source.training_config_hash ||
            restored.dataset_version != source.dataset_version || restored.metrics != source.metrics)
            throw std::runtime_error("checkpoint round-trip changed saved training state");

        bool rejected = false;
        try {
            save_checkpoint(checkpoint_path, source);
        } catch (const std::filesystem::filesystem_error&) {
            rejected = true;
        }
        if (!rejected || load_checkpoint(checkpoint_path).weights != source.weights)
            throw std::runtime_error("saving over an existing checkpoint did not fail safely");

        rejected = false;
        try {
            (void)load_checkpoint(checkpoint_path, CheckpointCompatibility{"sha256:other-config", {}});
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("incompatible training config was accepted");

        rejected = false;
        try {
            (void)load_checkpoint(checkpoint_path, CheckpointCompatibility{{}, "dataset-other"});
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("incompatible dataset version was accepted");

        std::filesystem::copy_file(checkpoint_path, corrupt_path);
        {
            std::fstream corrupt(corrupt_path, std::ios::binary | std::ios::in | std::ios::out);
            corrupt.seekg(12);
            char byte = 0;
            corrupt.read(&byte, 1);
            byte ^= 0x01;
            corrupt.seekp(12);
            corrupt.write(&byte, 1);
            corrupt.close();
            if (!corrupt)
                throw std::runtime_error("could not prepare the corruption test fixture");
        }
        rejected = false;
        try {
            (void)load_checkpoint(corrupt_path);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("corrupted checkpoint was accepted");

        CheckpointState invalid = source;
        invalid.weights.clear();
        rejected = false;
        try {
            save_checkpoint(directory / "invalid.ckpt", invalid);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected || std::filesystem::exists(directory / "invalid.ckpt"))
            throw std::runtime_error("checkpoint with missing weights was not rejected before writing");

        std::filesystem::remove_all(directory);
        std::cout << "checkpoint_store_test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove_all(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
