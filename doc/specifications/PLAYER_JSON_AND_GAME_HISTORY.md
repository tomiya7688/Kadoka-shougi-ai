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
