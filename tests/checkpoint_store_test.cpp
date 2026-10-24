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
    std::filesystem::path directory;
    try {
        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            const auto candidate = std::filesystem::temp_directory_path() /
                                   ("kadoka-checkpoint-test-" + std::to_string(unique) + "-" +
                                    std::to_string(attempt));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                directory = candidate;
                break;
            }
            if (error)
                throw std::filesystem::filesystem_error("could not create checkpoint test directory", candidate,
                                                        error);
        }
        if (directory.empty())
            throw std::runtime_error("could not reserve a unique checkpoint test directory");
        const std::filesystem::path checkpoint_path = directory / "epoch-12-step-340.ckpt";
        const std::filesystem::path corrupt_path = directory / "corrupt.ckpt";

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

        CheckpointState different = source;
        different.weights = {99, 98, 97};
        different.metrics = {{"loss", 0.999}};
        bool rejected = false;
        try {
            save_checkpoint(checkpoint_path, different);
        } catch (const std::filesystem::filesystem_error&) {
            rejected = true;
        }
        const CheckpointState after_rejection = load_checkpoint(checkpoint_path);
        if (!rejected || after_rejection.epoch != source.epoch || after_rejection.step != source.step ||
            after_rejection.weights != source.weights || after_rejection.optimizer_state != source.optimizer_state ||
            after_rejection.scheduler_state != source.scheduler_state || after_rejection.rng_state != source.rng_state ||
            after_rejection.dataset_cursor != source.dataset_cursor ||
            after_rejection.training_config_hash != source.training_config_hash ||
            after_rejection.dataset_version != source.dataset_version || after_rejection.metrics != source.metrics)
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
            if (corrupt.gcount() != 1 || !corrupt)
                throw std::runtime_error("could not read the corruption test byte");
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
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("checksum mismatch") != std::string::npos;
        }
        if (!rejected)
            throw std::runtime_error("corrupted checkpoint did not fail checksum validation");

        const std::filesystem::path future_path = directory / "future.ckpt";
        std::filesystem::copy_file(checkpoint_path, future_path);
        {
            std::fstream future(future_path, std::ios::binary | std::ios::in | std::ios::out);
            future.seekp(8);
            const char next_version = 2;
            future.write(&next_version, 1);
            future.close();
            if (!future)
                throw std::runtime_error("could not prepare the future version fixture");
        }
        rejected = false;
        try {
            (void)load_checkpoint(future_path);
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("format version is unsupported") != std::string::npos;
        }
        if (!rejected)
            throw std::runtime_error("future checkpoint version was not identified");

        const std::filesystem::path truncated_path = directory / "truncated.ckpt";
        std::filesystem::copy_file(checkpoint_path, truncated_path);
        std::filesystem::resize_file(truncated_path, 15);
        rejected = false;
        try {
            (void)load_checkpoint(truncated_path);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("truncated checkpoint was accepted");

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
        std::error_code ignored;
        if (!directory.empty())
            std::filesystem::remove_all(directory, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
