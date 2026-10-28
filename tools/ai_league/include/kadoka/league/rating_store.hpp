#pragma once

#include "kadoka/best/participant_registry.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::league {

// {
//   責務: [GameResult: 先手視点で確定した対局結果を表す]
// }
enum class GameResult { black_win, draw, white_win };

// {
//   責務: [RatingSnapshot: 参加者のGlicko rating、RD（不確実性）、対局数を保持する]
// }
struct RatingSnapshot {
    best::ParticipantId participant;
    double rating = 1500.0;
    double uncertainty = 350.0;
    std::uint64_t games = 0;
};

// {
//   責務: [RatingHistoryEntry: 1対局の入力結果と更新後ratingを記録する]
// }
struct RatingHistoryEntry {
    std::uint64_t sequence = 0;
    best::ParticipantId black;
    best::ParticipantId white;
    GameResult result = GameResult::draw;
    double black_rating_after = 1500.0;
    double black_uncertainty_after = 350.0;
    double white_rating_after = 1500.0;
    double white_uncertainty_after = 350.0;
};

// {
//   責務: [RatingStore: ParticipantRegistryの個体ratingと対局ごとの更新履歴を保持する]
// }
class RatingStore {
  public:
    // {
    //   責務: [RatingStore: Registry内の全参加者を初期ratingで登録する]
    //   引数: [registry: rating対象のParticipantRegistry]
    // }
    explicit RatingStore(const best::ParticipantRegistry& registry);

    // {
    //   責務: [RatingStore: ID一覧から初期ratingを構成する]
    //   引数: [participants: rating対象の一意なParticipantId一覧]
    // }
    explicit RatingStore(const std::vector<best::ParticipantId>& participants);

    // {
    //   責務: [find: ParticipantIdから最新rating snapshotを検索する]
    //   引数: [participant: 検索する個体ID]
    //   戻り値: [登録済みsnapshot、または未登録ならnullptr]
    // }
    [[nodiscard]] const RatingSnapshot* find(const best::ParticipantId& participant) const noexcept;

    // {
    //   責務: [participants: 登録済みratingを安定ID順で参照する]
    //   戻り値: [ParticipantIdをkeyにしたrating map]
    // }
    [[nodiscard]] const std::map<std::string, RatingSnapshot>& participants() const noexcept;

    // {
    //   責務: [history: 対局順に追加されたrating更新履歴を参照する]
    //   戻り値: [追記専用のrating履歴一覧]
    // }
    [[nodiscard]] const std::vector<RatingHistoryEntry>& history() const noexcept;

    // {
    //   責務: [record_result: 1局の結果を反映して両者のGlicko rating/RDを更新する]
    //   引数: [black: 先手参加者ID] [white: 後手参加者ID] [result: 対局結果]
    // }
    void record_result(const best::ParticipantId& black, const best::ParticipantId& white, GameResult result);

  private:
    std::map<std::string, RatingSnapshot> participants_;
    std::vector<RatingHistoryEntry> history_;
};

// {
//   責務: [serialize_rating_store: snapshotと再生可能な対局履歴をversion付き形式へ変換する]
//   引数: [store: 保存するRatingStore]
//   戻り値: [UTF-8のkadoka.rating_store.v1文書]
// }
[[nodiscard]] std::string serialize_rating_store(const RatingStore& store);

// {
//   責務: [deserialize_rating_store: participant一覧と結果履歴からratingを再計算して復元する]
//   引数: [serialized: 読み込むrating文書]
//   戻り値: [復元したRatingStore]
//   例外: [std::runtime_error: 保存形式または保存内容が不正]
// }
[[nodiscard]] RatingStore deserialize_rating_store(std::string_view serialized);

} // namespace kadoka::league
