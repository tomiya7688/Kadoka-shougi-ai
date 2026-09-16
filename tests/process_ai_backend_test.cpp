#include "kadoka/runtime/process_ai_backend.hpp"
#include "kadoka/runtime/turn_runner.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

using namespace kadoka::shogi;
using namespace kadoka::shogi::runtime;

namespace {

PersistentProcessBackendConfig make_external_config(
    const std::string& executable,
    const std::string& mode = {}) {
    PersistentProcessBackendConfig config;
    config.kind = AIBackendKind::ExternalProcess;
    config.name = "external-test";
    config.executable = executable;
    if (!mode.empty()) config.arguments.push_back("--mode=" + mode);
    config.response_timeout = std::chrono::milliseconds(500);
    return config;
}

PersistentProcessBackendConfig make_script_config(
    const std::string& python,
    const std::string& script) {
    PersistentProcessBackendConfig config;
    config.kind = AIBackendKind::Script;
    config.name = "script-test";
    config.executable = python;
    config.arguments.push_back(script);
    config.response_timeout = std::chrono::milliseconds(500);
    return config;
}

} // namespace

int main(int argc, char** argv) {
    assert(argc >= 4);
    const std::string helper = argv[1];
    const std::string python = argv[2];
    const std::string script = argv[3];

    {
        const Position position = Position::startpos();
        PersistentProcessAIBackend backend(make_external_config(helper));
        assert(backend.kind() == AIBackendKind::ExternalProcess);

        const TurnResult first = run_ai_turn(backend, position);
        const TurnResult second = run_ai_turn(backend, position);
        assert(first.status == TurnStatus::MoveApplied);
        assert(second.status == TurnStatus::MoveApplied);
        assert(first.search_result.has_value());
        assert(second.search_result.has_value());
        assert(first.search_result->info == "request_count=1");
        assert(second.search_result->info == "request_count=2");
        assert(first.search_result->score_cp == 12);
        assert(first.search_result->nodes == 34);
        assert(first.search_result->depth == 2);
    }

    {
        const Position position = Position::startpos();
        PersistentProcessAIBackend backend(make_script_config(python, script));
        assert(backend.kind() == AIBackendKind::Script);

        SearchLimits limits;
        limits.time_limit = std::chrono::milliseconds(250);
        limits.node_limit = 1000;
        limits.depth_limit = 4;
        const TurnResult result = run_ai_turn(backend, position, limits);
        assert(result.status == TurnStatus::MoveApplied);
        assert(result.search_result.has_value());
        assert(result.search_result->info == "script_request_count=1");
        assert(result.search_result->score_cp == 21);
        assert(result.search_result->nodes == 55);
        assert(result.search_result->depth == 3);
    }

    {
        const Position position = Position::startpos();
        const std::string before = position.to_sfen();
        PersistentProcessAIBackend backend(make_external_config(helper, "illegal"));

        const TurnResult result = run_ai_turn(backend, position);
        assert(result.status == TurnStatus::IllegalMove);
        assert(result.search_result.has_value());
        assert(!result.next_position.has_value());
        assert(position.to_sfen() == before);
    }

    {
        const Position position = Position::startpos();
        PersistentProcessAIBackend backend(make_external_config(helper, "malformed"));
        bool failed = false;
        try {
            (void)run_ai_turn(backend, position);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    }

    {
        const Position position = Position::startpos();
        PersistentProcessAIBackend backend(make_external_config(helper, "exit"));
        bool failed = false;
        try {
            (void)run_ai_turn(backend, position);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    }

    {
        const Position position = Position::startpos();
        PersistentProcessBackendConfig config = make_external_config(helper, "timeout");
        config.response_timeout = std::chrono::milliseconds(50);
        PersistentProcessAIBackend backend(std::move(config));
        bool failed = false;
        try {
            (void)run_ai_turn(backend, position);
        } catch (const std::runtime_error&) {
            failed = true;
        }
        assert(failed);
    }

    {
        const Position no_moves = Position::from_sfen("9/9/9/9/9/9/9/9/9 b - 1");
        PersistentProcessAIBackend backend(make_external_config(helper, "exit"));
        const TurnResult result = run_ai_turn(backend, no_moves);
        assert(result.status == TurnStatus::NoLegalMoves);
        assert(!result.search_result.has_value());
        assert(!result.next_position.has_value());
    }

    return 0;
}
