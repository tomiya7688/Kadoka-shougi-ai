#include "kadoka/tree/pure_tree.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace kadoka::shogi::tree {
namespace {

constexpr std::string_view kHeader = "KADOKA_PURE_TREE";

/*
{
  責務: [
    validate_identity: 必須識別子が空でないことを確認する
  ]
  処理: [
    1: 識別子が空でないことを確認する
  ]
  引数: [
    value: 検証する文字列 field: 項目名
  ]
  戻り値: [
  ]
}
*/
void validate_identity(std::string_view value, std::string_view field) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(field) + " must not be empty");
    }
}

/*
{
  責務: [
    validate_stats: 探索統計の整合性と平均値の範囲を検証する
  ]
  処理: [
    1: 結果数と訪問数の整合を確認する
    2: 平均値が有限かつ範囲内であることを確認する
  ]
  引数: [
    visits: 訪問回数 wins: 勝数 losses: 敗数 draws: 引分数 mean_value: 平均評価値
  ]
  戻り値: [
  ]
}
*/
void validate_stats(std::uint64_t visits, std::uint64_t wins, std::uint64_t losses, std::uint64_t draws,
                    double mean_value) {
    if (wins > visits || losses > visits - wins || draws > visits - wins - losses) {
        throw std::invalid_argument("outcome counts must not exceed visit count");
    }
    if (!std::isfinite(mean_value) || mean_value < -1.0 || mean_value > 1.0) {
        throw std::invalid_argument("mean_value must be finite and in [-1, 1]");
    }
}

/*
{
  責務: [
    escape_field: 制御文字を保存可能なエスケープ表現に変換する
  ]
  処理: [
    1: バックスラッシュと制御文字をエスケープする
  ]
  引数: [
    value: 変換する文字列
  ]
  戻り値: [
    1: エスケープ済み文字列
  ]
}
*/
std::string escape_field(std::string_view value) {
    std::string result;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        default:
            if (ch < 0x20U || ch == 0x7fU) {
                constexpr char digits[] = "0123456789abcdef";
                result += "\\x";
                result += digits[ch >> 4];
                result += digits[ch & 0x0fU];
            } else {
                result += static_cast<char>(ch);
            }
        }
    }
    return result;
}

/*
{
  責務: [
    hex_value: 16進数一桁を数値へ変換する
  ]
  処理: [
    1: 16進数一桁を数値へ変換する
  ]
  引数: [
    ch: 変換する文字
  ]
  戻り値: [
    1: 数値。対象外なら負数
  ]
}
*/
int hex_value(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

/*
{
  責務: [
    unescape_field: エスケープ表現から元の文字列を復元する
  ]
  処理: [
    1: エスケープ表現を復元する
    2: 不正なエスケープを拒否する
  ]
  引数: [
    value: 復元する文字列
  ]
  戻り値: [
    1: 復元した文字列
  ]
}
*/
std::string unescape_field(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\') {
            result += value[index];
            continue;
        }
        if (++index >= value.size()) {
            throw std::invalid_argument("truncated field escape");
        }
        switch (value[index]) {
        case '\\':
            result += '\\';
            break;
        case 't':
            result += '\t';
            break;
        case 'n':
            result += '\n';
            break;
        case 'r':
            result += '\r';
            break;
        case 'x': {
            if (index + 2 >= value.size()) {
                throw std::invalid_argument("truncated hexadecimal escape");
            }
            const int high = hex_value(value[index + 1]);
            const int low = hex_value(value[index + 2]);
            if (high < 0 || low < 0) {
                throw std::invalid_argument("invalid hexadecimal escape");
            }
            result += static_cast<char>((high << 4) | low);
            index += 2;
            break;
        }
        default:
            throw std::invalid_argument("unknown field escape");
        }
    }
    return result;
}

/*
{
  責務: [
    split_fields: タブ区切り行をフィールドへ分割する
  ]
  処理: [
    1: タブ位置で分割してフィールドビューを返す
  ]
  引数: [
    line: 分割する行
  ]
  戻り値: [
    1: フィールド一覧
  ]
}
*/
std::vector<std::string_view> split_fields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t tab = line.find('\t', start);
        const std::size_t end = tab == std::string_view::npos ? line.size() : tab;
        fields.push_back(line.substr(start, end - start));
        if (tab == std::string_view::npos)
            break;
        start = tab + 1;
    }
    return fields;
}

template <typename Integer>
/*
{
  責務: [
    parse_integer: 整数フィールド全体を解析する
  ]
  処理: [
    1: 整数全体を解析する
    2: 形式不正と余分な末尾文字を拒否する
  ]
  引数: [
    value: 解析する文字列 field: 項目名
  ]
  戻り値: [
    1: 解析した整数
  ]
}
*/
Integer parse_integer(std::string_view value, std::string_view field) {
    Integer parsed{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("invalid integer field: " + std::string(field));
    }
    return parsed;
}

/*
{
  責務: [
    parse_double: 平均評価値を解析する
  ]
  処理: [
    1: 小数全体を解析する
    2: 形式不正と余分な末尾文字を拒否する
  ]
  引数: [
    value: 解析する文字列
  ]
  戻り値: [
    1: 解析した値
  ]
}
*/
double parse_double(std::string_view value) {
    double parsed = 0.0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), parsed, std::chars_format::general);
    if (error != std::errc{} || end != value.data() + value.size()) {
        throw std::invalid_argument("invalid mean_value field");
    }
    return parsed;
}

/*
{
  責務: [
    format_double: 平均評価値を往復可能な精度で整形する
  ]
  処理: [
    1: ロケール非依存で往復可能な精度に整形する
  ]
  引数: [
    value: 整形する値
  ]
  戻り値: [
    1: 数値文字列
  ]
}
*/
std::string format_double(double value) {
    std::array<char, 64> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                            std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (error != std::errc{}) {
        throw std::runtime_error("failed to format mean_value");
    }
    return std::string(buffer.data(), end);
}

/*
{
  責務: [
    identity_key: 2つの文字列を衝突しない複合キーへ符号化する
  ]
  処理: [
    1: 各要素の長さと内容を順に連結して境界を保持する
  ]
  引数: [
    position: 局面または親識別子 path: 経路または着手
  ]
  戻り値: [
    1: 長さ接頭辞付きキー
  ]
}
*/
std::string identity_key(std::string_view position, std::string_view path) {
    std::string key;
    key.reserve(position.size() + path.size() + 2 * sizeof(std::size_t) + 2);
    key += std::to_string(position.size());
    key += ':';
    key.append(position);
    key += std::to_string(path.size());
    key += ':';
    key.append(path);
    return key;
}

/*
{
  責務: [
    is_valid_usi_move: USIの通常着手・成り・駒打ち表記を検証する
  ]
  処理: [
    1: 駒打ちは駒種と升目の形式を確認する
    2: 通常着手は移動元・移動先と任意の成り記号を確認する
  ]
  引数: [
    move: 検証する着手文字列
  ]
  戻り値: [
    1: USI構文なら真
  ]
}
*/
bool is_valid_usi_move(std::string_view move) {
    const auto valid_square = [](char file, char rank) {
        return file >= '1' && file <= '9' && rank >= 'a' && rank <= 'i';
    };
    if (move.size() == 4 && move[1] == '*') {
        constexpr std::string_view kDroppablePieces = "PLNSGBR";
        return kDroppablePieces.find(move[0]) != std::string_view::npos && valid_square(move[2], move[3]);
    }
    if (move.size() != 4 && move.size() != 5) {
        return false;
    }
    if (move.size() == 5 && move[4] != '+') {
        return false;
    }
    return valid_square(move[0], move[1]) && valid_square(move[2], move[3]);
}

} // namespace

/*
{
  責務: [
    PureTree::empty: 空のPure Treeを生成する
  ]
  処理: [
    1: 空の内部状態を作って返す
  ]
  引数: [
  ]
  戻り値: [
    1: 空の木
  ]
}
*/
PureTree PureTree::empty() {
    return {};
}

/*
{
  責務: [
    PureTree::add_node: 検証済みノードを木へ追加する
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
void PureTree::add_node(PureTreeNode node) {
    validate_identity(node.id, "node id");
    validate_identity(node.position_identity, "position identity");
    validate_identity(node.path_identity, "path identity");
    validate_stats(node.visits, node.wins, node.losses, node.draws, node.mean_value);

    const std::string identity = identity_key(node.position_identity, node.path_identity);
    if (node_ids_.contains(node.id)) {
        throw std::invalid_argument("duplicate node id");
    }
    if (position_path_keys_.contains(identity)) {
        throw std::invalid_argument("duplicate position/path identity");
    }

    const auto [node_id, inserted_id] = node_ids_.insert(node.id);
    if (!inserted_id) {
        throw std::invalid_argument("duplicate node id");
    }
    try {
        const auto [identity_key_it, inserted_identity] = position_path_keys_.insert(identity);
        if (!inserted_identity) {
            throw std::invalid_argument("duplicate position/path identity");
        }
        try {
            nodes_.push_back(std::move(node));
        } catch (...) {
            position_path_keys_.erase(identity_key_it);
            throw;
        }
    } catch (...) {
        node_ids_.erase(node_id);
        throw;
    }
}

/*
{
  責務: [
    PureTree::set_root: 既存ノードを木のルートに設定する
  ]
  処理: [
    1: 指定IDのノードを検索する
    2: 存在するIDをルートに保存する
  ]
  引数: [
    node_id: ルート識別子
  ]
  戻り値: [
  ]
}
*/
void PureTree::set_root(std::string_view node_id) {
    const auto node =
        std::find_if(nodes_.begin(), nodes_.end(), [node_id](const auto& item) { return item.id == node_id; });
    if (node == nodes_.end()) {
        throw std::invalid_argument("root node id does not exist");
    }
    root_node_id_ = node->id;
}

/*
{
  責務: [
    PureTree::add_edge: 検証済み辺を木へ追加する
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
void PureTree::add_edge(PureTreeEdge edge) {
    validate_identity(edge.id, "edge id");
    validate_identity(edge.parent_node_id, "edge parent node id");
    validate_identity(edge.child_node_id, "edge child node id");
    validate_identity(edge.move_usi, "edge move");
    validate_stats(edge.visits, edge.wins, edge.losses, edge.draws, edge.mean_value);
    if (!is_valid_usi_move(edge.move_usi)) {
        throw std::invalid_argument("edge move is not valid USI syntax");
    }
    if (edge.parent_node_id == edge.child_node_id) {
        throw std::invalid_argument("edge must connect different nodes");
    }
    if (!node_ids_.contains(edge.parent_node_id) || !node_ids_.contains(edge.child_node_id)) {
        throw std::invalid_argument("edge references an unknown node");
    }
    if (edge_ids_.contains(edge.id)) {
        throw std::invalid_argument("duplicate edge id");
    }
    const std::string parent_move_key = identity_key(edge.parent_node_id, edge.move_usi);
    if (parent_move_keys_.contains(parent_move_key)) {
        throw std::invalid_argument("duplicate move from parent node");
    }

    const auto [edge_id, inserted_id] = edge_ids_.insert(edge.id);
    if (!inserted_id) {
        throw std::invalid_argument("duplicate edge id");
    }
    try {
        const auto [parent_move, inserted_move] = parent_move_keys_.insert(parent_move_key);
        if (!inserted_move) {
            throw std::invalid_argument("duplicate move from parent node");
        }
        try {
            edges_.push_back(std::move(edge));
        } catch (...) {
            parent_move_keys_.erase(parent_move);
            throw;
        }
    } catch (...) {
        edge_ids_.erase(edge_id);
        throw;
    }
}

/*
{
  責務: [
    validate_pure_tree: ノード・辺・統計・到達可能性を検証する
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
void validate_pure_tree(const PureTree& tree) {
    const auto& nodes = tree.nodes();
    const auto& edges = tree.edges();
    const auto& root = tree.root_node_id();

    if (nodes.empty()) {
        if (root.has_value() || !edges.empty()) {
            throw std::invalid_argument("empty tree cannot have a root or edges");
        }
        return;
    }
    if (!root.has_value()) {
        throw std::invalid_argument("non-empty tree must have a root node");
    }

    std::unordered_map<std::string, const PureTreeNode*> by_id;
    std::unordered_set<std::string> position_paths;
    by_id.reserve(nodes.size());
    position_paths.reserve(nodes.size());
    for (const PureTreeNode& node : nodes) {
        validate_identity(node.id, "node id");
        validate_identity(node.position_identity, "position identity");
        validate_identity(node.path_identity, "path identity");
        validate_stats(node.visits, node.wins, node.losses, node.draws, node.mean_value);
        if (!by_id.emplace(node.id, &node).second) {
            throw std::invalid_argument("duplicate node id");
        }
        if (!position_paths.insert(identity_key(node.position_identity, node.path_identity)).second) {
            throw std::invalid_argument("duplicate position/path identity");
        }
    }
    if (!by_id.contains(*root)) {
        throw std::invalid_argument("root node id does not exist");
    }

    std::unordered_map<std::string, std::size_t> parent_count;
    std::unordered_map<std::string, std::vector<std::string>> children;
    std::unordered_set<std::string> edge_ids;
    std::unordered_set<std::string> parent_moves;
    parent_count.reserve(nodes.size());
    edge_ids.reserve(edges.size());
    for (const PureTreeEdge& edge : edges) {
        validate_identity(edge.id, "edge id");
        validate_identity(edge.move_usi, "edge move");
        validate_stats(edge.visits, edge.wins, edge.losses, edge.draws, edge.mean_value);
        if (!edge_ids.insert(edge.id).second) {
            throw std::invalid_argument("duplicate edge id");
        }
        if (!by_id.contains(edge.parent_node_id) || !by_id.contains(edge.child_node_id)) {
            throw std::invalid_argument("edge references an unknown node");
        }
        if (edge.parent_node_id == edge.child_node_id) {
            throw std::invalid_argument("edge must connect different nodes");
        }
        const std::string move_key = identity_key(edge.parent_node_id, edge.move_usi);
        if (!parent_moves.insert(std::move(move_key)).second) {
            throw std::invalid_argument("duplicate move from parent node");
        }
        const std::size_t count = ++parent_count[edge.child_node_id];
        if (count > 1) {
            throw std::invalid_argument("node has more than one parent");
        }
        children[edge.parent_node_id].push_back(edge.child_node_id);
    }

    if (parent_count.contains(*root)) {
        throw std::invalid_argument("root node cannot have a parent");
    }
    for (const PureTreeNode& node : nodes) {
        if (node.id != *root && parent_count[node.id] != 1) {
            throw std::invalid_argument("every non-root node must have one parent");
        }
    }

    std::unordered_set<std::string> visited;
    std::vector<std::string> pending{*root};
    visited.reserve(nodes.size());
    while (!pending.empty()) {
        std::string current = std::move(pending.back());
        pending.pop_back();
        if (!visited.insert(current).second) {
            throw std::invalid_argument("cycle detected in tree edges");
        }
        const auto children_it = children.find(current);
        if (children_it != children.end()) {
            for (const std::string& child : children_it->second) {
                pending.push_back(child);
            }
        }
    }
    if (visited.size() != nodes.size()) {
        throw std::invalid_argument("tree contains nodes unreachable from root");
    }
}

/*
{
  責務: [
    serialize_pure_tree: 木をバージョン付きテキストへ変換する
  ]
  処理: [
    1: 木を検証する
    2: バージョン・ルート・ノード・辺を行形式に出力する
  ]
  引数: [
    tree: 直列化する木
  ]
  戻り値: [
    1: 直列化テキスト
  ]
}
*/
std::string serialize_pure_tree(const PureTree& tree) {
    validate_pure_tree(tree);
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << kHeader << '\t' << kPureTreeSchemaVersion << '\n';
    output << "ROOT\t";
    if (tree.root_node_id().has_value()) {
        output << escape_field(*tree.root_node_id());
    }
    output << '\n';

    for (const PureTreeNode& node : tree.nodes()) {
        output << "NODE\t" << escape_field(node.id) << '\t' << escape_field(node.position_identity) << '\t'
               << escape_field(node.path_identity) << '\t' << node.visits << '\t' << node.wins << '\t' << node.losses
               << '\t' << node.draws << '\t' << format_double(node.mean_value) << '\n';
    }
    for (const PureTreeEdge& edge : tree.edges()) {
        output << "EDGE\t" << escape_field(edge.id) << '\t' << escape_field(edge.parent_node_id) << '\t'
               << escape_field(edge.child_node_id) << '\t' << escape_field(edge.move_usi) << '\t' << edge.visits << '\t'
               << edge.wins << '\t' << edge.losses << '\t' << edge.draws << '\t' << format_double(edge.mean_value)
               << '\n';
    }
    return output.str();
}

/*
{
  責務: [
    deserialize_pure_tree: テキストを解析し木を復元する
  ]
  処理: [
    1: ヘッダーとルートを解析する
    2: レコードの各フィールドを復元する
    3: 復元した木を検証する
  ]
  引数: [
    data: 入力テキスト
  ]
  戻り値: [
    1: 復元した木
  ]
}
*/
PureTree deserialize_pure_tree(std::string_view data) {
    PureTree tree = PureTree::empty();
    std::size_t line_number = 0;
    std::size_t start = 0;
    bool saw_header = false;
    bool saw_root = false;
    std::optional<std::string> root_id{};
    while (start <= data.size()) {
        const std::size_t newline = data.find('\n', start);
        const std::size_t end = newline == std::string_view::npos ? data.size() : newline;
        std::string_view line = data.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        ++line_number;
        start = newline == std::string_view::npos ? data.size() + 1 : newline + 1;
        if (line.empty())
            continue;

        const std::vector<std::string_view> fields = split_fields(line);
        try {
            if (!saw_header) {
                if (fields.size() != 2 || fields[0] != kHeader ||
                    parse_integer<std::uint32_t>(fields[1], "version") != kPureTreeSchemaVersion) {
                    throw std::invalid_argument("unsupported Pure Tree header/version");
                }
                saw_header = true;
                continue;
            }
            if (!saw_root) {
                if (fields.size() != 2 || fields[0] != "ROOT") {
                    throw std::invalid_argument("expected ROOT record");
                }
                const std::string id = unescape_field(fields[1]);
                if (!id.empty())
                    root_id = id;
                saw_root = true;
                continue;
            }
            if (fields[0] == "NODE") {
                if (fields.size() != 9) {
                    throw std::invalid_argument("NODE record has wrong field count");
                }
                tree.add_node(PureTreeNode{
                    unescape_field(fields[1]),
                    unescape_field(fields[2]),
                    unescape_field(fields[3]),
                    parse_integer<std::uint64_t>(fields[4], "visits"),
                    parse_integer<std::uint64_t>(fields[5], "wins"),
                    parse_integer<std::uint64_t>(fields[6], "losses"),
                    parse_integer<std::uint64_t>(fields[7], "draws"),
                    parse_double(fields[8]),
                });
            } else if (fields[0] == "EDGE") {
                if (fields.size() != 10) {
                    throw std::invalid_argument("EDGE record has wrong field count");
                }
                tree.add_edge(PureTreeEdge{
                    unescape_field(fields[1]),
                    unescape_field(fields[2]),
                    unescape_field(fields[3]),
                    unescape_field(fields[4]),
                    parse_integer<std::uint64_t>(fields[5], "visits"),
                    parse_integer<std::uint64_t>(fields[6], "wins"),
                    parse_integer<std::uint64_t>(fields[7], "losses"),
                    parse_integer<std::uint64_t>(fields[8], "draws"),
                    parse_double(fields[9]),
                });
            } else {
                throw std::invalid_argument("unknown Pure Tree record type");
            }
        } catch (const std::exception& error) {
            throw std::invalid_argument("invalid Pure Tree line " + std::to_string(line_number) + ": " + error.what());
        }
    }
    if (!saw_header || !saw_root) {
        throw std::invalid_argument("Pure Tree data is missing header or root record");
    }
    if (root_id.has_value())
        tree.set_root(*root_id);
    validate_pure_tree(tree);
    return tree;
}

/*
{
  責務: [
    write_pure_tree: テキストを出力ストリームへ書き出す
  ]
  処理: [
    1: 木をテキストへ直列化する
    2: テキストを出力ストリームに書く
  ]
  引数: [
    output: 書き込み先 tree: 出力する木
  ]
  戻り値: [
  ]
}
*/
void write_pure_tree(std::ostream& output, const PureTree& tree) {
    const std::string data = serialize_pure_tree(tree);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!output)
        throw std::runtime_error("failed to write Pure Tree data");
}

/*
{
  責務: [
    read_pure_tree: 入力ストリームから木を復元する
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
PureTree read_pure_tree(std::istream& input) {
    const std::string data{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad())
        throw std::runtime_error("failed to read Pure Tree data");
    return deserialize_pure_tree(data);
}

/*
{
  責務: [
    save_pure_tree: 木をファイルへ保存する
  ]
  処理: [
    1: 出力ファイルを切り詰めモードで開く
    2: 木を直列化して保存する
    3: flushとcloseの結果を検査する
  ]
  引数: [
    path: 保存先 tree: 保存する木
  ]
  戻り値: [
  ]
}
*/
void save_pure_tree(const std::filesystem::path& path, const PureTree& tree) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("failed to open Pure Tree output file");
    write_pure_tree(output, tree);
    output.flush();
    if (!output) {
        throw std::runtime_error("failed to flush Pure Tree output file");
    }
    output.close();
    if (output.fail()) {
        throw std::runtime_error("failed to close Pure Tree output file");
    }
}

/*
{
  責務: [
    load_pure_tree: ファイルから木を復元する
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
PureTree load_pure_tree(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("failed to open Pure Tree input file");
    return read_pure_tree(input);
}

} // namespace kadoka::shogi::tree
