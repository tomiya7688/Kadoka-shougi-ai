#pragma once

#include "kadoka/best/participant_registry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace kadoka::league {

// {
//   責務: [ScheduleMode: 全組合せの順序を固定またはseed付きで無作為化する]
// }
enum class ScheduleMode { round_robin, random };

// {
//   責務: [SchedulerOptions: 対局カード生成方式と再現用seedを保持する]
// }
struct SchedulerOptions {
    ScheduleMode mode = ScheduleMode::round_robin;
    std::uint64_t seed = 0;
};

// {
//   責務: [MatchConfig: 1局の両参加者、先後、個別時間設定、再現用seedを保持する]
// }
struct MatchConfig {
    std::string match_id;
    best::ParticipantId black;
    best::ParticipantId white;
    std::string black_time_control_config_hash;
    std::string white_time_control_config_hash;
    std::uint64_t seed = 0;
};

// {
//   責務: [create_schedule: Registryから先後バランス済みの総当たり対局カードを生成する]
//   引数: [registry: 対局候補の参加者Registry] [options: 方式とseed]
//   戻り値: [各組合せを先後入替えで含む決定論的な対局設定一覧]
// }
[[nodiscard]] std::vector<MatchConfig> create_schedule(const best::ParticipantRegistry& registry,
                                                       const SchedulerOptions& options);

} // namespace kadoka::league
