#include "kadoka/best/participant_registry.hpp"

#include <stdexcept>
#include <string>

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
//   責務: [make_participant: 再現可能なIDを設定した外部AI個体を作る]
//   戻り値: [登録可能な外部AI個体]
// }
kadoka::best::Participant make_participant() {
    kadoka::best::Participant participant;
    participant.kind = kadoka::best::ParticipantKind::external;
    participant.family = "usi-engine";
    participant.layer = kadoka::best::LayerType::full;
    participant.model_id = "external.ginkgo";
    participant.model_version = "2026.10";
    participant.search_config_hash = kadoka::best::compute_config_hash({{"depth", "8"}});
    participant.orchestrator_config_hash = kadoka::best::compute_config_hash({{"mode", "usi"}});
    participant.time_control_config_hash = kadoka::best::compute_config_hash({{"byoyomi_ms", "1000"}});
    participant.id = kadoka::best::participant_id(participant);
    return participant;
}

} // namespace

// {
//   責務: [main: ID再現性、変更分離、Registry登録、重複拒否、保存復元を確認する]
//   戻り値: [全検証成功時0]
// }
int main() {
    using kadoka::best::ParticipantKind;
    using kadoka::best::ParticipantRegistry;

    const auto participant = make_participant();
    expect(participant.id == kadoka::best::participant_id(participant), "same settings should produce the same ID");

    auto changed = participant;
    changed.time_control_config_hash = kadoka::best::compute_config_hash({{"byoyomi_ms", "2000"}});
    expect(kadoka::best::participant_id(changed) != participant.id, "a setting change should produce another ID");

    ParticipantRegistry registry;
    registry.register_participant(participant);
    expect(registry.find(participant.id) != nullptr, "registered participant should be found");
    expect(registry.find(participant.id)->kind == ParticipantKind::external, "external kind should be retained");

    auto classical = participant;
    classical.kind = ParticipantKind::classical;
    classical.family = "classical-baseline";
    classical.id = kadoka::best::participant_id(classical);
    registry.register_participant(classical);
    auto experimental = participant;
    experimental.kind = ParticipantKind::experimental;
    experimental.family = "experimental-search";
    experimental.id = kadoka::best::participant_id(experimental);
    registry.register_participant(experimental);
    expect(registry.participants().size() == 3, "all participant kinds should share one registry");

    bool duplicate_rejected = false;
    try {
        registry.register_participant(participant);
    } catch (const std::invalid_argument&) {
        duplicate_rejected = true;
    }
    expect(duplicate_rejected, "duplicate ID should be rejected");

    const std::string serialized = kadoka::best::serialize_registry(registry);
    const auto restored = kadoka::best::deserialize_registry(serialized);
    expect(kadoka::best::serialize_registry(restored) == serialized, "registry serialization should be stable");
    expect(restored.find(participant.id) != nullptr, "restored registry should contain the participant");
    expect(restored.participants().size() == 3, "all participant kinds should survive serialization");

    auto bad_id = participant;
    bad_id.id.value = "sha256:0000000000000000000000000000000000000000000000000000000000000000";
    bool mismatch_rejected = false;
    try {
        ParticipantRegistry invalid_registry;
        invalid_registry.register_participant(bad_id);
    } catch (const std::invalid_argument&) {
        mismatch_rejected = true;
    }
    expect(mismatch_rejected, "ID inconsistent with settings should be rejected");

    bool malformed_rejected = false;
    try {
        (void)kadoka::best::deserialize_registry("kadoka.participant_registry.v1\ninvalid\n");
    } catch (const std::runtime_error&) {
        malformed_rejected = true;
    }
    expect(malformed_rejected, "malformed participant row should be rejected");
}
