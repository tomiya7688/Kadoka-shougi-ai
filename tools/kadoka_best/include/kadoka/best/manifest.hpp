#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::best {

// {
//   責務: [LayerType: Kadoka BestのRaw / Integrated / Full層を識別する]
// }
enum class LayerType { raw, integrated, full };

// {
//   責務: [ComponentRole: manifestが参照するAI構成要素の役割を識別する]
// }
enum class ComponentRole { evaluation, search, opening, endgame, orchestrator };

// {
//   責務: [ArtifactReference: modelまたはcheckpointのIDとversionを保持する]
//   フィールド: [id: model等の識別子 / version: 参照する版]
// }
struct ArtifactReference {
    std::string id;
    std::string version;
};

// {
//   責務: [ComponentReference: AI構成要素の役割、ID、versionを保持する]
//   フィールド: [role: 役割 / component_id: 識別子 / version: 版]
// }
struct ComponentReference {
    ComponentRole role = ComponentRole::evaluation;
    std::string component_id;
    std::string version;
};

// {
//   責務: [KadokaBestManifest: 3層の構成、参照先、設定、再現用hashを保持する]
//   フィールド: [format・package_version: 形式と版 / layer: 構成層 / model・checkpoint: 参照 /
//   source_layer・target_layer: 移行元と移行先 / components: 構成要素 / config・config_hash: 設定とhash]
// }
struct KadokaBestManifest {
    std::string format = "kadoka.best_manifest.v1";
    LayerType layer = LayerType::raw;
    std::string package_version;
    std::optional<ArtifactReference> model;
    std::optional<ArtifactReference> checkpoint;
    std::optional<LayerType> source_layer;
    std::optional<LayerType> target_layer;
    std::vector<ComponentReference> components;
    std::map<std::string, std::string> config;
    std::string config_hash;
};

// {
//   責務: [layer_type_name: 層のenumをmanifest用の文字列へ変換する]
//   引数: [layer: 変換する層]
//   戻り値: [raw / integrated / full、未知のenum値ならinvalid]
//   処理: [1: 各enum値を固定の層名に対応付ける]
// }
[[nodiscard]] std::string_view layer_type_name(LayerType layer) noexcept;

// {
//   責務: [component_role_name: 構成要素の役割をmanifest用の文字列へ変換する]
//   引数: [role: 変換する役割]
//   戻り値: [役割名]
//   処理: [1: 各enum値を固定の役割名に対応付ける]
// }
[[nodiscard]] std::string_view component_role_name(ComponentRole role) noexcept;

// {
//   責務: [compute_config_hash: 設定mapを正規化しSHA-256 hashを再現可能に計算する]
//   引数: [config: hash対象の設定map]
//   戻り値: [sha256:接頭辞と小文字16進数64桁のhash。不正UTF-8ならinvalid_argument]
//   処理: [1: 設定をcanonical JSONにしSHA-256を計算する]
// }
[[nodiscard]] std::string compute_config_hash(const std::map<std::string, std::string>& config);

// {
//   責務: [refresh_config_hash: manifestの設定からconfig_hashを更新する]
//   引数: [manifest: hashを更新するmanifest。不正UTF-8の設定はinvalid_argumentで拒否し、hashは変更しない]
//   処理: [1: 設定から再計算したhashを代入する]
//   戻り値: [なし]
// }
void refresh_config_hash(KadokaBestManifest& manifest);

// {
//   責務: [validate_manifest: manifestのversion、参照、層要件、hashを検査する]
//   引数: [manifest: 検査するmanifest]
//   戻り値: [問題がなければ空、それ以外は人間向けエラー一覧]
//   処理: [1: 層・参照・UTF-8・設定hashの制約を検査する]
// }
[[nodiscard]] std::vector<std::string> validate_manifest(const KadokaBestManifest& manifest);

// {
//   責務: [serialize_manifest: 検証済みmanifestを安定順序のJSONへ変換する]
//   引数: [manifest: JSON化するmanifest]
//   戻り値: [manifest JSON。不正なmanifestならinvalid_argument]
//   処理: [1: 制約を検査し固定順のJSONを組み立てる]
// }
[[nodiscard]] std::string serialize_manifest(const KadokaBestManifest& manifest);

// {
//   責務: [deserialize_manifest: JSONからmanifestを復元し構造と参照を検証する]
//   引数: [json: 読み込むmanifest JSON]
//   戻り値: [復元したmanifest。構文・型・必須fieldの不正はruntime_error、manifest制約違反はinvalid_argument]
//   処理: [1: JSON構文とmanifestの制約を順に検査する]
// }
[[nodiscard]] KadokaBestManifest deserialize_manifest(std::string_view json);

} // namespace kadoka::best
