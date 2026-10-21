#include "kadoka/model_registry/model_registry.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

// {
//   責務: [check: 条件違反をテスト失敗として報告する],
//   処理: [1: falseなら理由付き例外を投げる],
//   引数: [condition: 検査結果] [message: 失敗理由],
//   戻り値: []
// }
void check(const bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [TestDirectory: 作成した一時registryだけを片付ける],
//   フィールド: [path: テスト専用ディレクトリ]
// }
struct TestDirectory {
    std::filesystem::path path;

    // {
    //   責務: [TestDirectory: 一時registryを作る],
    //   処理: [1: 時刻から専用名を作成する],
    //   引数: [],
    //   戻り値: [directory: 専用保存先]
    // }
    TestDirectory() {
        path = std::filesystem::temp_directory_path() /
               ("kadoka-model-registry-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(path))
            throw std::runtime_error("temporary registry directory already exists");
    }

    // {
    //   責務: [~TestDirectory: 専用保存先を片付ける],
    //   処理: [1: このテストが作成したディレクトリを削除する],
    //   引数: [],
    //   戻り値: []
    // }
    ~TestDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

// {
//   責務: [make_manifest: 有効な検査用model manifestを作る],
//   処理: [1: 必須情報を決定論的に設定する],
//   引数: [],
//   戻り値: [manifest: 完全なv1 manifest]
// }
kadoka::model_registry::ModelManifest make_manifest() {
    return {.model_id = "kadoka.shogi.sample",
            .model_version = "1.2.0",
            .model_type = "evaluation_search",
            .artifact_format = "kadoka.weights",
            .artifact_format_version = 2,
            .api_version = 1,
            .artifact_sha256 = std::string(64, 'a'),
            .source_kind = "bundled",
            .source_reference = "Kadoka repository \"release\" \\ v1",
            .license = "MIT",
            .runtime_id = "kadoka_shogi_runtime",
            .min_memory_mb = 512,
            .min_threads = 2};
}

// {
//   責務: [expect_invalid: 不正manifestまたはJSONの拒否を確認する],
//   処理: [1: 処理を実行しinvalid_argumentを確認する],
//   引数: [action: 検査対象処理],
//   戻り値: []
// }
template <typename Action> void expect_invalid(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("invalid manifest input should be rejected");
}

// {
//   責務: [test_round_trip_and_compatibility: JSON復元と実行互換性を確認する],
//   処理: [1: 全フィールドを往復する] [2: 非対応形式・API・資源を拒否する],
//   引数: [],
//   戻り値: []
// }
void test_round_trip_and_compatibility() {
    using namespace kadoka::model_registry;
    auto manifest = make_manifest();
    const auto json = manifest_to_json(manifest);
    const auto restored = manifest_from_json(json);
    check(restored.model_id == manifest.model_id && restored.model_version == manifest.model_version &&
              restored.source_reference == manifest.source_reference &&
              restored.artifact_sha256 == manifest.artifact_sha256 && restored.artifact_format_version == 2 &&
              restored.min_memory_mb == 512 && restored.min_threads == 2,
          "manifest JSON round trip must preserve all identity and runtime fields");
    auto escaped_json = json;
    const auto source_key = escaped_json.find("\"source_reference\":");
    const auto source_end = escaped_json.find(",\n", source_key);
    check(source_key != std::string::npos && source_end != std::string::npos,
          "serialized source reference must be present");
    const auto source_colon = escaped_json.find(':', source_key);
    escaped_json.replace(source_colon + 1, source_end - source_colon - 1, R"("\uD83D\uDE00")");
    check(manifest_from_json(escaped_json).source_reference == "\xF0\x9F\x98\x80",
          "surrogate pair must decode into UTF-8");
    CompatibilityTarget target{.api_version = 1,
                               .formats = {{"kadoka.weights", 2}},
                               .runtime_id = "kadoka_shogi_runtime",
                               .memory_mb = 1024,
                               .threads = 4};
    check(check_compatibility(restored, target).compatible, "supported model must be compatible");
    target.formats.clear();
    check(check_compatibility(restored, target).reason == "artifact_format", "unsupported format must fail");
    target.formats.insert({"kadoka.weights", 2});
    target.api_version = 2;
    check(check_compatibility(restored, target).reason == "api_version", "API mismatch must fail");
    target.api_version = 1;
    target.memory_mb = 100;
    check(check_compatibility(restored, target).reason == "min_memory_mb", "insufficient memory must fail");
    manifest.model_id = "../escape";
    expect_invalid([&manifest] { validate_manifest(manifest); });
    manifest = make_manifest();
    manifest.artifact_sha256 = "bad";
    expect_invalid([&manifest] { validate_manifest(manifest); });
    expect_invalid([] { (void)manifest_from_json(R"({"schema":"kadoka.model_manifest.v1","schema":"duplicate"})"); });
    expect_invalid([] { (void)manifest_from_json(R"({"schema":"kadoka.model_manifest.v1","unknown":0})"); });
    expect_invalid([] { (void)manifest_from_json(R"({"schema":"kadoka.model_manifest.v1","x":"\uD800"})"); });
}

// {
//   責務: [test_registry_reload: 重複版・再読込・削除と破損拒否を確認する],
//   処理: [1: 2版を登録する] [2: 新インスタンスで再読込する]
//     [3: 重複版と改ざんした内容を拒否する],
//   引数: [],
//   戻り値: []
// }
void test_registry_reload() {
    using namespace kadoka::model_registry;
    TestDirectory directory;
    LocalModelRegistry registry(directory.path);
    const auto first = make_manifest();
    auto second = first;
    second.model_version = "1.3.0";
    second.source_kind = "user_trained";
    second.source_reference = "training-run-42";
    registry.add(second);
    registry.add(first);
    expect_invalid([&registry, &first] { registry.add(first); });
    LocalModelRegistry reopened(directory.path);
    const auto entries = reopened.list();
    check(entries.size() == 2 && entries[0].model_version == "1.2.0" && entries[1].model_version == "1.3.0",
          "registry must reload and sort both versions");
    const auto loaded = reopened.get(first.model_id, first.model_version);
    check(loaded && loaded->source_reference == first.source_reference, "get must restore stored manifest");
    check(reopened.remove(first.model_id, first.model_version), "remove must delete selected version");
    check(!reopened.remove(first.model_id, first.model_version) && !reopened.get(first.model_id, first.model_version),
          "removed version must stay absent");
    check(reopened.list().size() == 1, "other versions must survive remove");
    {
        std::ofstream damaged(directory.path / "kadoka.shogi.sample@1.3.0.json", std::ios::trunc);
        damaged << "{bad";
    }
    bool corruption_rejected = false;
    try {
        (void)reopened.list();
    } catch (const std::invalid_argument&) {
        corruption_rejected = true;
    }
    check(corruption_rejected, "corrupt registry record must not be silently skipped");
}

} // namespace

// {
//   責務: [main: manifest、互換性、registry永続化の契約を検査する],
//   処理: [1: JSONと互換性を確認する] [2: 保存後の再読込と削除を確認する]
//     [3: 配布用JSON例を解析する],
//   引数: [argc: 起動引数数] [argv: JSON例のパス],
//   戻り値: [0: 成功] [1: 検証失敗]
// }
int main(const int argc, char** argv) {
    try {
        check(argc == 2, "model manifest example path is required");
        test_round_trip_and_compatibility();
        test_registry_reload();
        std::ifstream input(argv[1], std::ios::binary);
        check(static_cast<bool>(input), "cannot open model manifest example");
        const std::string json(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
        const auto example = kadoka::model_registry::manifest_from_json(json);
        check(example.model_id == "kadoka.shogi.example", "model manifest example must parse");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
