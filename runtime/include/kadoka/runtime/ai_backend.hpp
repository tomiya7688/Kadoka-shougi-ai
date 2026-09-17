#pragma once

#include "kadoka/engine.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace kadoka::shogi::runtime {

enum class AIBackendKind : std::uint8_t {
    Native,
    DynamicLibrary,
    ExternalProcess,
    Script,
    Network,
};

// Runtime-facing AI boundary. Backends only propose a SearchResult; they never
// mutate the authoritative Position. TurnRunner validates the proposed move.
class AIBackend {
public:
    virtual ~AIBackend() = default;

    [[nodiscard]] virtual AIBackendKind kind() const noexcept = 0;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual SearchResult decide(
        const Position& position,
        const SearchLimits& limits
    ) = 0;

    [[nodiscard]] virtual MutualImpasseResponse respond_to_mutual_impasse_offer(
        const Position&
    ) {
        return MutualImpasseResponse::Decline;
    }
};

// Fast adapter for existing in-process Engine implementations.
class NativeEngineBackend final : public AIBackend {
public:
    explicit NativeEngineBackend(Engine& engine) noexcept : engine_(&engine) {}

    [[nodiscard]] AIBackendKind kind() const noexcept override {
        return AIBackendKind::Native;
    }

    [[nodiscard]] std::string name() const override;

    [[nodiscard]] SearchResult decide(
        const Position& position,
        const SearchLimits& limits
    ) override;

    [[nodiscard]] MutualImpasseResponse respond_to_mutual_impasse_offer(
        const Position& position
    ) override;

private:
    Engine* engine_;
};

// Small adapter used by runtime integrations whose transport is implemented
// elsewhere (process/script/dynamic-library/network). The transport supplies a
// callable and TurnRunner keeps authority over legality/state transition.
class FunctionAIBackend final : public AIBackend {
public:
    using DecideFunction = std::function<SearchResult(const Position&, const SearchLimits&)>;
    using MutualImpasseResponseFunction =
        std::function<MutualImpasseResponse(const Position&)>;

    FunctionAIBackend(
        AIBackendKind kind,
        std::string name,
        DecideFunction decide,
        MutualImpasseResponseFunction mutual_impasse_response = {}
    );

    [[nodiscard]] AIBackendKind kind() const noexcept override { return kind_; }
    [[nodiscard]] std::string name() const override { return name_; }

    [[nodiscard]] SearchResult decide(
        const Position& position,
        const SearchLimits& limits
    ) override;

    [[nodiscard]] MutualImpasseResponse respond_to_mutual_impasse_offer(
        const Position& position
    ) override;

private:
    AIBackendKind kind_;
    std::string name_;
    DecideFunction decide_;
    MutualImpasseResponseFunction mutual_impasse_response_;
};

[[nodiscard]] const char* ai_backend_kind_name(AIBackendKind kind) noexcept;

} // namespace kadoka::shogi::runtime
