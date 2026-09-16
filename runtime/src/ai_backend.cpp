#include "kadoka/runtime/ai_backend.hpp"

#include <stdexcept>

namespace kadoka::shogi::runtime {

std::string NativeEngineBackend::name() const {
    return engine_->name();
}

SearchResult NativeEngineBackend::decide(
    const Position& position,
    const SearchLimits& limits) {
    return engine_->search(position, limits);
}

FunctionAIBackend::FunctionAIBackend(
    AIBackendKind kind,
    std::string name,
    DecideFunction decide)
    : kind_(kind), name_(std::move(name)), decide_(std::move(decide)) {
    if (!decide_) {
        throw std::invalid_argument("FunctionAIBackend requires decide function");
    }
}

SearchResult FunctionAIBackend::decide(
    const Position& position,
    const SearchLimits& limits) {
    return decide_(position, limits);
}

const char* ai_backend_kind_name(AIBackendKind kind) noexcept {
    switch (kind) {
        case AIBackendKind::Native: return "native";
        case AIBackendKind::DynamicLibrary: return "dynamic_library";
        case AIBackendKind::ExternalProcess: return "external_process";
        case AIBackendKind::Script: return "script";
        case AIBackendKind::Network: return "network";
    }
    return "unknown";
}

} // namespace kadoka::shogi::runtime
