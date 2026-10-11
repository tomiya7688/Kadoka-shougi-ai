#include "kadoka/league/rating_store.hpp"

#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
//   責務: [make_registry: 3参加者を持つRatingテスト用Registryを作る]
//   戻り値: [有効な参加者3名のRegistry]
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

} // namespace

// {
//   責務: [main: Glicko結果更新、不確実性、履歴、再生復元、入力拒否を検証する]
//   戻り値: [全検証成功時0]
// }
int main() {
    const auto registry = make_registry();
    const auto first = registry.participants().begin()->second.id;
    const auto second = std::next(registry.participants().begin())->second.id;
    const auto third = std::next(std::next(registry.participants().begin()))->second.id;
    kadoka::league::RatingStore store(registry);

    const auto* initial_first = store.find(first);
    const auto* initial_second = store.find(second);
    expect(initial_first != nullptr && initial_second != nullptr,
           "Registry participants should start in the RatingStore");
    expect(initial_first->rating == 1500.0 && initial_first->uncertainty == 350.0 && initial_first->games == 0,
           "new participants should use documented Glicko defaults");

    store.record_result(first, second, kadoka::league::GameResult::black_win);
    const auto* after_first_win = store.find(first);
    const auto* after_second_loss = store.find(second);
    expect(after_first_win->rating > 1500.0, "winner rating should increase");
    expect(after_second_loss->rating < 1500.0, "loser rating should decrease");
    expect(after_first_win->uncertainty < 350.0 && after_second_loss->uncertainty < 350.0,
           "a rated game should reduce both rating deviations");
    expect(after_first_win->games == 1 && after_second_loss->games == 1, "both game counts should advance");
    expect(store.history().size() == 1 && store.history().front().sequence == 1,
           "each result should append a contiguous history entry");

    store.record_result(second, third, kadoka::league::GameResult::draw);
    expect(store.find(second)->games == 2 && store.find(third)->games == 1,
           "later results should update only the participating players' game counts");
    const std::string serialized = kadoka::league::serialize_rating_store(store);
    const auto restored = kadoka::league::deserialize_rating_store(serialized);
    expect(kadoka::league::serialize_rating_store(restored) == serialized, "rating history replay should be stable");
    expect(restored.history().size() == 2, "rating history should survive serialization");
    expect(restored.find(first)->rating == store.find(first)->rating, "replay should reproduce the current rating");
    expect(restored.find(second)->uncertainty == store.find(second)->uncertainty,
           "replay should reproduce rating uncertainty");

    kadoka::league::RatingStore draw_store(registry);
    draw_store.record_result(first, second, kadoka::league::GameResult::draw);
    expect(draw_store.find(first)->rating == 1500.0 && draw_store.find(second)->rating == 1500.0,
           "equal initial ratings should remain equal after a draw");

    bool unknown_rejected = false;
    try {
        draw_store.record_result(first, {"sha256:0000000000000000000000000000000000000000000000000000000000000000"},
                                 kadoka::league::GameResult::black_win);
    } catch (const std::invalid_argument&) {
        unknown_rejected = true;
    }
    expect(unknown_rejected, "unknown participants should be rejected");

    bool self_match_rejected = false;
    try {
        draw_store.record_result(first, first, kadoka::league::GameResult::draw);
    } catch (const std::invalid_argument&) {
        self_match_rejected = true;
    }
    expect(self_match_rejected, "self-play rating updates should be rejected");

    bool bad_result_rejected = false;
    try {
        draw_store.record_result(first, second, static_cast<kadoka::league::GameResult>(99));
    } catch (const std::invalid_argument&) {
        bad_result_rejected = true;
    }
    expect(bad_result_rejected, "unknown result values should be rejected");

    std::string corrupted = serialized;
    const std::size_t result_position = corrupted.find("black_win");
    expect(result_position != std::string::npos, "serialized winner result should be present");
    corrupted.replace(result_position, std::string("black_win").size(), "unknown");
    bool corruption_rejected = false;
    try {
        (void)kadoka::league::deserialize_rating_store(corrupted);
    } catch (const std::runtime_error&) {
        corruption_rejected = true;
    }
    expect(corruption_rejected, "unknown persisted results should be rejected");
}
