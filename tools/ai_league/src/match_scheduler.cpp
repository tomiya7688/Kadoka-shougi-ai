#include "kadoka/league/match_scheduler.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace kadoka::league {
namespace {

// {
//   責務: [random_below: 一様な64-bit乱数から偏りのない範囲内indexを選ぶ]
//   引数: [generator: seed付き乱数生成器] [upper_bound: 選択範囲の上限]
//   戻り値: [0以上upper_bound未満のindex]
// }
std::uint64_t random_below(std::mt19937_64& generator, std::uint64_t upper_bound) {
    if (upper_bound == 0)
        throw std::invalid_argument("random range must not be empty");
    const std::uint64_t threshold = (std::uint64_t{0} - upper_bound) % upper_bound;
    while (true) {
        const std::uint64_t value = generator();
        if (value >= threshold)
            return value % upper_bound;
    }
}

// {
//   責務: [shuffle_matches: mt19937_64と固定Fisher-Yates手順で対局順を再現可能に並べ替える]
//   引数: [matches: 並べ替える対局一覧] [generator: seed付き乱数生成器]
// }
void shuffle_matches(std::vector<MatchConfig>& matches, std::mt19937_64& generator) {
    for (std::size_t remaining = matches.size(); remaining > 1; --remaining) {
        const std::size_t selected = static_cast<std::size_t>(random_below(generator, remaining));
        std::swap(matches[remaining - 1], matches[selected]);
    }
}

// {
//   責務: [format_match_id: schedule内の連番から固定長のmatch IDを作る]
//   引数: [index: 1始まりのmatch番号]
//   戻り値: [match-と8桁番号からなるID]
// }
std::string format_match_id(std::size_t index) {
    std::ostringstream output;
    output << "match-";
    output.width(8);
    output.fill('0');
    output << index;
    return output.str();
}

// {
//   責務: [make_match: 参加者の先後・時間設定・seedを持つ対局設定を作る]
//   引数: [black: 先手参加者] [white: 後手参加者] [seed: 対局固有seed]
//   戻り値: [まだ連番IDを付けていない対局設定]
// }
MatchConfig make_match(const best::Participant& black, const best::Participant& white, std::uint64_t seed) {
    return {"", black.id, white.id, black.time_control_config_hash, white.time_control_config_hash, seed};
}

} // namespace

// {
//   責務: [create_schedule: Registryから先後バランス済みの総当たり対局カードを生成する]
//   引数: [registry: 対局候補の参加者Registry] [options: 方式とseed]
//   戻り値: [各組合せを先後入替えで含む決定論的な対局設定一覧]
// }
std::vector<MatchConfig> create_schedule(const best::ParticipantRegistry& registry, const SchedulerOptions& options) {
    if (options.mode != ScheduleMode::round_robin && options.mode != ScheduleMode::random)
        throw std::invalid_argument("unsupported schedule mode");
    const auto& participants = registry.participants();
    if (participants.size() < 2)
        throw std::invalid_argument("at least two participants are required");

    std::vector<MatchConfig> matches;
    const std::size_t participant_count = participants.size();
    if (participant_count > std::numeric_limits<std::size_t>::max() / (participant_count - 1))
        throw std::length_error("participant count is too large for a complete schedule");
    matches.reserve(participant_count * (participant_count - 1));
    for (auto first = participants.begin(); first != participants.end(); ++first) {
        for (auto second = std::next(first); second != participants.end(); ++second) {
            matches.push_back(make_match(first->second, second->second, 0));
            matches.push_back(make_match(second->second, first->second, 0));
        }
    }

    std::mt19937_64 generator(options.seed);
    if (options.mode == ScheduleMode::random)
        shuffle_matches(matches, generator);
    for (std::size_t index = 0; index < matches.size(); ++index) {
        matches[index].match_id = format_match_id(index + 1);
        matches[index].seed = generator();
    }
    return matches;
}

} // namespace kadoka::league
