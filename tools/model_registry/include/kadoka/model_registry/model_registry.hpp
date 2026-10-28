#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kadoka::model_registry {

// {
//   責務: [ModelManifest: 登録単位の識別子、互換性、出典、利用資源を保持する],
//   フィールド: [model_id: 安定ID] [model_version: モデル版] [model_type: モデル種別]
//     [artifact_format: 重み等の形式ID] [artifact_format_version: 形式版]
//     [api_version: 実行API版] [artifact_sha256: 配布物のSHA-256]
//     [source_kind: bundled/downloaded/user_trained] [source_reference: 出典識別子]
//     [license: 利用ライセンス] [runtime_id: 必須runtime]
//     [min_memory_mb: 必須メモリMiB] [min_threads: 必須スレッド数]
// }
struct ModelManifest {
    std::string model_id;
    std::string model_version;
    std::string model_type;
    std::string artifact_format;
    std::uint32_t artifact_format_version = 1;
    std::uint32_t api_version = 1;
    std::string artifact_sha256;
    std::string source_kind;
    std::string source_reference;
    std::string license;
    std::string runtime_id;
    std::uint64_t min_memory_mb = 0;
    std::uint32_t min_threads = 1;
};

// {
//   責務: [CompatibilityTarget: 使用環境の実行APIと対応形式・資源を表す],
//   フィールド: [api_version: 対応API版] [formats: 対応する形式IDと版]
//     [runtime_id: 利用可能runtime] [memory_mb: 利用可能メモリMiB]
//     [threads: 利用可能スレッド数]
// }
struct CompatibilityTarget {
    std::uint32_t api_version = 1;
    std::set<std::pair<std::string, std::uint32_t>> formats;
    std::string runtime_id;
    std::uint64_t memory_mb = 0;
    std::uint32_t threads = 0;
};

// {
//   責務: [CompatibilityResult: 互換判定と不一致理由を返す],
//   フィールド: [compatible: 利用可能か] [reason: 不一致理由、成功時は空]
// }
struct CompatibilityResult {
    bool compatible = false;
    std::string reason;
};

// {
//   責務: [validate_manifest: v1の必須項目と安全な識別子形式を確認する],
//   処理: [1: ID・version・形式・hash・資源を検証する],
//   引数: [manifest: 登録対象],
//   戻り値: [],
//   エラー: [違反時はstd::invalid_argument]
// }
void validate_manifest(const ModelManifest& manifest);

// {
//   責務: [manifest_to_json: v1のmanifestを決定的なJSONへ変換する],
//   処理: [1: 検証する] [2: 固定順のフィールドでJSONを作る],
//   引数: [manifest: 登録対象],
//   戻り値: [json: UTF-8 JSON]
// }
[[nodiscard]] std::string manifest_to_json(const ModelManifest& manifest);

// {
//   責務: [manifest_from_json: v1 JSONを厳格に読みmanifestを復元する],
//   処理: [1: 重複・未知フィールドを拒否する] [2: 型と値を検証する],
//   引数: [json: UTF-8 JSON],
//   戻り値: [manifest: 復元した内容]
// }
[[nodiscard]] ModelManifest manifest_from_json(std::string_view json);

// {
//   責務: [check_compatibility: 形式・API・runtime・資源が使えるか判定する],
//   処理: [1: manifestを検証する] [2: 各要件を照合する],
//   引数: [manifest: 登録済み仕様] [target: 実行環境],
//   戻り値: [result: 互換性と最初の不一致理由]
// }
[[nodiscard]] CompatibilityResult check_compatibility(const ModelManifest& manifest, const CompatibilityTarget& target);

// {
//   責務: [LocalModelRegistry: IDとversion別にmanifestをローカル永続化する],
//   フィールド: [directory_: 登録用ディレクトリ]
// }
class LocalModelRegistry {
  public:
    // {
    //   責務: [LocalModelRegistry: ローカル保存先を準備する],
    //   処理: [1: 指定ディレクトリを作成する],
    //   引数: [directory: registry専用保存先],
    //   戻り値: [registry: 利用可能な保存先]
    // }
    explicit LocalModelRegistry(std::filesystem::path directory);

    // {
    //   責務: [add: IDとversionの重複を拒否してmanifestを保存する],
    //   処理: [1: manifestを検証する] [2: 一時ファイルから確定名へ保存する],
    //   引数: [manifest: 登録対象],
    //   戻り値: []
    // }
    void add(const ModelManifest& manifest);

    // {
    //   責務: [remove: 指定した版のmanifestを削除する],
    //   処理: [1: 識別子を確認する] [2: 対象ファイルを削除する],
    //   引数: [model_id: モデルID] [model_version: 版],
    //   戻り値: [removed: 存在して削除できたか]
    // }
    [[nodiscard]] bool remove(std::string_view model_id, std::string_view model_version);

    // {
    //   責務: [get: 指定IDと版のmanifestを復元する],
    //   処理: [1: ファイルを探す] [2: 内容のIDと版を照合する],
    //   引数: [model_id: モデルID] [model_version: 版],
    //   戻り値: [manifest: 見つかった場合の内容]
    // }
    [[nodiscard]] std::optional<ModelManifest> get(std::string_view model_id, std::string_view model_version) const;

    // {
    //   責務: [list: 登録済みmanifestを決定的な順序で列挙する],
    //   処理: [1: 保存ファイルを読んでID・版順に並べる],
    //   引数: [],
    //   戻り値: [manifests: 全登録項目]
    // }
    [[nodiscard]] std::vector<ModelManifest> list() const;

  private:
    std::filesystem::path directory_;
};

} // namespace kadoka::model_registry
