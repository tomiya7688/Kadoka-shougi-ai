#include "kadoka/league/match_scheduler.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <locale>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class GroupedNumberPunctuation final : public std::numpunct<char> {
  protected:
    char do_thousands_sep() const override {
        return ',';
    }
    std::string do_grouping() const override {
        return "\3";
    }
};

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
kadoka::best::ParticipantRegistry make_registry(const std::vector<std::string>& families) {
    kadoka::best::ParticipantRegistry registry;
    for (const std::string& family : families) {
        kadoka::best::Participant participant;
        participant.family = family;
        participant.model_id = family + ".model";
        participant.model_version = "1.0.0";
        participant.search_config_hash = kadoka::best::compute_config_hash({{"depth", "8"}});
        participant.orchestrator_config_hash = kadoka::best::compute_config_hash({{"mode", "usi"}});
        const std::string byoyomi_ms = family == "alpha" ? "1000" : family == "beta" ? "2000" : "3000";
        participant.time_control_config_hash = kadoka::best::compute_config_hash({{"byoyomi_ms", byoyomi_ms}});
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

// {
//   責務: [expect_pairwise_color_balance: 全参加者ペアの網羅と先後入替えを検証する]
//   引数: [matches: 検査対象の対局一覧]
// }
void expect_pairwise_color_balance(const std::vector<kadoka::league::MatchConfig>& matches) {
    using Pair = std::pair<std::string, std::string>;
    std::map<Pair, std::pair<unsigned, unsigned>> side_counts;
    for (const auto& match : matches) {
        const auto ordered = std::minmax(match.black.value, match.white.value);
        auto& counts = side_counts[{ordered.first, ordered.second}];
        if (match.black.value == ordered.first)
            ++counts.first;
        else
            ++counts.second;
    }
    expect(side_counts.size() == 3, "schedule should contain every unique participant pair");
    for (const auto& [pair, counts] : side_counts) {
        (void)pair;
        expect(counts.first == 1 && counts.second == 1, "every pair should play once on each side");
    }
}

} // namespace

// {
//   責務: [main: 総当たり、先後バランス、random mode再現性、match config、入力検証を確認する]
//   戻り値: [全検証成功時0]
// }
int main() {
    using kadoka::league::ScheduleMode;
    using kadoka::league::SchedulerOptions;

    const auto registry = make_registry({"alpha", "beta", "gamma"});
    const std::uint64_t round_robin_seed = 17;
    const auto round_robin = kadoka::league::create_schedule(registry, {ScheduleMode::round_robin, round_robin_seed});
    expect(round_robin.size() == 6, "round robin should create two games for each of three pairs");
    std::mt19937_64 expected_seed_generator(round_robin_seed);

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
        expect(match.black_time_control_config_hash ==
                   registry.participants().at(match.black.value).time_control_config_hash,
               "black time control should match the black participant");
        expect(match.white_time_control_config_hash ==
                   registry.participants().at(match.white.value).time_control_config_hash,
               "white time control should match the white participant");
        expect(match.seed == expected_seed_generator(), "round robin should assign deterministic per-match seeds");
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
    expect_pairwise_color_balance(round_robin);

    const SchedulerOptions random_options{ScheduleMode::random, 9127};
    const auto first_random = kadoka::league::create_schedule(registry, random_options);
    const auto second_random = kadoka::league::create_schedule(registry, random_options);
    expect(first_random.size() == round_robin.size(), "random mode should preserve full pair coverage");
    expect_pairwise_color_balance(first_random);
    using ParticipantPair = std::pair<kadoka::best::ParticipantId, kadoka::best::ParticipantId>;
    std::vector<ParticipantPair> expected_random_pairs;
    const auto& participants = registry.participants();
    for (auto first = participants.begin(); first != participants.end(); ++first) {
        for (auto second = std::next(first); second != participants.end(); ++second) {
            expected_random_pairs.emplace_back(first->second.id, second->second.id);
            expected_random_pairs.emplace_back(second->second.id, first->second.id);
        }
    }
    std::mt19937_64 expected_random_generator(random_options.seed);
    for (std::size_t remaining = expected_random_pairs.size(); remaining > 1; --remaining) {
        const auto upper_bound = static_cast<std::uint64_t>(remaining);
        const std::uint64_t threshold = (std::uint64_t{0} - upper_bound) % upper_bound;
        std::uint64_t value = 0;
        do {
            value = expected_random_generator();
        } while (value < threshold);
        const auto selected = static_cast<std::size_t>(value % upper_bound);
        std::swap(expected_random_pairs[remaining - 1], expected_random_pairs[selected]);
    }
    bool random_changes_order = false;
    for (std::size_t index = 0; index < first_random.size(); ++index) {
        expect(same_match(first_random[index], second_random[index]), "fixed seed should reproduce the full schedule");
        expect(first_random[index].black == expected_random_pairs[index].first &&
                   first_random[index].white == expected_random_pairs[index].second,
               "fixed seed should produce the expected shuffled pair and side order");
        expect(first_random[index].seed == expected_random_generator(),
               "random mode should assign expected per-match seeds after shuffling");
        random_changes_order = random_changes_order || first_random[index].black != round_robin[index].black ||
                               first_random[index].white != round_robin[index].white;
    }
    expect(random_changes_order, "random mode should shuffle the schedule for the selected seed");

    const auto reordered_registry = make_registry({"gamma", "beta", "alpha"});
    const auto reordered_random = kadoka::league::create_schedule(reordered_registry, random_options);
    for (std::size_t index = 0; index < first_random.size(); ++index)
        expect(same_match(first_random[index], reordered_random[index]),
               "participant registration order should not change a schedule");

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

    bool empty_rejected = false;
    try {
        const kadoka::best::ParticipantRegistry empty;
        (void)kadoka::league::create_schedule(empty, {ScheduleMode::round_robin, 1});
    } catch (const std::invalid_argument&) {
        empty_rejected = true;
    }
    expect(empty_rejected, "scheduling zero participants should be rejected");

    const auto two_registry = make_registry({"alpha", "beta"});
    const auto two_matches = kadoka::league::create_schedule(two_registry, {ScheduleMode::round_robin, 1});
    expect(two_matches.size() == 2, "two participants should produce two matches");
    const auto first_participant = two_registry.participants().begin();
    const auto second_participant = std::next(first_participant);
    expect((two_matches[0].black == first_participant->second.id &&
            two_matches[0].white == second_participant->second.id) ||
               (two_matches[0].black == second_participant->second.id &&
                two_matches[0].white == first_participant->second.id),
           "the first match should include both participants");
    expect(two_matches[1].black == two_matches[0].white && two_matches[1].white == two_matches[0].black,
           "the second match should swap sides");

    bool unsupported_mode_rejected = false;
    try {
        (void)kadoka::league::create_schedule(registry, {static_cast<ScheduleMode>(99), 1});
    } catch (const std::invalid_argument&) {
        unsupported_mode_rejected = true;
    }
    expect(unsupported_mode_rejected, "unsupported schedule mode should be rejected");

    std::vector<std::string> many_families;
    for (unsigned index = 0; index < 33; ++index)
        many_families.push_back("player" + std::to_string(index));
    const std::locale original_locale;
    std::locale::global(std::locale(original_locale, new GroupedNumberPunctuation));
    const auto locale_schedule =
        kadoka::league::create_schedule(make_registry(many_families), {ScheduleMode::round_robin, round_robin_seed});
    std::locale::global(original_locale);
    expect(locale_schedule[999].match_id == "match-00001000", "match IDs should ignore global digit grouping");
}
