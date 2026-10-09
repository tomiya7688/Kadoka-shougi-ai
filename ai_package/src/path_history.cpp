#include "kadoka/ai_package/path_history.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace kadoka::shogi::ai_package {
namespace {

constexpr std::string_view kMagic{"KPH\0", 4};
constexpr std::uint64_t kVersion = 1;
constexpr std::uint64_t kFullMoveKind = 1;
constexpr std::uint64_t kLastNMoveKind = 2;

/*
{
  責務: [append_u64: 符号なし整数をbyte順固定の8byte形式で追加する]
  処理: [1: 上位byteから順に出力する]
  引数: [output: 追記先, value: 符号化する値]
  戻り値: []
}
*/
void append_u64(std::string& output, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

/*
{
  責務: [read_u64: 8byteの整数を境界検査しながら読み取る]
  処理: [1: 入力残量を検証する 2: byte順固定の値を復元する]
  引数: [input: 保存bytes, offset: 読取位置]
  戻り値: [復元した符号なし整数]
  エラー: [切り詰め入力ならstd::invalid_argument]
}
*/
std::uint64_t read_u64(std::string_view input, std::size_t& offset) {
    if (input.size() - offset < 8) {
        throw std::invalid_argument("truncated path-history integer");
    }
    std::uint64_t value = 0;
    for (int byte = 0; byte < 8; ++byte) {
        value = (value << 8)
            | static_cast<unsigned char>(input[offset++]);
    }
    return value;
}

/*
{
  責務: [append_string: 文字列をbyte長付きで履歴形式へ追加する]
  処理: [1: byte長を記録する 2: 内容をそのまま追記する]
  引数: [output: 追記先, value: 保存する文字列]
  戻り値: []
}
*/
void append_string(std::string& output, std::string_view value) {
    append_u64(output, static_cast<std::uint64_t>(value.size()));
    output.append(value);
}

/*
{
  責務: [read_string: 長さ付き文字列を境界検査しながら読み取る]
  処理: [1: 長さを読む 2: 残量を検証して該当範囲を取得する]
  引数: [input: 保存bytes, offset: 読取位置]
  戻り値: [復元した文字列]
  エラー: [不正長または切り詰め入力ならstd::invalid_argument]
}
*/
std::string read_string(std::string_view input, std::size_t& offset) {
    const std::uint64_t length = read_u64(input, offset);
    if (length > input.size() - offset) {
        throw std::invalid_argument("truncated path-history string");
    }
    const auto size = static_cast<std::size_t>(length);
    std::string value{input.substr(offset, size)};
    offset += size;
    return value;
}

/*
{
  責務: [append_header: 履歴形式のmagic・version・種別を出力する]
  処理: [1: 固定magicとversionを追記する 2: 種別と保持上限を追記する]
  引数: [output: 追記先, kind: 履歴形式, capacity: 最大保持数]
  戻り値: []
}
*/
void append_header(
    std::string& output,
    std::uint64_t kind,
    std::uint64_t capacity
) {
    output.append(kMagic);
    append_u64(output, kVersion);
    append_u64(output, kind);
    append_u64(output, capacity);
}

/*
{
  責務: [validate_header: 保存bytesのmagic・version・形式を確認する]
  処理: [1: magicを比較する 2: 対応versionと既知の形式を検証する]
  引数: [input: 保存bytes, offset: header後の読取位置, kind: 形式出力, capacity: 上限出力]
  戻り値: []
  エラー: [未知形式・version・破損headerならstd::invalid_argument]
}
*/
void validate_header(
    std::string_view input,
    std::size_t& offset,
    std::uint64_t& kind,
    std::uint64_t& capacity
) {
    if (input.size() < kMagic.size()
        || input.substr(0, kMagic.size()) != kMagic) {
        throw std::invalid_argument("invalid path-history magic");
    }
    offset = kMagic.size();
    const std::uint64_t version = read_u64(input, offset);
    if (version != kVersion) {
        throw std::invalid_argument("unsupported path-history version");
    }
    kind = read_u64(input, offset);
    capacity = read_u64(input, offset);
    if (kind != kFullMoveKind && kind != kLastNMoveKind) {
        throw std::invalid_argument("unknown path-history format");
    }
    if (kind == kFullMoveKind && capacity != 0) {
        throw std::invalid_argument("full path-history has unexpected capacity");
    }
    if (capacity > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("path-history capacity is too large");
    }
}

/*
{
  責務: [append_payload: 現在局面と保持着手列を共通payloadとして符号化する]
  処理: [1: 現在SFENを記録する 2: 着手数と各着手を長さ付きで記録する]
  引数: [output: 追記先, current_sfen: 現在局面, moves: 保持着手列]
  戻り値: []
}
*/
void append_payload(
    std::string& output,
    std::string_view current_sfen,
    const std::vector<std::string>& moves
) {
    append_string(output, current_sfen);
    append_u64(output, static_cast<std::uint64_t>(moves.size()));
    for (const std::string& move : moves) {
        append_string(output, move);
    }
}

/*
{
  責務: [read_moves: 着手数と着手文字列を検証しながら復元する]
  処理: [1: 着手数が入力残量に収まることを確認する 2: 各着手を読む]
  引数: [input: 保存bytes, offset: 読取位置]
  戻り値: [復元した着手列]
  エラー: [過大な個数または空着手ならstd::invalid_argument]
}
*/
std::vector<std::string> read_moves(
    std::string_view input,
    std::size_t& offset
) {
    const std::uint64_t count = read_u64(input, offset);
    if (count > (input.size() - offset) / 8) {
        throw std::invalid_argument("invalid path-history move count");
    }
    std::vector<std::string> moves;
    moves.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t index = 0; index < count; ++index) {
        std::string move = read_string(input, offset);
        if (move.empty()) {
            throw std::invalid_argument("empty path-history move");
        }
        moves.push_back(std::move(move));
    }
    return moves;
}

/*
{
  責務: [ensure_complete: 復元後に余剰byteがないことを確認する]
  処理: [1: 読取位置が入力末尾と一致するか検査する]
  引数: [input: 保存bytes, offset: 最終読取位置]
  戻り値: []
  エラー: [余剰byteがある場合std::invalid_argument]
}
*/
void ensure_complete(std::string_view input, std::size_t offset) {
    if (offset != input.size()) {
        throw std::invalid_argument("trailing bytes in path-history data");
    }
}

} // namespace

PathHistory::PathHistory(std::string initial_position_sfen)
    : current_position_sfen_(std::move(initial_position_sfen)) {
    if (current_position_sfen_.empty()) {
        throw std::invalid_argument("initial position SFEN must not be empty");
    }
}

const std::string& PathHistory::current_position_sfen() const noexcept {
    return current_position_sfen_;
}

const std::vector<std::string>& PathHistory::moves_usi() const noexcept {
    return moves_usi_;
}

std::string PathHistory::identity() const {
    return serialize();
}

FullMovePathHistory::FullMovePathHistory(std::string initial_position_sfen)
    : PathHistory(std::move(initial_position_sfen)) {}

std::string_view FullMovePathHistory::format_id() const noexcept {
    return "kadoka.path_history.full";
}

std::string FullMovePathHistory::serialize() const {
    std::string output;
    output.reserve(36 + current_position_sfen_.size());
    append_header(output, kFullMoveKind, 0);
    append_payload(output, current_position_sfen_, moves_usi_);
    return output;
}

void FullMovePathHistory::record(
    std::string move_usi,
    std::string resulting_position_sfen
) {
    if (move_usi.empty() || resulting_position_sfen.empty()) {
        throw std::invalid_argument("move and resulting SFEN must not be empty");
    }
    moves_usi_.push_back(std::move(move_usi));
    current_position_sfen_ = std::move(resulting_position_sfen);
}

LastNMovePathHistory::LastNMovePathHistory(
    std::size_t capacity,
    std::string initial_position_sfen
)
    : PathHistory(std::move(initial_position_sfen)), capacity_(capacity) {}

std::string_view LastNMovePathHistory::format_id() const noexcept {
    return "kadoka.path_history.last_n";
}

std::size_t LastNMovePathHistory::capacity() const noexcept {
    return capacity_;
}

std::string LastNMovePathHistory::serialize() const {
    std::string output;
    output.reserve(44 + current_position_sfen_.size());
    append_header(output, kLastNMoveKind, static_cast<std::uint64_t>(capacity_));
    append_payload(output, current_position_sfen_, moves_usi_);
    return output;
}

void LastNMovePathHistory::record(
    std::string move_usi,
    std::string resulting_position_sfen
) {
    if (move_usi.empty() || resulting_position_sfen.empty()) {
        throw std::invalid_argument("move and resulting SFEN must not be empty");
    }
    current_position_sfen_ = std::move(resulting_position_sfen);
    if (capacity_ == 0) {
        moves_usi_.clear();
        return;
    }
    moves_usi_.push_back(std::move(move_usi));
    if (moves_usi_.size() > capacity_) {
        moves_usi_.erase(moves_usi_.begin());
    }
}

FullMovePathHistory deserialize_full_move_path_history(
    std::string_view serialized
) {
    std::size_t offset = 0;
    std::uint64_t kind = 0;
    std::uint64_t capacity = 0;
    validate_header(serialized, offset, kind, capacity);
    if (kind != kFullMoveKind) {
        throw std::invalid_argument("path-history format is not full-move");
    }
    std::string current_sfen = read_string(serialized, offset);
    if (current_sfen.empty()) {
        throw std::invalid_argument("path-history current SFEN is empty");
    }
    std::vector<std::string> moves = read_moves(serialized, offset);
    ensure_complete(serialized, offset);

    FullMovePathHistory history{std::move(current_sfen)};
    for (std::string& move : moves) {
        history.record(std::move(move), history.current_position_sfen());
    }
    return history;
}

std::unique_ptr<PathHistory> deserialize_path_history(
    std::string_view serialized
) {
    std::size_t offset = 0;
    std::uint64_t kind = 0;
    std::uint64_t capacity = 0;
    validate_header(serialized, offset, kind, capacity);
    std::string current_sfen = read_string(serialized, offset);
    if (current_sfen.empty()) {
        throw std::invalid_argument("path-history current SFEN is empty");
    }
    std::vector<std::string> moves = read_moves(serialized, offset);
    ensure_complete(serialized, offset);

    if (kind == kFullMoveKind) {
        auto history = std::make_unique<FullMovePathHistory>(
            std::move(current_sfen)
        );
        for (std::string& move : moves) {
            history->record(std::move(move), history->current_position_sfen());
        }
        return history;
    }

    if (moves.size() > capacity) {
        throw std::invalid_argument("path-history exceeds its configured capacity");
    }
    auto history = std::make_unique<LastNMovePathHistory>(
        static_cast<std::size_t>(capacity),
        std::move(current_sfen)
    );
    for (std::string& move : moves) {
        history->record(std::move(move), history->current_position_sfen());
    }
    return history;
}

} // namespace kadoka::shogi::ai_package

