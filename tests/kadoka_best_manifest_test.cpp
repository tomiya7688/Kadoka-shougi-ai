#include "kadoka/best/manifest.hpp"

#include <stdexcept>
#include <string>

namespace {

// {
//   責務: [expect: 失敗した契約検証を位置と共にテスト失敗へ変換する]
//   引数: [condition: 成立を期待する条件] [message: 失敗理由]
// }
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [raw_manifest: model一つを参照する最小Raw manifestを作る]
//   戻り値: [hashを設定したRaw manifest]
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
//   責務: [main: 3層、参照検証、config hash、JSON round-tripを確認する]
//   戻り値: [全検証成功時0]
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
    expect(raw.config_hash == kadoka::best::compute_config_hash(raw.config), "config hash should be reproducible");
    expect(kadoka::best::compute_config_hash({{"b", "2"}, {"a", "1"}}) ==
               kadoka::best::compute_config_hash({{"a", "1"}, {"b", "2"}}),
           "config hash should ignore insertion order");

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
