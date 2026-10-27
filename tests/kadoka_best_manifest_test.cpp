#include "kadoka/best/manifest.hpp"

#include <locale>
#include <stdexcept>
#include <string>

namespace {

// {
//   責務: [GroupedHex: 桁区切りのあるロケールでhash出力の独立性を検証する]
//   フィールド: [なし: テスト用の桁区切り規則を提供する]
// }
class GroupedHex : public std::numpunct<char> {
  protected:
    // {
    //   責務: [do_grouping: テスト用に3桁ごとの区切りを指定する],
    //   戻り値: [区切り幅]
    //   処理: [1: 3桁ごとの区切り指定を返す]
    //   引数: [なし]
    // }
    std::string do_grouping() const override {
        return "\3";
    }

    // {
    //   責務: [do_thousands_sep: テスト用の桁区切り文字を返す],
    //   戻り値: [アンダースコア]
    //   処理: [1: テスト用の区切り文字を返す]
    //   引数: [なし]
    // }
    char do_thousands_sep() const override {
        return '_';
    }
};

// {
//   責務: [expect: 失敗した契約検証を位置と共にテスト失敗へ変換する]
//   引数: [condition: 成立を期待する条件] [message: 失敗理由]
//   処理: [1: 条件が偽なら理由付き例外を送出する]
//   戻り値: [なし]
// }
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [raw_manifest: model一つを参照する最小Raw manifestを作る]
//   戻り値: [hashを設定したRaw manifest]
//   処理: [1: model参照と設定を用意してhashを更新する]
//   引数: [なし]
// }
kadoka::best::KadokaBestManifest raw_manifest() {
    kadoka::best::KadokaBestManifest manifest;
    manifest.layer = kadoka::best::LayerType::raw;
    manifest.package_version = "1.0.0";
    manifest.model = kadoka::best::ArtifactReference{"kadoka.raw.example", "2.1.0"};
    manifest.config = {{"seed", "17"}, {"deterministic", "true"}};
    kadoka::best::refresh_config_hash(manifest);
    return manifest;
}

// {
//   責務: [integrated_manifest: model、evaluation、searchを持つIntegrated manifestを作る]
//   戻り値: [hashを設定したIntegrated manifest]
//   処理: [1: Raw構成にevaluationとsearchを追加する]
//   引数: [なし]
// }
kadoka::best::KadokaBestManifest integrated_manifest() {
    kadoka::best::KadokaBestManifest manifest = raw_manifest();
    manifest.layer = kadoka::best::LayerType::integrated;
    manifest.components = {{kadoka::best::ComponentRole::evaluation, "kadoka.eval", "1.2.0"},
                           {kadoka::best::ComponentRole::search, "kadoka.search", "3.0.1"}};
    return manifest;
}

// {
//   責務: [full_manifest: evaluator、search、orchestratorを持つFull Engineを作る]
//   戻り値: [hashを設定したFull manifest]
//   処理: [1: 必須componentと層参照を設定してhashを更新する]
//   引数: [なし]
// }
kadoka::best::KadokaBestManifest full_manifest() {
    kadoka::best::KadokaBestManifest manifest;
    manifest.layer = kadoka::best::LayerType::full;
    manifest.package_version = "1.0.0";
    manifest.components = {{kadoka::best::ComponentRole::orchestrator, "kadoka.orchestrator", "1.0.0"},
                           {kadoka::best::ComponentRole::search, "kadoka.search", "3.0.1"},
                           {kadoka::best::ComponentRole::evaluation, "kadoka.eval", "1.2.0"}};
    manifest.source_layer = kadoka::best::LayerType::raw;
    manifest.target_layer = kadoka::best::LayerType::integrated;
    kadoka::best::refresh_config_hash(manifest);
    return manifest;
}

} // namespace

// {
//   責務: [main: 3層、UTF-8、参照、locale、config hashとJSON round-tripを確認する]
//   戻り値: [全検証成功時0]
//   処理: [1: 各構成・異常値・ハッシュの契約を検証する]
//   引数: [なし]
// }
int main() {
    using kadoka::best::ComponentRole;
    using kadoka::best::LayerType;

    const auto raw = raw_manifest();
    expect(kadoka::best::validate_manifest(raw).empty(), "raw manifest should validate");

    const auto integrated = integrated_manifest();
    expect(kadoka::best::validate_manifest(integrated).empty(), "integrated manifest should validate");

    const auto full = full_manifest();
    expect(kadoka::best::validate_manifest(full).empty(), "full manifest should validate");

    const std::string raw_json = kadoka::best::serialize_manifest(raw);
    const auto raw_copy = kadoka::best::deserialize_manifest(raw_json);
    expect(kadoka::best::serialize_manifest(raw_copy) == raw_json, "raw JSON round-trip should be stable");

    const std::string full_json = kadoka::best::serialize_manifest(full);
    const auto full_copy = kadoka::best::deserialize_manifest(full_json);
    expect(full_copy.source_layer == LayerType::raw, "source layer should round-trip");
    expect(full_copy.target_layer == LayerType::integrated, "target layer should round-trip");

    auto escaped = raw;
    escaped.model->id = "kadoka.quoted\"raw\nmodel";
    const auto escaped_copy = kadoka::best::deserialize_manifest(kadoka::best::serialize_manifest(escaped));
    expect(escaped_copy.model->id == escaped.model->id, "JSON string escapes should round-trip");

    expect(kadoka::best::compute_config_hash({}) ==
               "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
           "empty config should match the SHA-256 golden value");
    expect(kadoka::best::compute_config_hash({{"\x01", "\"\\\n"}, {"\xe3\x81\x82", "\xe3\x81\x82/"}}) ==
               "sha256:a01661166e0e60a9020bdda18e3907fca1dcbadbbae2ec71e4436ddc19e93296",
           "escaped and non-ASCII canonical config must match the SHA-256 golden value");
    expect(raw.config_hash == kadoka::best::compute_config_hash(raw.config), "config hash should be reproducible");
    expect(kadoka::best::compute_config_hash({{"b", "2"}, {"a", "1"}}) ==
               kadoka::best::compute_config_hash({{"a", "1"}, {"b", "2"}}),
           "config hash should ignore insertion order");

    const std::locale original_locale = std::locale();
    std::locale::global(std::locale(original_locale, new GroupedHex));
    const auto grouped_hash = kadoka::best::compute_config_hash({});
    std::locale::global(original_locale);
    expect(grouped_hash == "sha256:44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a",
           "config hash must not depend on the global locale");

    const std::string invalid_utf8[] = {"\xc0\xaf",         "\xe0\x80\x80", "\xed\xa0\x80", "\xf0\x80\x80\x80",
                                        "\xf4\x90\x80\x80", "\xe3\x81",     "\x80"};
    for (const auto& invalid : invalid_utf8) {
        auto invalid_manifest = raw;
        invalid_manifest.model->id = invalid;
        expect(!kadoka::best::validate_manifest(invalid_manifest).empty(),
               "invalid UTF-8 in a model ID must be rejected");
        bool serialize_rejected = false;
        try {
            (void)kadoka::best::serialize_manifest(invalid_manifest);
        } catch (const std::invalid_argument&) {
            serialize_rejected = true;
        }
        expect(serialize_rejected, "serialization must reject invalid UTF-8");
        auto invalid_json = raw_json;
        const auto id_position = invalid_json.find("kadoka.raw.example");
        expect(id_position != std::string::npos, "fixture model ID must be present");
        invalid_json.replace(id_position, std::string("kadoka.raw.example").size(), invalid);
        bool parse_rejected = false;
        try {
            (void)kadoka::best::deserialize_manifest(invalid_json);
        } catch (const std::runtime_error&) {
            parse_rejected = true;
        }
        expect(parse_rejected, "JSON parser must reject invalid UTF-8");
    }
    auto invalid_config = raw;
    invalid_config.config["seed"] = "\x80";
    expect(!kadoka::best::validate_manifest(invalid_config).empty(), "invalid UTF-8 config must return errors");
    bool hash_rejected = false;
    try {
        (void)kadoka::best::compute_config_hash(invalid_config.config);
    } catch (const std::invalid_argument&) {
        hash_rejected = true;
    }
    expect(hash_rejected, "config hash must reject invalid UTF-8");
    const auto previous_hash = invalid_config.config_hash;
    bool refresh_rejected = false;
    try {
        kadoka::best::refresh_config_hash(invalid_config);
    } catch (const std::invalid_argument&) {
        refresh_rejected = true;
    }
    expect(refresh_rejected && invalid_config.config_hash == previous_hash,
           "failed hash refresh must preserve the existing hash");
    auto invalid_config_key = raw;
    invalid_config_key.config["\x80"] = "value";
    expect(!kadoka::best::validate_manifest(invalid_config_key).empty(), "invalid UTF-8 config key must return errors");
    bool key_hash_rejected = false;
    try {
        (void)kadoka::best::compute_config_hash(invalid_config_key.config);
    } catch (const std::invalid_argument&) {
        key_hash_rejected = true;
    }
    expect(key_hash_rejected, "config hash must reject invalid UTF-8 keys");

    for (const std::string escaped_invalid : {R"(\uD800)", R"(\uDC00)", R"(\uD800\u0041)"}) {
        auto malformed = raw_json;
        const auto id_position = malformed.find("kadoka.raw.example");
        expect(id_position != std::string::npos, "fixture model ID must be present");
        malformed.replace(id_position, std::string("kadoka.raw.example").size(), escaped_invalid);
        bool rejected = false;
        try {
            (void)kadoka::best::deserialize_manifest(malformed);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        expect(rejected, "invalid escaped surrogate must be rejected");
    }
    auto escaped_unicode_json = raw_json;
    const auto escaped_id_position = escaped_unicode_json.find("kadoka.raw.example");
    expect(escaped_id_position != std::string::npos, "fixture model ID must be present");
    escaped_unicode_json.replace(escaped_id_position, std::string("kadoka.raw.example").size(), R"(\uD83D\uDE00)");
    expect(kadoka::best::deserialize_manifest(escaped_unicode_json).model->id == "\xf0\x9f\x98\x80",
           "valid surrogate pair must decode to UTF-8");

    auto unicode = raw;
    unicode.config["name"] = "\xe6\x97\xa5\xe6\x9c\xac\xf0\x9f\x98\x80";
    kadoka::best::refresh_config_hash(unicode);
    const auto unicode_copy = kadoka::best::deserialize_manifest(kadoka::best::serialize_manifest(unicode));
    expect(unicode_copy.config == unicode.config, "valid UTF-8 config must round-trip");

    auto missing_component_version = integrated;
    missing_component_version.components.front().version.clear();
    expect(!kadoka::best::validate_manifest(missing_component_version).empty(),
           "missing component version should be rejected");

    auto missing_model = raw;
    missing_model.model.reset();
    expect(!kadoka::best::validate_manifest(missing_model).empty(),
           "raw manifest without model reference should be rejected");

    auto missing_orchestrator = full;
    missing_orchestrator.components.erase(missing_orchestrator.components.begin(),
                                          missing_orchestrator.components.begin() + 1);
    expect(!kadoka::best::validate_manifest(missing_orchestrator).empty(),
           "full manifest without orchestrator should be rejected");

    expect(kadoka::best::component_role_name(ComponentRole::orchestrator) == "orchestrator",
           "component role should serialize to its canonical name");
    expect(kadoka::best::layer_type_name(LayerType::integrated) == "integrated",
           "layer should serialize to its canonical name");

    bool duplicate_rejected = false;
    try {
        (void)kadoka::best::deserialize_manifest("{\"format\":\"kadoka.best_manifest.v1\",\"format\":\"duplicate\"}");
    } catch (const std::runtime_error&) {
        duplicate_rejected = true;
    }
    expect(duplicate_rejected, "duplicate JSON keys should be rejected");
}
