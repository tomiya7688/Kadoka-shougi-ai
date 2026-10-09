#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::shogi::ai_package {

/*
{
  責務: [PathHistory: 現在局面と保持対象の着手経路を一貫した形式で公開する]
  処理: [1: 現在局面SFENと着手列を参照する 2: 可逆シリアライズと経路識別子を提供する]
  戻り値: [保持している局面・着手列・形式情報]
}
*/
class PathHistory {
  public:
    /*
    {
      責務: [~PathHistory: 派生履歴を基底型経由で安全に破棄する]
      処理: [1: 仮想デストラクタとして解放を派生型へ委譲する]
      戻り値: []
    }
    */
    virtual ~PathHistory() = default;

    /*
    {
      責務: [format_id: 履歴形式の安定した識別名を返す]
      処理: [1: 派生形式の識別名を参照する]
      戻り値: [形式識別名]
    }
    */
    [[nodiscard]] virtual std::string_view format_id() const noexcept = 0;

    /*
    {
      責務: [current_position_sfen: 記録時点の現在局面を返す]
      処理: [1: 保存済みSFENを参照する]
      戻り値: [現在局面SFEN]
    }
    */
    [[nodiscard]] const std::string& current_position_sfen() const noexcept;

    /*
    {
      責務: [moves_usi: この形式が保持する着手列を返す]
      処理: [1: 初期局面からのUSI着手列を参照する]
      戻り値: [保持中のUSI着手列]
    }
    */
    [[nodiscard]] const std::vector<std::string>& moves_usi() const noexcept;

    /*
    {
      責務: [identity: 現在局面と保持履歴を衝突なく識別する]
      処理: [1: version・形式・局面・着手列を長さ付きで符号化する]
      戻り値: [正準バイナリ識別子]
    }
    */
    [[nodiscard]] std::string identity() const;

    /*
    {
      責務: [serialize: 履歴をversion付きの可逆バイナリ形式へ変換する]
      処理: [1: 派生形式が保持するメタデータと履歴を符号化する]
      戻り値: [保存用バイト列]
    }
    */
    [[nodiscard]] virtual std::string serialize() const = 0;

    /*
    {
      責務: [record: 1着手と着手後の現在局面を履歴へ追加する]
      処理: [1: 入力を検証する 2: 派生形式の保持規則で履歴を更新する]
      引数: [move_usi: 追加するUSI着手, resulting_position_sfen: 着手後の局面]
      戻り値: []
      エラー: [空の着手または局面ならstd::invalid_argument]
    }
    */
    virtual void record(std::string move_usi, std::string resulting_position_sfen) = 0;

  protected:
    /*
    {
      責務: [PathHistory: 履歴形式の共通局面・着手状態を初期化する]
      処理: [1: 初期局面SFENを保存し着手列を空にする]
      引数: [initial_position_sfen: 履歴開始時の局面]
      戻り値: []
    }
    */
    explicit PathHistory(std::string initial_position_sfen);

    std::string current_position_sfen_{};
    std::vector<std::string> moves_usi_{};
};

/*
{
  責務: [FullMovePathHistory: 初期局面以降の全着手を保持する]
  フィールド: [current_position_sfen_: 記録時点の局面, moves_usi_: 全着手列]
  処理: [1: 着手を末尾へ追加する 2: 全経路をversion付きで保存する]
}
*/
class FullMovePathHistory final : public PathHistory {
  public:
    /*
    {
      責務: [FullMovePathHistory: 初期局面を指定して全手履歴を作る]
      処理: [1: 基底履歴を初期SFENで初期化する]
      引数: [initial_position_sfen: 履歴開始時の局面]
      戻り値: []
    }
    */
    explicit FullMovePathHistory(std::string initial_position_sfen);

    /*
    {
      責務: [format_id: 全手履歴形式の識別名を返す]
      処理: [1: 固定形式名を返す]
      戻り値: [kadoka.path_history.full]
    }
    */
    [[nodiscard]] std::string_view format_id() const noexcept override;

    /*
    {
      責務: [serialize: 全手履歴をversion付きで保存する]
      処理: [1: 現在SFENと全着手列を長さ付きで符号化する]
      戻り値: [保存用バイト列]
    }
    */
    [[nodiscard]] std::string serialize() const override;

    /*
    {
      責務: [record: 着手を全手履歴へ追加する]
      処理: [1: 入力を検証する 2: 着手と着手後SFENを保存する]
      引数: [move_usi: 追加する着手, resulting_position_sfen: 着手後SFEN]
      戻り値: []
    }
    */
    void record(std::string move_usi, std::string resulting_position_sfen) override;
};

/*
{
  責務: [LastNMovePathHistory: 指定数までの直近着手を保持する]
  フィールド: [capacity_: 最大保持着手数, current_position_sfen_: 現在局面, moves_usi_: 直近着手列]
  処理: [1: 新しい着手を追加する 2: 上限超過時は最古の保持着手を除く]
}
*/
class LastNMovePathHistory final : public PathHistory {
  public:
    /*
    {
      責務: [LastNMovePathHistory: 保持上限と初期局面を指定して履歴を作る]
      処理: [1: 保持上限と基底局面を保存する]
      引数: [capacity: 保持する最大着手数, initial_position_sfen: 履歴開始時の局面]
      戻り値: []
    }
    */
    LastNMovePathHistory(std::size_t capacity, std::string initial_position_sfen);

    /*
    {
      責務: [format_id: 直近N手履歴形式の識別名を返す]
      処理: [1: 固定形式名を返す]
      戻り値: [kadoka.path_history.last_n]
    }
    */
    [[nodiscard]] std::string_view format_id() const noexcept override;

    /*
    {
      責務: [capacity: この履歴の最大保持数を返す]
      処理: [1: 設定済み上限を参照する]
      戻り値: [最大着手数]
    }
    */
    [[nodiscard]] std::size_t capacity() const noexcept;

    /*
    {
      責務: [serialize: 直近N手履歴を上限付きversion形式で保存する]
      処理: [1: 上限・現在SFEN・保持着手列を符号化する]
      戻り値: [保存用バイト列]
    }
    */
    [[nodiscard]] std::string serialize() const override;

    /*
    {
      責務: [record: 着手を直近N手履歴へ追加する]
      処理: [1: 入力を検証する 2: 上限を超えた最古着手を除く]
      引数: [move_usi: 追加する着手, resulting_position_sfen: 着手後SFEN]
      戻り値: []
    }
    */
    void record(std::string move_usi, std::string resulting_position_sfen) override;

  private:
    std::size_t capacity_{0};
};

/*
{
  責務: [deserialize_full_move_path_history: 保存データから全手履歴を復元する]
  処理: [1: magicとversionを確認する 2: 長さ付き項目を読み形式別に復元する]
  引数: [serialized: 保存済みバイト列]
  戻り値: [復元した履歴]
  エラー: [破損・未知version・未知形式ならstd::invalid_argument]
}
*/
[[nodiscard]] FullMovePathHistory deserialize_full_move_path_history(std::string_view serialized);

/*
{
  責務: [deserialize_path_history: 保存形式に対応する履歴型を復元する]
  処理: [1: headerを検査する 2:形式別の全フィールドを復元する]
  引数: [serialized: 保存済みバイト列]
  戻り値: [復元した履歴への所有ポインタ]
  エラー: [破損・未知version・未知形式ならstd::invalid_argument]
}
*/
[[nodiscard]] std::unique_ptr<PathHistory> deserialize_path_history(std::string_view serialized);

} // namespace kadoka::shogi::ai_package
