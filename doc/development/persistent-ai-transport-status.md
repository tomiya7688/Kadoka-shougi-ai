# Persistent AI Transport Status

As of 2026-09-16, Kadoka Shougi AI supports persistent `external_process` and `script` Runtime backends through `PersistentProcessAIBackend`.

Verified in CI:

- Linux build: success
- Linux CTest including `process_ai_backend_test`: success
- Linux CLI smoke: success
- Windows build: success
- Windows CTest including `process_ai_backend_test`: success
- Windows CLI smoke: success
- Windows developer artifact upload: success

The transport starts one child process per backend instance, exchanges SFEN/SearchLimits requests over stdin/stdout, and reuses the same child for subsequent decisions.

The authoritative Position remains owned by `TurnRunner`; illegal external moves are rejected without mutating the input Position.

See `doc/ai-runtime-backend.md` for the protocol and runtime contract.
