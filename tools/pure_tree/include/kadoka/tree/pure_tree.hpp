#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace kadoka::shogi::tree {

inline constexpr std::uint32_t kPureTreeSchemaVersion = 1;

/*
{
  責務: [
    PureTreeNode: 永続木の局面ノードと探索統計を保持する
  ]
  フィールド: [
    id: ノード識別子 position_identity: 局面識別子 path_identity: 経路識別子 visits: 訪問回数 wins: 勝数 losses: 敗数
draws: 引分数 mean_value: 平均評価値
  ]
  処理: [
    1: 局面・経路識別子と探索統計を保持する
  ]
}
*/
struct PureTreeNode {
    std::string id{};
    std::string position_identity{};
    std::string path_identity{};
    std::uint64_t visits{0};
    std::uint64_t wins{0};
    std::uint64_t losses{0};
    std::uint64_t draws{0};
    double mean_value{0.0};
};

/*
{
  責務: [
    PureTreeEdge: 親子ノードを結ぶ着手と探索統計を保持する
  ]
  フィールド: [
    id: 辺識別子 parent_node_id: 親ノード識別子 child_node_id: 子ノード識別子 move_usi: USI形式の着手 visits: 訪問回数
wins: 勝数 losses: 敗数 draws: 引分数 mean_value: 平均評価値
  ]
  処理: [
    1: 親子参照、着手、探索統計を保持する
  ]
}
*/
struct PureTreeEdge {
    std::string id{};
    std::string parent_node_id{};
    std::string child_node_id{};
    std::string move_usi{};
    std::uint64_t visits{0};
    std::uint64_t wins{0};
    std::uint64_t losses{0};
    std::uint64_t draws{0};
    double mean_value{0.0};
};

/*
{
  責務: [
    PureTree: ルート・ノード・辺からなる永続木を保持する
  ]
  フィールド: [
    root_node_id_: ルート識別子
    nodes_: ノード一覧
    edges_: 辺一覧
    node_ids_: ノードIDの重複確認用索引
    position_path_keys_: 局面・経路ペアの重複確認用索引
    edge_ids_: 辺IDの重複確認用索引
    parent_move_keys_: 親ノード・着手ペアの重複確認用索引
  ]
  処理: [
    1: 空木、ノード、辺、ルートを管理する
  ]
}
*/
class PureTree {
  public:
    /*
{
  責務: [
    PureTree::empty: ルート・ノード・辺がない空木を生成する
  ]
  処理: [
    1: 空の内部状態を作って返す
  ]
  引数: [
  ]
  戻り値: [
    PureTree: 空の木
  ]
}
*/
    [[nodiscard]] static PureTree empty();

    /*
{
  責務: [
    PureTree::add_node: 一意な局面・経路ノードを追加する
  ]
  処理: [
    1: 識別子と統計を検証する
    2: 重複を拒否する
    3: 検証済みノードを一覧に追加する
  ]
  引数: [
    node: 追加するノード
  ]
  戻り値: [
  ]
}
*/
    void add_node(PureTreeNode node);
    /*
{
  責務: [
    PureTree::set_root: 既存ノードをルートに設定する
  ]
  処理: [
    1: 指定IDのノードを検索する
    2: 存在するIDをルートに保存する
  ]
  引数: [
    node_id: ルートにするノード識別子
  ]
  戻り値: [
  ]
}
*/
    void set_root(std::string_view node_id);
    /*
{
  責務: [
    PureTree::add_edge: 既存の親子ノードを結ぶ辺を追加する
  ]
  処理: [
    1: 両端・識別子・統計を検証する
    2: 重複する辺と着手を拒否する
    3: 検証済み辺を一覧に追加する
  ]
  引数: [
    edge: 追加する辺
  ]
  戻り値: [
  ]
}
*/
    void add_edge(PureTreeEdge edge);

    /*
{
  責務: [
    PureTree::root_node_id: ルートノード識別子を参照する
  ]
  処理: [
    1: ルート識別子を読み取り専用で返す
  ]
  引数: [
  ]
  戻り値: [
    1: ルート識別子。空木なら空値
  ]
}
*/
    [[nodiscard]] const std::optional<std::string>& root_node_id() const noexcept {
        return root_node_id_;
    }
    /*
{
  責務: [
    PureTree::nodes: ノード一覧を参照する
  ]
  処理: [
    1: ノード一覧を読み取り専用で返す
  ]
  引数: [
  ]
  戻り値: [
    1: ノード一覧
  ]
}
*/
    [[nodiscard]] const std::vector<PureTreeNode>& nodes() const noexcept {
        return nodes_;
    }
    /*
{
  責務: [
    PureTree::edges: 辺一覧を参照する
  ]
  処理: [
    1: 辺一覧を読み取り専用で返す
  ]
  引数: [
  ]
  戻り値: [
    1: 辺一覧
  ]
}
*/
    [[nodiscard]] const std::vector<PureTreeEdge>& edges() const noexcept {
        return edges_;
    }

  private:
    std::optional<std::string> root_node_id_{};
    std::vector<PureTreeNode> nodes_{};
    std::vector<PureTreeEdge> edges_{};
    std::unordered_set<std::string> node_ids_{};
    std::unordered_set<std::string> position_path_keys_{};
    std::unordered_set<std::string> edge_ids_{};
    std::unordered_set<std::string> parent_move_keys_{};
};

/*
{
  責務: [
    validate_pure_tree: Pure Treeの統計・参照・木構造を検証する
  ]
  処理: [
    1: ルートと空木の規則を確認する
    2: 識別子・統計・参照・親子関係を検証する
    3: 全ノードの到達性と非循環性を確認する
  ]
  引数: [
    tree: 検証する木
  ]
  戻り値: [
  ]
}
*/
void validate_pure_tree(const PureTree& tree);

/*
{
  責務: [
    serialize_pure_tree: Pure Treeをバージョン付きテキストへ変換する
  ]
  処理: [
    1: 木を検証する
    2: バージョン・ルート・ノード・辺を行形式に出力する
  ]
  引数: [
    tree: 直列化する木
  ]
  戻り値: [
    1: UTF-8テキスト
  ]
}
*/
[[nodiscard]] std::string serialize_pure_tree(const PureTree& tree);
/*
{
  責務: [
    deserialize_pure_tree: バージョン付きテキストからPure Treeを復元する
  ]
  処理: [
    1: ヘッダーとルートを解析する
    2: レコードの各フィールドを復元する
    3: 復元した木を検証する
  ]
  引数: [
    data: 読み込むテキスト
  ]
  戻り値: [
    1: 復元した木
  ]
}
*/
[[nodiscard]] PureTree deserialize_pure_tree(std::string_view data);

/*
{
  責務: [
    write_pure_tree: Pure Treeを出力ストリームへ書き込む
  ]
  処理: [
    1: 木をテキストへ直列化する
    2: テキストを出力ストリームに書く
  ]
  引数: [
    output: 書き込み先 tree: 書き込む木
  ]
  戻り値: [
  ]
}
*/
void write_pure_tree(std::ostream& output, const PureTree& tree);
/*
{
  責務: [
    read_pure_tree: 入力ストリームからPure Treeを読む
  ]
  処理: [
    1: ストリーム全体を読み込む
    2: 読み込んだ内容から木を復元する
  ]
  引数: [
    input: 読み込み元
  ]
  戻り値: [
    1: 復元した木
  ]
}
*/
[[nodiscard]] PureTree read_pure_tree(std::istream& input);

/*
{
  責務: [
    save_pure_tree: Pure Treeをファイルへ保存する
  ]
  処理: [
    1: 出力ファイルを切り詰めモードで開く
    2: 木を直列化して保存する
  ]
  引数: [
    path: 保存先 tree: 保存する木
  ]
  戻り値: [
  ]
}
*/
void save_pure_tree(const std::filesystem::path& path, const PureTree& tree);
/*
{
  責務: [
    load_pure_tree: ファイルからPure Treeを読む
  ]
  処理: [
    1: 入力ファイルを開く
    2: 内容を読み木を復元する
  ]
  引数: [
    path: 読み込み元
  ]
  戻り値: [
    1: 復元した木
  ]
}
*/
[[nodiscard]] PureTree load_pure_tree(const std::filesystem::path& path);

} // namespace kadoka::shogi::tree
