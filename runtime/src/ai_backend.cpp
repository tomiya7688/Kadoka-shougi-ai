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

MutualImpasseResponse NativeEngineBackend::respond_to_mutual_impasse_offer(
    const Position& position) {
    return engine_->respond_to_mutual_impasse_offer(position);
}

FunctionAIBackend::FunctionAIBackend(
    AIBackendKind kind,
    std::string name,
    DecideFunction decide,
    MutualImpasseResponseFunction mutual_impasse_response)
    : kind_(kind),
      name_(std::move(name)),
      decide_(std::move(decide)),
      mutual_impasse_response_(std::move(mutual_impasse_response)) {
    if (!decide_) {
        throw std::invalid_argument("FunctionAIBackend requires decide function");
    }
}

SearchResult FunctionAIBackend::decide(
    const Position& position,
    const SearchLimits& limits) {
    return decide_(position, limits);
}

MutualImpasseResponse FunctionAIBackend::respond_to_mutual_impasse_offer(
    const Position& position) {
    if (!mutual_impasse_response_) {
        return MutualImpasseResponse::Decline;
    }
    return mutual_impasse_response_(position);
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
