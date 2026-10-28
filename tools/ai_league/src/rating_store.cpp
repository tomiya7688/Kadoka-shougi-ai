#include "kadoka/league/rating_store.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace kadoka::league {
namespace {

// {
//   責務: [is_participant_id: ParticipantIdがsha256固定表記か検査する]
//   引数: [value: 検査するID]
//   戻り値: [sha256:と小文字hex 64桁の形式ならtrue]
// }
bool is_participant_id(std::string_view value) {
    if (value.size() != 71 || value.substr(0, 7) != "sha256:")
        return false;
    for (const char character : value.substr(7)) {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
            return false;
    }
    return true;
}

// {
//   責務: [result_name: GameResultを保存形式の名前へ変換する]
//   引数: [result: 変換する対局結果]
//   戻り値: [black_win / draw / white_winのいずれか]
// }
std::string_view result_name(GameResult result) noexcept {
    switch (result) {
    case GameResult::black_win:
        return "black_win";
    case GameResult::draw:
        return "draw";
    case GameResult::white_win:
        return "white_win";
    }
    return "invalid";
}

// {
//   責務: [parse_result: 保存形式の結果名をGameResultへ変換する]
//   引数: [value: 保存形式の結果名]
//   戻り値: [復元した結果]
// }
GameResult parse_result(std::string_view value) {
    if (value == "black_win")
        return GameResult::black_win;
    if (value == "draw")
        return GameResult::draw;
    if (value == "white_win")
        return GameResult::white_win;
    throw std::runtime_error("unknown rating result");
}

// {
//   責務: [split_fields: 1行をtab区切りfieldへ分割する]
//   引数: [line: 分割する行]
//   戻り値: [空fieldを保持したfield一覧]
// }
std::vector<std::string_view> split_fields(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    while (true) {
        const std::size_t separator = line.find('\t', start);
        if (separator == std::string_view::npos) {
            fields.push_back(line.substr(start));
            return fields;
        }
        if (fields.size() == 8)
            throw std::runtime_error("too many rating fields");
        fields.push_back(line.substr(start, separator - start));
        start = separator + 1;
    }
}

// {
//   責務: [parse_unsigned: 数字field全体をuint64_tとして厳密に読み取る]
//   引数: [value: 読み取るfield]
//   戻り値: [読み取った整数]
// }
std::uint64_t parse_unsigned(std::string_view value) {
    std::uint64_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size())
        throw std::runtime_error("invalid unsigned rating field");
    return result;
}

// {
//   責務: [parse_double: 数値field全体を有限doubleとして厳密に読み取る]
//   引数: [value: 読み取るfield]
//   戻り値: [読み取った有限double]
// }
double parse_double(std::string_view value) {
    double result = 0.0;
    const auto [end, error] =
        std::from_chars(value.data(), value.data() + value.size(), result, std::chars_format::general);
    if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(result))
        throw std::runtime_error("invalid finite rating field");
    return result;
}

// {
//   責務: [ratings_match: 異なる標準数学ライブラリ間の丸め差を許容して履歴値を照合する]
//   引数: [left: 再計算値] [right: 保存値]
//   戻り値: [相対誤差が1e-12以内ならtrue]
// }
bool ratings_match(double left, double right) {
    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= scale * 1e-12;
}

// {
//   責務: [format_double: doubleをround-trip可能な最短形式へ変換する]
//   引数: [value: 変換するrating値]
//   戻り値: [locale非依存の10進表記]
// }
std::string format_double(double value) {
    std::array<char, 64> buffer{};
    const auto [end, error] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                            std::chars_format::general, std::numeric_limits<double>::max_digits10);
    if (error != std::errc{})
        throw std::runtime_error("could not format rating value");
    return {buffer.data(), end};
}

// {
//   責務: [append_field: tab区切りrecordの文字列fieldを追加する]
//   引数: [output: 書き込み先] [value: 追加する文字列]
// }
void append_field(std::string& output, std::string_view value) {
    output.append(value);
    output.push_back('\t');
}

// {
//   責務: [append_field: tab区切りrecordの整数fieldを追加する]
//   引数: [output: 書き込み先] [value: 追加する整数]
// }
void append_field(std::string& output, std::uint64_t value) {
    append_field(output, std::to_string(value));
}

// {
//   責務: [append_field: tab区切りrecordのdouble fieldを追加する]
//   引数: [output: 書き込み先] [value: 追加する数値]
// }
void append_field(std::string& output, double value) {
    append_field(output, format_double(value));
}

// {
//   責務: [glicko_update: Glicko-1の単局更新からratingとRDを計算する]
//   引数: [current: 更新する参加者snapshot] [opponent: 相手snapshot] [score: 0 / 0.5 / 1の結果]
//   戻り値: [更新後のratingとuncertainty]
// }
std::pair<double, double> glicko_update(const RatingSnapshot& current, const RatingSnapshot& opponent, double score) {
    constexpr double scale = 173.7178;
    constexpr double minimum_uncertainty = 30.0;
    constexpr double pi = 3.14159265358979323846;

    const double mu = (current.rating - 1500.0) / scale;
    const double opponent_mu = (opponent.rating - 1500.0) / scale;
    const double phi = current.uncertainty / scale;
    const double opponent_phi = opponent.uncertainty / scale;
    const double g = 1.0 / std::sqrt(1.0 + 3.0 * opponent_phi * opponent_phi / (pi * pi));
    const double exponent = -g * (mu - opponent_mu);
    const double expected =
        exponent >= 0.0 ? std::exp(-exponent) / (1.0 + std::exp(-exponent)) : 1.0 / (1.0 + std::exp(exponent));
    const double information = g * g * expected * (1.0 - expected);
    const double phi_squared = 1.0 / (1.0 / (phi * phi) + information);
    const double next_phi = std::sqrt(phi_squared);
    const double next_mu = mu + phi_squared * g * (score - expected);
    const double next_rating = 1500.0 + scale * next_mu;
    const double next_uncertainty = std::max(minimum_uncertainty, scale * next_phi);
    if (!std::isfinite(next_rating) || !std::isfinite(next_uncertainty))
        throw std::overflow_error("rating update produced a non-finite value");
    return {next_rating, next_uncertainty};
}

} // namespace

// {
//   責務: [RatingStore: Registry内の全参加者を初期ratingで登録する]
//   引数: [registry: rating対象のParticipantRegistry]
// }
RatingStore::RatingStore(const best::ParticipantRegistry& registry)
    : RatingStore([&registry] {
          std::vector<best::ParticipantId> ids;
          ids.reserve(registry.participants().size());
          for (const auto& [id, participant] : registry.participants()) {
              (void)id;
              ids.push_back(participant.id);
          }
          return ids;
      }()) {}

// {
//   責務: [RatingStore: ID一覧から初期ratingを構成する]
//   引数: [participants: rating対象の一意なParticipantId一覧]
// }
RatingStore::RatingStore(const std::vector<best::ParticipantId>& participants) {
    for (const best::ParticipantId& participant : participants) {
        if (!is_participant_id(participant.value))
            throw std::invalid_argument("participant ID must be a lowercase SHA-256 identifier");
        const auto [entry, inserted] =
            participants_.emplace(participant.value, RatingSnapshot{participant, 1500.0, 350.0, 0});
        (void)entry;
        if (!inserted)
            throw std::invalid_argument("duplicate participant ID in rating store");
    }
}

// {
//   責務: [find: ParticipantIdから最新rating snapshotを検索する]
//   引数: [participant: 検索する個体ID]
//   戻り値: [登録済みsnapshot、または未登録ならnullptr]
// }
const RatingSnapshot* RatingStore::find(const best::ParticipantId& participant) const noexcept {
    const auto found = participants_.find(participant.value);
    return found == participants_.end() ? nullptr : &found->second;
}

// {
//   責務: [participants: 登録済みratingを安定ID順で参照する]
//   戻り値: [ParticipantIdをkeyにしたrating map]
// }
const std::map<std::string, RatingSnapshot>& RatingStore::participants() const noexcept {
    return participants_;
}

// {
//   責務: [history: 対局順に追加されたrating更新履歴を参照する]
//   戻り値: [追記専用のrating履歴一覧]
// }
const std::vector<RatingHistoryEntry>& RatingStore::history() const noexcept {
    return history_;
}

// {
//   責務: [record_result: 1局の結果を反映して両者のGlicko rating/RDを更新する]
//   引数: [black: 先手参加者ID] [white: 後手参加者ID] [result: 対局結果]
// }
void RatingStore::record_result(const best::ParticipantId& black, const best::ParticipantId& white, GameResult result) {
    const auto black_entry = participants_.find(black.value);
    const auto white_entry = participants_.find(white.value);
    if (black_entry == participants_.end() || white_entry == participants_.end())
        throw std::invalid_argument("rating result references an unknown participant");
    if (black.value == white.value)
        throw std::invalid_argument("a participant cannot play against itself");
    if (result_name(result) == "invalid")
        throw std::invalid_argument("rating result is invalid");
    if (black_entry->second.games == std::numeric_limits<std::uint64_t>::max() ||
        white_entry->second.games == std::numeric_limits<std::uint64_t>::max() ||
        (!history_.empty() && history_.back().sequence == std::numeric_limits<std::uint64_t>::max()))
        throw std::overflow_error("rating game count or history sequence overflow");

    const double black_score = result == GameResult::black_win ? 1.0 : (result == GameResult::draw ? 0.5 : 0.0);
    const double white_score = 1.0 - black_score;
    const auto [black_rating, black_uncertainty] = glicko_update(black_entry->second, white_entry->second, black_score);
    const auto [white_rating, white_uncertainty] = glicko_update(white_entry->second, black_entry->second, white_score);
    const std::uint64_t sequence = history_.empty() ? 1 : history_.back().sequence + 1;
    history_.push_back(
        {sequence, black, white, result, black_rating, black_uncertainty, white_rating, white_uncertainty});
    black_entry->second.rating = black_rating;
    black_entry->second.uncertainty = black_uncertainty;
    ++black_entry->second.games;
    white_entry->second.rating = white_rating;
    white_entry->second.uncertainty = white_uncertainty;
    ++white_entry->second.games;
}

// {
//   責務: [serialize_rating_store: snapshotと再生可能な対局履歴をversion付き形式へ変換する]
//   引数: [store: 保存するRatingStore]
//   戻り値: [UTF-8のkadoka.rating_store.v1文書]
// }
std::string serialize_rating_store(const RatingStore& store) {
    std::string output = "kadoka.rating_store.v1\n";
    for (const auto& [id, snapshot] : store.participants()) {
        (void)snapshot;
        output += "P\t" + id + "\n";
    }
    for (const RatingHistoryEntry& entry : store.history()) {
        output += "G\t";
        append_field(output, entry.sequence);
        append_field(output, entry.black.value);
        append_field(output, entry.white.value);
        append_field(output, result_name(entry.result));
        append_field(output, entry.black_rating_after);
        append_field(output, entry.black_uncertainty_after);
        append_field(output, entry.white_rating_after);
        output += format_double(entry.white_uncertainty_after);
        output.push_back('\n');
    }
    return output;
}

// {
//   責務: [deserialize_rating_store: participant一覧と結果履歴からratingを再計算して復元する]
//   引数: [serialized: 読み込むrating文書]
//   戻り値: [復元したRatingStore]
// }
RatingStore deserialize_rating_store(std::string_view serialized) {
    constexpr std::string_view header = "kadoka.rating_store.v1\n";
    if (!serialized.starts_with(header))
        throw std::runtime_error("unsupported rating store format");
    std::vector<best::ParticipantId> ids;
    bool reading_games = false;
    std::size_t games_offset = serialized.size();
    std::string_view remaining = serialized.substr(header.size());
    while (!remaining.empty()) {
        const std::size_t newline = remaining.find('\n');
        if (newline == std::string_view::npos)
            throw std::runtime_error("rating record must end with a newline");
        std::string_view line = remaining.substr(0, newline);
        remaining.remove_prefix(newline + 1);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (line.empty())
            throw std::runtime_error("rating record must not be empty");
        const auto fields = split_fields(line);
        if (fields[0] == "P") {
            if (reading_games || fields.size() != 2 || !is_participant_id(fields[1]))
                throw std::runtime_error("invalid participant rating record");
            ids.push_back({std::string(fields[1])});
        } else if (fields[0] == "G") {
            reading_games = true;
            if (fields.size() != 9)
                throw std::runtime_error("rating game record must contain nine fields");
            if (games_offset == serialized.size())
                games_offset = static_cast<std::size_t>(line.data() - serialized.data());
        } else {
            throw std::runtime_error("unknown rating record type");
        }
    }

    RatingStore result = [&ids] {
        try {
            return RatingStore(ids);
        } catch (const std::invalid_argument& error) {
            throw std::runtime_error(std::string("invalid rating participants: ") + error.what());
        }
    }();
    remaining = serialized.substr(games_offset);
    while (!remaining.empty()) {
        const std::size_t newline = remaining.find('\n');
        std::string_view line = remaining.substr(0, newline);
        remaining.remove_prefix(newline + 1);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        const auto fields = split_fields(line);
        const std::uint64_t sequence = parse_unsigned(fields[1]);
        const GameResult game_result = parse_result(fields[4]);
        if (sequence != result.history().size() + 1)
            throw std::runtime_error("rating history sequence is not contiguous");
        try {
            result.record_result({std::string(fields[2])}, {std::string(fields[3])}, game_result);
        } catch (const std::invalid_argument& error) {
            throw std::runtime_error(std::string("invalid rating game: ") + error.what());
        }
        const RatingHistoryEntry& actual = result.history().back();
        if (!ratings_match(actual.black_rating_after, parse_double(fields[5])) ||
            !ratings_match(actual.black_uncertainty_after, parse_double(fields[6])) ||
            !ratings_match(actual.white_rating_after, parse_double(fields[7])) ||
            !ratings_match(actual.white_uncertainty_after, parse_double(fields[8])))
            throw std::runtime_error("stored rating history does not match replayed result");
    }
    return result;
}

} // namespace kadoka::league
