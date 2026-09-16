#pragma once

#include "kadoka/runtime/ai_backend.hpp"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace kadoka::shogi::runtime {

struct PersistentProcessBackendConfig {
    AIBackendKind kind{AIBackendKind::ExternalProcess};
    std::string name{"external-ai"};
    std::string executable;
    std::vector<std::string> arguments;
    std::chrono::milliseconds response_timeout{5000};
};

// Long-lived stdin/stdout transport for external executable and script AIs.
// The child process is created once per backend instance and reused across
// decisions. Core game state remains authoritative in TurnRunner.
class PersistentProcessAIBackend final : public AIBackend {
public:
    explicit PersistentProcessAIBackend(PersistentProcessBackendConfig config);
    ~PersistentProcessAIBackend() override;

    PersistentProcessAIBackend(PersistentProcessAIBackend&&) noexcept;
    PersistentProcessAIBackend& operator=(PersistentProcessAIBackend&&) noexcept;

    PersistentProcessAIBackend(const PersistentProcessAIBackend&) = delete;
    PersistentProcessAIBackend& operator=(const PersistentProcessAIBackend&) = delete;

    [[nodiscard]] AIBackendKind kind() const noexcept override;
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] SearchResult decide(
        const Position& position,
        const SearchLimits& limits
    ) override;

private:
    class Impl;

    PersistentProcessBackendConfig config_;
    std::unique_ptr<Impl> impl_;
    std::size_t next_request_id_{1};
};

} // namespace kadoka::shogi::runtime
