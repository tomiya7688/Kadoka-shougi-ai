#include "kadoka/league/match_scheduler.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

// {
//   責務: [expect: 失敗した契約検証を位置と共にテスト失敗へ変換する]
//   引数: [condition: 成立を期待する条件] [message: 失敗理由]
// }
void expect(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// {
//   責務: [make_registry: 3参加者を登録したRegistryを作る]
//   戻り値: [参加者3名のRegistry]
// }
kadoka::best::ParticipantRegistry make_registry() {
    kadoka::best::ParticipantRegistry registry;
    for (const std::string& family : {"alpha", "beta", "gamma"}) {
        kadoka::best::Participant participant;
        participant.family = family;
        participant.model_id = family + ".model";
        participant.model_version = "1.0.0";
        participant.search_config_hash = kadoka::best::compute_config_hash({{"depth", "8"}});
        participant.orchestrator_config_hash = kadoka::best::compute_config_hash({{"mode", "usi"}});
        participant.time_control_config_hash = kadoka::best::compute_config_hash({{"byoyomi_ms", "1000"}});
        participant.id = kadoka::best::participant_id(participant);
        registry.register_participant(std::move(participant));
    }
    return registry;
}

// {
//   責務: [same_match: 対局カードのすべての出力fieldが一致するか比較する]
//   引数: [left: 左側の対局設定] [right: 右側の対局設定]
//   戻り値: [全fieldが一致するならtrue]
// }
bool same_match(const kadoka::league::MatchConfig& left, const kadoka::league::MatchConfig& right) {
    return left.match_id == right.match_id && left.black == right.black && left.white == right.white &&
           left.black_time_control_config_hash == right.black_time_control_config_hash &&
           left.white_time_control_config_hash == right.white_time_control_config_hash && left.seed == right.seed;
}

} // namespace

// {
//   責務: [main: 総当たり、先後バランス、random mode再現性、match config、入力検証を確認する]
//   戻り値: [全検証成功時0]
// }
int main() {
    using kadoka::league::ScheduleMode;
    using kadoka::league::SchedulerOptions;

    const auto registry = make_registry();
    const auto round_robin = kadoka::league::create_schedule(registry, {ScheduleMode::round_robin, 17});
    expect(round_robin.size() == 6, "round robin should create two games for each of three pairs");

    std::map<std::pair<std::string, std::string>, unsigned> pair_counts;
    std::map<std::string, unsigned> first_counts;
    std::map<std::string, unsigned> second_counts;
    for (std::size_t index = 0; index < round_robin.size(); ++index) {
        const auto& match = round_robin[index];
        expect(match.match_id ==
                   "match-" + std::string(8 - std::to_string(index + 1).size(), '0') + std::to_string(index + 1),
               "match IDs should be assigned in stable order");
        expect(!match.black_time_control_config_hash.empty() && !match.white_time_control_config_hash.empty(),
               "match config should carry both participants' time controls");
        const auto ordered = std::minmax(match.black.value, match.white.value);
        ++pair_counts[{ordered.first, ordered.second}];
        ++first_counts[match.black.value];
        ++second_counts[match.white.value];
    }
    expect(pair_counts.size() == 3, "every unique pair should appear");
    for (const auto& [pair, count] : pair_counts) {
        (void)pair;
        expect(count == 2, "every pair should play two color-swapped games");
    }
    for (const auto& [id, count] : first_counts)
        expect(count == second_counts[id], "every participant should have balanced first and second side counts");

    const SchedulerOptions random_options{ScheduleMode::random, 9127};
    const auto first_random = kadoka::league::create_schedule(registry, random_options);
    const auto second_random = kadoka::league::create_schedule(registry, random_options);
    expect(first_random.size() == round_robin.size(), "random mode should preserve full pair coverage");
    for (std::size_t index = 0; index < first_random.size(); ++index)
        expect(same_match(first_random[index], second_random[index]), "fixed seed should reproduce the full schedule");

    bool too_few_rejected = false;
    try {
        kadoka::best::ParticipantRegistry single;
        const auto& participant = registry.participants().begin()->second;
        single.register_participant(participant);
        (void)kadoka::league::create_schedule(single, {ScheduleMode::round_robin, 1});
    } catch (const std::invalid_argument&) {
        too_few_rejected = true;
    }
    expect(too_few_rejected, "scheduling fewer than two participants should be rejected");
}
