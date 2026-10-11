#include "kadoka/best/participant_registry.hpp"

#include <stdexcept>
#include <string>
#include <string_view>

namespace kadoka::best {
namespace {

// {
//   責務: [is_token: 安全な単一フィールド用ASCII tokenか検査する]
//   引数: [value: 検査する文字列]
//   戻り値: [英数字で始まり英数字・dot・underscore・hyphenだけならtrue]
// }
bool is_token(std::string_view value) {
    if (value.empty())
        return false;
    const auto is_alphanumeric = [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9');
    };
    if (!is_alphanumeric(value.front()))
        return false;
    for (const char character : value) {
        if (!is_alphanumeric(character) && character != '.' && character != '_' && character != '-')
            return false;
    }
    return true;
}

// {
//   責務: [is_config_hash: v1 config hashの固定表記を検証する]
//   引数: [value: 検査するhash]
//   戻り値: [sha256:と小文字hex 64桁の形式ならtrue]
// }
bool is_config_hash(std::string_view value) {
    if (value.size() != 71 || value.substr(0, 7) != "sha256:")
        return false;
    for (const char character : value.substr(7)) {
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
            return false;
    }
    return true;
}

// {
//   責務: [parse_kind: 保存形式の種別名をParticipantKindへ変換する]
//   引数: [value: 保存形式の種別名]
//   戻り値: [復元した種別]
// }
ParticipantKind parse_kind(std::string_view value) {
    if (value == "classical")
        return ParticipantKind::classical;
    if (value == "external")
        return ParticipantKind::external;
    if (value == "experimental")
        return ParticipantKind::experimental;
    throw std::runtime_error("unknown participant kind");
}

// {
//   責務: [parse_layer: 保存形式の層名をLayerTypeへ変換する]
//   引数: [value: 保存形式の層名]
//   戻り値: [復元した層]
// }
LayerType parse_layer(std::string_view value) {
    if (value == "raw")
        return LayerType::raw;
    if (value == "integrated")
        return LayerType::integrated;
    if (value == "full")
        return LayerType::full;
    throw std::runtime_error("unknown participant layer");
}

// {
//   責務: [split_fields: 1行をtab区切りのfieldへ分割する]
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
        fields.push_back(line.substr(start, separator - start));
        start = separator + 1;
    }
}

// {
//   責務: [append_field: tokenとtabで区切られたfieldを1行へ追加する]
//   引数: [output: 書き込み先] [value: 追加するfield]
// }
void append_field(std::string& output, std::string_view value) {
    output.append(value);
    output.push_back('\t');
}

} // namespace

// {
//   責務: [participant_kind_name: ParticipantKindを保存形式の名前へ変換する]
//   引数: [kind: 変換する個体種別]
//   戻り値: [種別名]
// }
std::string_view participant_kind_name(ParticipantKind kind) noexcept {
    switch (kind) {
    case ParticipantKind::classical:
        return "classical";
    case ParticipantKind::external:
        return "external";
    case ParticipantKind::experimental:
        return "experimental";
    }
    return "invalid";
}

// {
//   責務: [validate_participant: 登録可能な個体情報と設定hashを検査する]
//   引数: [participant: 検査する個体]
//   戻り値: [問題がなければ空、それ以外は人間向けエラー一覧]
// }
std::vector<std::string> validate_participant(const Participant& participant) {
    std::vector<std::string> errors;
    if (participant_kind_name(participant.kind) == "invalid")
        errors.emplace_back("participant kind is invalid");
    if (layer_type_name(participant.layer) == "invalid")
        errors.emplace_back("participant layer is invalid");
    if (!is_token(participant.family))
        errors.emplace_back("family must be a non-empty ASCII token");
    if (!is_token(participant.model_id))
        errors.emplace_back("model_id must be a non-empty ASCII token");
    if (!is_token(participant.model_version))
        errors.emplace_back("model_version must be a non-empty ASCII token");
    if (!is_config_hash(participant.search_config_hash))
        errors.emplace_back("search_config_hash must be a lowercase SHA-256 config hash");
    if (!is_config_hash(participant.orchestrator_config_hash))
        errors.emplace_back("orchestrator_config_hash must be a lowercase SHA-256 config hash");
    if (!is_config_hash(participant.time_control_config_hash))
        errors.emplace_back("time_control_config_hash must be a lowercase SHA-256 config hash");
    return errors;
}

// {
//   責務: [participant_id: 個体の再現に必要な全設定から安定したSHA-256 IDを作る]
//   引数: [participant: IDを作る個体]
//   戻り値: [sha256:接頭辞を含む再現可能な識別子]
// }
ParticipantId participant_id(const Participant& participant) {
    const auto errors = validate_participant(participant);
    if (!errors.empty())
        throw std::invalid_argument("invalid participant: " + errors.front());
    const std::map<std::string, std::string> identity = {
        {"family", participant.family},
        {"kind", std::string(participant_kind_name(participant.kind))},
        {"layer", std::string(layer_type_name(participant.layer))},
        {"model_id", participant.model_id},
        {"model_version", participant.model_version},
        {"orchestrator_config_hash", participant.orchestrator_config_hash},
        {"search_config_hash", participant.search_config_hash},
        {"time_control_config_hash", participant.time_control_config_hash},
    };
    return {compute_config_hash(identity)};
}

// {
//   責務: [register_participant: IDと内容を検証して個体をRegistryへ追加する]
//   引数: [participant: 登録する個体]
// }
void ParticipantRegistry::register_participant(Participant participant) {
    const auto errors = validate_participant(participant);
    if (!errors.empty())
        throw std::invalid_argument("invalid participant: " + errors.front());
    const ParticipantId expected = participant_id(participant);
    if (participant.id.value != expected.value)
        throw std::invalid_argument("participant ID does not match its reproducible identity");
    const std::string id = participant.id.value;
    const auto [entry, inserted] = participants_.emplace(id, std::move(participant));
    (void)entry;
    if (!inserted)
        throw std::invalid_argument("duplicate participant ID");
}

// {
//   責務: [find: IDから登録済み個体を検索する]
//   引数: [id: 検索する識別子]
//   戻り値: [見つかった個体、または未登録ならnullptr]
// }
const Participant* ParticipantRegistry::find(const ParticipantId& id) const noexcept {
    const auto found = participants_.find(id.value);
    return found == participants_.end() ? nullptr : &found->second;
}

// {
//   責務: [participants: 登録内容を安定したID順で参照する]
//   戻り値: [IDをkeyにした登録済み個体map]
// }
const std::map<std::string, Participant>& ParticipantRegistry::participants() const noexcept {
    return participants_;
}

// {
//   責務: [serialize_registry: Registryを再現可能なversion付きテキストへ変換する]
//   引数: [registry: 保存するRegistry]
//   戻り値: [UTF-8のkadoka.participant_registry.v1文書]
// }
std::string serialize_registry(const ParticipantRegistry& registry) {
    std::string output = "kadoka.participant_registry.v1\n";
    for (const auto& [id, participant] : registry.participants()) {
        append_field(output, id);
        append_field(output, participant_kind_name(participant.kind));
        append_field(output, participant.family);
        append_field(output, layer_type_name(participant.layer));
        append_field(output, participant.model_id);
        append_field(output, participant.model_version);
        append_field(output, participant.search_config_hash);
        append_field(output, participant.orchestrator_config_hash);
        output.append(participant.time_control_config_hash);
        output.push_back('\n');
    }
    return output;
}

// {
//   責務: [deserialize_registry: version付き文書を復元しIDと重複を検証する]
//   引数: [serialized: 読み込む文書]
//   戻り値: [復元したRegistry]
// }
ParticipantRegistry deserialize_registry(std::string_view serialized) {
    constexpr std::string_view header = "kadoka.participant_registry.v1\n";
    if (!serialized.starts_with(header))
        throw std::runtime_error("unsupported participant registry format");
    ParticipantRegistry result;
    std::string_view remaining = serialized.substr(header.size());
    while (!remaining.empty()) {
        const std::size_t newline = remaining.find('\n');
        if (newline == std::string_view::npos)
            throw std::runtime_error("participant record must end with a newline");
        const std::string_view line = remaining.substr(0, newline);
        remaining.remove_prefix(newline + 1);
        if (line.empty())
            throw std::runtime_error("participant record must not be empty");
        const auto fields = split_fields(line);
        if (fields.size() != 9)
            throw std::runtime_error("participant record must contain exactly nine fields");
        Participant participant;
        participant.id = {std::string(fields[0])};
        participant.kind = parse_kind(fields[1]);
        participant.family = fields[2];
        participant.layer = parse_layer(fields[3]);
        participant.model_id = fields[4];
        participant.model_version = fields[5];
        participant.search_config_hash = fields[6];
        participant.orchestrator_config_hash = fields[7];
        participant.time_control_config_hash = fields[8];
        result.register_participant(std::move(participant));
    }
    return result;
}

} // namespace kadoka::best

