#pragma once

#include "kadoka/best/manifest.hpp"

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kadoka::best {

// {
//   責務: [ParticipantKind: 共通Registryへ登録するAI個体の系統を識別する]
// }
enum class ParticipantKind { classical, external, experimental };

// {
//   責務: [ParticipantId: 再現可能な競技個体識別子を保持する]
//   フィールド: [value: sha256接頭辞付きの個体ID]
// }
struct ParticipantId {
    std::string value;

    // {
    //   責務: [operator==: ParticipantIdの値が一致するか比較する]
    //   引数: [other: 比較対象]
    //   戻り値: [同じ識別子ならtrue]
    //   処理: [1: 保持する文字列を比較する]
    // }
    bool operator==(const ParticipantId& other) const = default;
};

// {
//   責務: [Participant: 競技個体の由来、層、モデル、実行設定hashを保持する]
//   フィールド: [id・kind・family・layer: 個体の識別と系統 / model_id・model_version: モデル参照 / 各config_hash:
//   実行設定]
// }
struct Participant {
    ParticipantId id;
    ParticipantKind kind = ParticipantKind::classical;
    std::string family;
    LayerType layer = LayerType::raw;
    std::string model_id;
    std::string model_version;
    std::string search_config_hash;
    std::string orchestrator_config_hash;
    std::string time_control_config_hash;
};

// {
//   責務: [participant_kind_name: ParticipantKindを保存形式の名前へ変換する]
//   引数: [kind: 変換する個体種別]
//   戻り値: [種別名]
//   処理: [1: 既知のenum値を固定名に対応付ける]
// }
[[nodiscard]] std::string_view participant_kind_name(ParticipantKind kind) noexcept;

// {
//   責務: [participant_id: 個体の再現に必要な全設定から安定したSHA-256 IDを作る]
//   引数: [participant: IDを作る個体]
//   戻り値: [sha256:接頭辞を含む再現可能な識別子]
//   処理: [1: 項目を検証し正規化mapのhashを計算する]
// }
[[nodiscard]] ParticipantId participant_id(const Participant& participant);

// {
//   責務: [validate_participant: 登録可能な個体情報と設定hashを検査する]
//   引数: [participant: 検査する個体]
//   戻り値: [問題がなければ空、それ以外は人間向けエラー一覧]
//   処理: [1: 種別・層・ASCII token・hashを検査する]
// }
[[nodiscard]] std::vector<std::string> validate_participant(const Participant& participant);

// {
//   責務: [ParticipantRegistry: 種別に関係なく競技個体を一意に保持する]
//   フィールド: [participants_: ID順に保持する競技個体]
// }
class ParticipantRegistry {
  public:
    // {
    //   責務: [register_participant: IDと内容を検証して個体をRegistryへ追加する]
    //   引数: [participant: 値渡しの個体。右辺値は登録失敗時も呼出し時にmoveされ得る]
    //   処理: [1: 入力とIDを検証し未登録の場合だけ追加する]
    //   戻り値: [なし]
    // }
    void register_participant(Participant participant);

    // {
    //   責務: [find: IDから登録済み個体を検索する]
    //   引数: [id: 検索する識別子]
    //   戻り値: [見つかった個体、または未登録ならnullptr]
    //   処理: [1: IDでmapを検索し登録済み要素を返す]
    // }
    [[nodiscard]] const Participant* find(const ParticipantId& id) const noexcept;

    // {
    //   責務: [participants: 登録内容を安定したID順で参照する]
    //   戻り値: [IDをkeyにした登録済み個体map]
    //   処理: [1: ID順に保持するmapへの参照を返す]
    //   引数: [なし]
    // }
    [[nodiscard]] const std::map<std::string, Participant>& participants() const noexcept;

  private:
    std::map<std::string, Participant> participants_;
};

// {
//   責務: [serialize_registry: Registryを再現可能なversion付きテキストへ変換する]
//   引数: [registry: 保存するRegistry]
//   戻り値: [UTF-8のkadoka.participant_registry.v1文書]
//   処理: [1: headerとID順のtab区切り行を作る]
// }
[[nodiscard]] std::string serialize_registry(const ParticipantRegistry& registry);

// {
//   責務: [deserialize_registry: version付き文書を復元しIDと重複を検証する]
//   引数: [serialized: 読み込む文書]
//   戻り値: [復元したRegistry]
//   処理: [1: headerと行を解析し登録APIで検証する]
// }
[[nodiscard]] ParticipantRegistry deserialize_registry(std::string_view serialized);

} // namespace kadoka::best
