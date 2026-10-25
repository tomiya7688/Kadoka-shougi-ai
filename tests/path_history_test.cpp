#include "kadoka/ai_package/path_history.hpp"

#include <stdexcept>
#include <string>
#include <vector>

using namespace kadoka::shogi::ai_package;

namespace {

constexpr const char* kStartSfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";
constexpr const char* kAfterFirstMove = "lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2";
constexpr const char* kAfterSecondMove = "lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3";
constexpr const char* kAfterThirdMove = "lnsgkgsnl/1r5+B1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/7R1/LNSGKGSNL w B 4";

/*
{
  責務: [check: Release buildでも検証を省略せず失敗を報告する]
  引数: [condition: 検証結果]
  エラー: [falseならstd::runtime_error]
}
*/
void check(bool condition) {
    if (!condition) {
        throw std::runtime_error("path-history check failed");
    }
}

/*
{
  責務: [assert_invalid: 不正な入力がinvalid_argumentで拒否されることを確認する]
  引数: [callback: 不正入力を実行する関数]
  エラー: [拒否されなければstd::runtime_error]
}
*/
template <class Callback> void assert_invalid(Callback&& callback) {
    bool rejected = false;
    try {
        callback();
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    check(rejected);
}

} // namespace

/*
{
  責務: [main: 経路の記録・識別・保存復元・破損拒否を検証する]
  戻り値: [すべて成功なら0]
}
*/
int main() {
    FullMovePathHistory full{kStartSfen};
    check(full.moves_usi().empty());
    full.record("7g7f", kAfterFirstMove);
    full.record("3c3d", kAfterSecondMove);

    const FullMovePathHistory restored_full = deserialize_full_move_path_history(full.serialize());
    check(restored_full.current_position_sfen() == kAfterSecondMove);
    check((restored_full.moves_usi() == std::vector<std::string>{"7g7f", "3c3d"}));
    check(restored_full.identity() == full.identity());

    FullMovePathHistory different_path{kStartSfen};
    different_path.record("2g2f", kAfterFirstMove);
    different_path.record("3c3d", kAfterSecondMove);
    check(different_path.current_position_sfen() == full.current_position_sfen());
    check(different_path.identity() != full.identity());

    LastNMovePathHistory recent{2, kStartSfen};
    recent.record("7g7f", kAfterFirstMove);
    recent.record("3c3d", kAfterSecondMove);
    recent.record("8h2b+", kAfterThirdMove);
    check(recent.moves_usi().size() == 2);
    check((recent.moves_usi() == std::vector<std::string>{"3c3d", "8h2b+"}));

    const auto restored_recent = deserialize_path_history(recent.serialize());
    const auto* typed_recent = dynamic_cast<const LastNMovePathHistory*>(restored_recent.get());
    check(typed_recent != nullptr);
    check(typed_recent->capacity() == 2);
    check(typed_recent->current_position_sfen() == kAfterThirdMove);
    check(typed_recent->moves_usi() == recent.moves_usi());
    check(typed_recent->identity() == recent.identity());

    LastNMovePathHistory no_moves{0, kStartSfen};
    no_moves.record("7g7f", kAfterFirstMove);
    check(no_moves.moves_usi().empty());
    check(no_moves.current_position_sfen() == kAfterFirstMove);
    check(deserialize_path_history(no_moves.serialize())->identity() == no_moves.identity());

    const std::string full_before_invalid = full.identity();
    assert_invalid([&] { full.record("", kAfterFirstMove); });
    assert_invalid([&] { full.record("7g7f", ""); });
    check(full.identity() == full_before_invalid);
    const std::string recent_before_invalid = recent.identity();
    assert_invalid([&] { recent.record("", kAfterSecondMove); });
    assert_invalid([&] { recent.record("7g7f", ""); });
    check(recent.identity() == recent_before_invalid);

    /*
    {
      責務: [破損データ検証: version・形式・保持数・切り詰め・余剰byteを拒否する]
      処理: [正規の保存bytesの各header項目を個別に改変して復元する]
    }
    */
    std::string unknown_version = full.serialize();
    unknown_version[11] = 2;
    assert_invalid([&] { (void)deserialize_path_history(unknown_version); });
    std::string unknown_format = full.serialize();
    unknown_format[19] = 3;
    assert_invalid([&] { (void)deserialize_path_history(unknown_format); });
    std::string invalid_capacity = recent.serialize();
    invalid_capacity[27] = 1;
    assert_invalid([&] { (void)deserialize_path_history(invalid_capacity); });
    const std::string serialized_full = full.serialize();
    for (std::size_t length = 0; length < serialized_full.size(); ++length) {
        assert_invalid([&] { (void)deserialize_path_history(serialized_full.substr(0, length)); });
    }
    assert_invalid([&] { (void)deserialize_path_history(full.serialize() + "trailing"); });
    assert_invalid([&] { (void)deserialize_path_history(std::string{"KPH\0", 4}); });
}
