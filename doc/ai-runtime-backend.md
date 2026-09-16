# AI Runtime Backend Boundary

## Purpose

All AI implementations enter the match Runtime through one proposal boundary. The Runtime owns legality validation and state transition; AI implementations never mutate the authoritative `Position`.

```text
Position + SearchLimits
        |
     AIBackend
        |
   SearchResult
        |
    TurnRunner
        |
legal move validation
        |
 next Position / rejection
```

## Canonical backend kinds

`AIBackendKind` uses the Kadoka AI sibling-project vocabulary:

- `native`
- `dynamic_library`
- `external_process`
- `script`
- `network`

These names describe execution/transport, not game logic or model architecture.

## Native backend

Existing `Engine` implementations remain the native engine contract. `NativeEngineBackend` adapts them to `AIBackend` without moving engine/search responsibilities into Runtime.

```text
Engine::search()
      |
NativeEngineBackend
      |
 run_ai_turn()
```

Native search/evaluation does not pass through serialization, process I/O or script machinery.

## Persistent external process / script backend

`PersistentProcessAIBackend` implements both `external_process` and `script` backend kinds.

The child process is started once when the backend is created and is reused for all following decisions:

```text
create backend
  -> start child process once
  -> request 1 / result 1
  -> request 2 / result 2
  -> ...
  -> destroy backend / close stdin
```

No per-move process spawn or temporary request/response file is used.

External executable example:

```cpp
PersistentProcessBackendConfig config;
config.kind = AIBackendKind::ExternalProcess;
config.executable = "my_shogi_ai.exe";
PersistentProcessAIBackend backend(config);
```

Script example:

```cpp
PersistentProcessBackendConfig config;
config.kind = AIBackendKind::Script;
config.executable = "python";
config.arguments = {"my_shogi_ai.py"};
PersistentProcessAIBackend backend(config);
```

Runtime appends `--kadoka-session` to the child command line.

## Persistent wire protocol

The position is transferred as SFEN. The transport does not invent a second board representation.

Request:

```text
request 1
sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1
time_ms 1000
nodes -1
depth 6
end
```

`-1` means the corresponding optional search limit is not set.

Response for a normal move:

```text
result 1
move normal 7 7 7 6 0
score_cp 25
nodes 12345
depth 6
info example search info
end
```

The last value of `move normal` is promotion: `0` or `1`.

Response for a drop:

```text
result 2
move drop P 5 5
score_cp 10
nodes 2500
depth 4
end
```

Drop piece codes are:

```text
P L N S G B R
```

for pawn, lance, knight, silver, gold, bishop and rook.

Required response records are:

- matching `result <request_id>`;
- exactly one `move ...`;
- terminating `end`.

`score_cp`, `nodes`, `depth` and `info` are optional. Unknown or malformed records are rejected rather than silently ignored.

## Failure behavior

The Runtime reports an error when:

- the child process cannot start;
- the process closes stdin/stdout unexpectedly;
- the process exits before a response completes;
- a response exceeds the configured timeout;
- the response request ID is wrong;
- a move or metadata record is malformed.

Closing/destroying a backend closes the child's stdin. A cooperative child exits on EOF; Runtime terminates a child that does not stop promptly.

Each backend owns its child process. Headless/dataset workers should therefore own separate backend instances instead of sharing one global process behind a lock.

## Transport adapters

`FunctionAIBackend` remains a small integration adapter for dynamic-library/network or other transports implemented elsewhere. Any transport that returns a `SearchResult` can enter the same `run_ai_turn()` path.

Transport code must not be placed in the Core. Persistent process/session management, script runtimes, DLL loading and network clients belong to Runtime/adapter modules.

## Authority rule

A backend result is always non-authoritative.

- Runner generates the authoritative legal-move list.
- Backend proposes one move.
- Runner compares the proposal against legal moves.
- Illegal output does not mutate the current Position.
- If no legal move exists, the backend is not called.

This rule applies equally to native, process and script implementations.

## Performance rule

Do not force slow transport onto native models. The common boundary costs one Runtime-level dispatch per decision; search/evaluation hot loops stay inside the backend.

Preferred execution order when the same model can support several runtimes is roughly:

```text
native / in-process
-> dynamic_library
-> persistent external_process / script
-> one-shot compatibility transport, if ever required
```

Process serialization is an external-backend cost and must not leak into native search hot paths.

## Tests

`process_ai_backend_test` validates:

- an external executable is reused for multiple requests;
- a real Python script uses the same Runner boundary;
- illegal output leaves the canonical Position unchanged;
- malformed output is rejected;
- process exit is detected;
- timeout is detected;
- no-legal-move positions do not invoke the backend decision path.

## Sibling-project mapping

- Othello: `IAIEngine` + persistent package transport -> authoritative `Game::play`
- Shogi: `AIBackend` -> `TurnRunner` -> authoritative `Position::after_move`
- Tetris: observation -> AI adapter/backend -> semantic command -> authoritative game core

The games do not share move/state binary types. They share the boundary semantics, persistent-transport policy and backend vocabulary.
