#include "kadoka/dataset/dataset_registry.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

// {
//   責務: [check: 期待に反した場合テストを失敗させる],
//   引数: [condition: 検証条件] [message: 失敗理由],
//   戻り値: []
// }
void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// {
//   責務: [rejects: 期待する入力拒否を確認する],
//   引数: [action: 拒否される操作],
//   戻り値: [rejected: invalid_argumentを投げた場合true]
// }
template <typename Action> bool rejects(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

// {
//   責務: [TempDirectory: テスト専用の保存先を作り終了時に片付ける],
//   フィールド: [path: 一時ディレクトリ]
// }
struct TempDirectory {
    std::filesystem::path path;

    // {
    //   責務: [TempDirectory: テスト固有の空ディレクトリを作る],
    //   戻り値: [directory: テスト専用領域]
    // }
    TempDirectory() {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("kadoka-dataset-registry-" + std::to_string(nonce));
        std::filesystem::create_directory(path);
    }

    // {
    //   責務: [~TempDirectory: このテストで作ったファイルだけ片付ける],
    //   戻り値: []
    // }
    ~TempDirectory() {
        try {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        } catch (...) {
            // {
            //   責務: [片付け失敗: デストラクタから例外を外へ出さず失敗を表示する],
            //   処理: [標準エラーへ通知する]
            // }
            std::fputs("dataset test cleanup failed\n", stderr);
        }
    }
};

// {
//   責務: [make_root: 出自情報が揃ったroot Datasetを作る],
//   引数: [id: Dataset ID] [role: 用途],
//   戻り値: [metadata: 登録可能なDataset]
// }
kadoka::dataset::DatasetMetadata make_root(std::string id, kadoka::dataset::DatasetRole role) {
    kadoka::dataset::DatasetMetadata metadata;
    metadata.dataset_id = std::move(id);
    metadata.source_type = "self_play";
    metadata.generator_model_id = "alpha";
    metadata.generator_model_version = "1.0";
    metadata.opponent_model_id = "beta";
    metadata.opponent_model_version = "2.0";
    metadata.search_config_hash = std::string(64, 'a');
    metadata.training_config_hash = std::string(64, 'b');
    metadata.created_at = "2026-10-21T00:00:00Z";
    metadata.license = "MIT";
    metadata.role = role;
    return metadata;
}

// {
//   責務: [run: 登録・再読込・親参照・用途分離・破損拒否を検証する],
//   戻り値: [exit_code: 成功時0]
// }
int run() {
    using kadoka::dataset::DatasetRegistry;
    using kadoka::dataset::DatasetRole;
    TempDirectory temp;
    DatasetRegistry registry(temp.path);
    auto root = make_root("root-1", DatasetRole::training);
    check(registry.add(root), "root must be added");
    check(!registry.add(root), "duplicate ID must be rejected");

    auto validation = make_root("validation-1", DatasetRole::validation);
    check(registry.add(validation), "validation dataset must be added");
    check(registry.list_by_role(DatasetRole::training).size() == 1, "validation must not leak into training");
    check(registry.list_by_role(DatasetRole::validation).size() == 1, "validation role must be discoverable");
    check(registry.list_by_role(DatasetRole::benchmark).empty(), "empty benchmark role must remain empty");

    auto derived = make_root("derived-1", DatasetRole::benchmark);
    derived.source_type = "derived";
    derived.parent_dataset_id = root.dataset_id;
    derived.generation = 1;
    check(registry.add(derived), "child must be added");
    check(rejects([&] { static_cast<void>(registry.remove(root.dataset_id)); }), "parent with child must be protected");

    DatasetRegistry reloaded(temp.path);
    const auto restored = reloaded.get(derived.dataset_id);
    check(restored && restored->parent_dataset_id == root.dataset_id && restored->generation == 1,
          "reloaded provenance must match");
    check(reloaded.list().size() == 3 && reloaded.list().front().dataset_id == "derived-1",
          "list must be stable and sorted");
    auto grandchild = make_root("grandchild-1", DatasetRole::benchmark);
    grandchild.source_type = "derived";
    grandchild.parent_dataset_id = derived.dataset_id;
    grandchild.generation = 2;
    check(registry.add(grandchild), "grandchild must be added");
    const auto root_path = temp.path / "root-1.dataset";
    const auto hidden_root = temp.path / "root-1.tmp";
    std::filesystem::rename(root_path, hidden_root);
    check(rejects([&] { static_cast<void>(reloaded.list()); }), "orphaned child must be rejected on reload");
    check(rejects([&] { static_cast<void>(reloaded.get(derived.dataset_id)); }),
          "orphaned child must be rejected by get");
    check(rejects([&] { static_cast<void>(reloaded.get(grandchild.dataset_id)); }),
          "missing grandparent must be rejected by get");
    std::filesystem::rename(hidden_root, root_path);

    auto bad_parent = derived;
    bad_parent.dataset_id = "missing-parent";
    bad_parent.parent_dataset_id = "absent";
    check(rejects([&] { static_cast<void>(registry.add(bad_parent)); }), "missing parent must be rejected");
    bad_parent.parent_dataset_id = root.dataset_id;
    bad_parent.generation = 2;
    check(rejects([&] { static_cast<void>(registry.add(bad_parent)); }), "wrong generation must be rejected");

    auto bad_id = root;
    bad_id.dataset_id = "../outside";
    check(rejects([&] { static_cast<void>(registry.add(bad_id)); }), "path traversal must be rejected");
    bad_id.dataset_id = "Root-1";
    check(rejects([&] { static_cast<void>(registry.add(bad_id)); }), "case-folding collision must be rejected");
    for (const auto* reserved_id : {"con", "prn", "aux", "nul"}) {
        bad_id.dataset_id = reserved_id;
        check(rejects([&] { static_cast<void>(registry.add(bad_id)); }),
              "Windows reserved device name must be rejected");
    }
    for (char digit = '1'; digit <= '9'; ++digit) {
        for (const auto* prefix : {"com", "lpt"}) {
            bad_id.dataset_id = std::string(prefix) + digit;
            check(rejects([&] { static_cast<void>(registry.add(bad_id)); }),
                  "Windows numbered device name must be rejected");
        }
    }
    for (const auto* allowed_id : {"com0", "lpt0"}) {
        auto allowed = make_root(allowed_id, DatasetRole::training);
        check(registry.add(allowed), "non-reserved device-like ID must be accepted");
        check(registry.remove(allowed.dataset_id), "device-like test record must be removable");
    }
    auto bad_hash = root;
    bad_hash.dataset_id = "bad-hash";
    bad_hash.search_config_hash = "abc";
    check(rejects([&] { static_cast<void>(registry.add(bad_hash)); }), "invalid hash must be rejected");
    auto bad_date = root;
    bad_date.dataset_id = "bad-date";
    bad_date.created_at = "2026-02-30T00:00:00Z";
    check(rejects([&] { static_cast<void>(registry.add(bad_date)); }), "invalid calendar date must be rejected");
    auto bad_utf8 = root;
    bad_utf8.dataset_id = "bad-utf8";
    bad_utf8.license = std::string("\xC0\xAF", 2);
    check(rejects([&] { static_cast<void>(registry.add(bad_utf8)); }), "invalid UTF-8 must be rejected");

    check(registry.remove(grandchild.dataset_id), "grandchild removal must succeed");
    check(registry.remove(derived.dataset_id), "child removal must succeed");
    check(registry.remove(root.dataset_id), "parent removal after child must succeed");
    check(!registry.remove(root.dataset_id), "missing ID removal must report false");
    auto unicode = make_root("unicode-license", DatasetRole::training);
    unicode.license = "利用可";
    check(registry.add(unicode), "valid UTF-8 must be accepted");
    check(registry.get(unicode.dataset_id)->license == unicode.license, "UTF-8 must survive reload");
    check(registry.remove(unicode.dataset_id), "UTF-8 record must be removable");
    auto stale = make_root("stale-tmp", DatasetRole::training);
    {
        std::ofstream abandoned(temp.path / "stale-tmp.tmp", std::ios::binary);
        abandoned << "interrupted write";
    }
    check(registry.add(stale), "stale temporary file must not prevent registration");
    check(registry.get(stale.dataset_id).has_value(), "recovered registration must be readable");
    check(!std::filesystem::exists(temp.path / "stale-tmp.tmp"), "stale temporary file must be removed");

    {
        std::ofstream corrupt(temp.path / "corrupt.dataset", std::ios::binary);
        corrupt << "invalid record";
    }
    check(rejects([&] { static_cast<void>(registry.get("corrupt")); }), "corrupt record must be rejected");
    return 0;
}

} // namespace

// {
//   責務: [main: Dataset Registryの回帰検査を実行する],
//   戻り値: [exit_code: 成功時0]
// }
int main() {
    try {
        return run();
    } catch (const std::exception& error) {
        std::fputs(error.what(), stderr);
        std::fputc('\n', stderr);
        return 1;
    } catch (...) {
        return 1;
    }
}
