# Player JSON and Game History v1

## Player API JSON

The public player-facing API is JSON. It carries only information an ordinary player can see or know. It is primarily a learning/evaluation/test aid; AI packages must not assume other shogi software provides this API.

### PlayerObservation

```json
{
  "schema": "kadoka.player_observation",
  "version": 1,
  "board": { "sfen": "..." },
  "side_to_move": "black",
  "clock": {
    "black": { "main_ms": 0, "byoyomi_ms": 0, "increment_ms": 0 },
    "white": { "main_ms": 0, "byoyomi_ms": 0, "increment_ms": 0 },
    "per_move_limit_ms": null
  }
}
```

Wire board state uses SFEN. Moves use USI move notation. Time values are integer milliseconds. Unknown optional fields should be ignored for forward compatibility.

### PlayerAction

```json
{
  "schema": "kadoka.player_action",
  "version": 1,
  "type": "move",
  "move": "7g7f"
}
```

or:

```json
{
  "schema": "kadoka.player_action",
  "version": 1,
  "type": "resign"
}
```

### ActionResult

```json
{
  "schema": "kadoka.action_result",
  "version": 1,
  "status": "accepted"
}
```

Illegal move:

```json
{
  "schema": "kadoka.action_result",
  "version": 1,
  "status": "illegal",
  "reason": "illegal_move"
}
```

An illegal move is rejected before application. Canonical board and side-to-move remain unchanged. It does not end the game, and the same player may act again.

The Player API does not send legal move lists, check flags, repetition history, game IDs, search limits, evaluation features, or other AI-internal helpers.

## Game history output

A completed game produces two UTF-8 JSON Lines streams.

### BoardState JSONL

One record per canonical ply, including ply 0.

Required fields:

- `schema = "kadoka.board_state"`
- `version = 1`
- `game_id`
- `ply`
- `sfen`
- `side_to_move`
- visible clock state

### GameAux JSONL

Stores non-board game facts and events.

Required core fields:

- `schema = "kadoka.game_aux"`
- `version = 1`
- `game_id`
- `ply`
- `event_index`
- `event_type`
- `actor`
- `action`
- `result`
- `terminal`

BoardState and GameAux join on `game_id + ply`. `event_index` uniquely orders events when multiple rejected actions happen at the same ply.

`game_id` uses a ULID string. `ply` counts accepted legal moves only. Illegal moves do not increment `ply`; they are preserved only as GameAux rejected-action events.

These JSONL files are canonical easy-to-read source records. Dataset tooling may convert them to compressed, binary, or columnar training formats later.


## C++ Player API v1 implementation

The Runtime implementation is exposed from:

```cpp
#include "kadoka/runtime/player_api.hpp"
```

The public v1 types are:

- `PlayerObservation`
- `PlayerClock` / `PlayerClockSide`
- `PlayerAction`
- `ActionResult`
- `NativeActionApplication`

The wire codec entry points are:

```cpp
serialize_player_observation(...)
deserialize_player_observation(...)
serialize_player_action(...)
deserialize_player_action(...)
serialize_action_result(...)
deserialize_action_result(...)
move_to_usi(...)
move_from_usi(...)
```

`make_player_observation()` converts the canonical `Position` plus visible
clock information into the public observation. The observation repeats
`side_to_move` for convenient consumers, but decoding verifies that it agrees
with the side encoded in SFEN so two contradictory canonical states cannot be
accepted.

`apply_player_action()` is the native/reference bridge for the ordinary public
Player API. It accepts only normal move/resign actions:

- legal move -> `accepted` and a new canonical Position
- illegal move -> `illegal / illegal_move`, no new Position
- resign -> `accepted`, no new Position, `resigned=true`

The input Position is never mutated.

### JSON compatibility

The v1 decoder:

- requires exact `schema`
- requires `version == 1`
- validates required field types
- validates SFEN and USI syntax
- requires clock values to be non-negative integers or `null`
- ignores unknown fields, including nested object/array values
- rejects duplicate JSON object keys to avoid ambiguous payloads

Unknown fields are therefore forward-compatible, while malformed required
fields are rejected.

### Normal Player API vs optional match procedures

The normal public `PlayerAction` schema intentionally contains only:

- `move`
- `resign`

Entering-king declaration, mutually agreed impasse, league metadata, search
limits, legal move lists, and other internal/tournament procedures are not part
of Player API v1. Runtime may support those procedures through separate
internal or compatibility paths without expanding the ordinary player-facing
contract.
