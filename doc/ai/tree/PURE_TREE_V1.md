# Pure Tree v1

Pure Tree is a persistent, game-independent record of search nodes and edges.
It stores caller-supplied node and edge IDs, position identity, path identity,
visit/outcome counts, mean value, the root reference, and child connections.
It does not calculate shogi positions or define how identity strings are made.

## Identity and topology

- A node is identified by its `id`; IDs are unique within the node collection.
- `position_identity` and `path_identity` are opaque, non-empty strings. Their
  pair must be unique. The same position with a different path is allowed.
- An edge has a unique ID, a parent node, a child node, and a USI move string.
- Each non-root node has exactly one parent. Every node must be reachable from
  the root, so cycles, disconnected nodes, and shared children are rejected.
- An empty model has no root, nodes, or edges. A non-empty model must name an
  existing root node.

The caller supplies stable IDs. This v1 API does not generate IDs or canonicalize
position/path identities.

## Statistics

Node and edge statistics include visits, wins, losses, draws, and mean value.
Outcome counts cannot exceed visits. `mean_value` must be finite and between
-1 and 1. The API does not define the perspective or scoring convention for
mean value; producers and consumers must use the same convention.

## File format

The UTF-8 text format is line-oriented and versioned. The header is
`KADOKA_PURE_TREE<TAB>1`. It is followed by one `ROOT` record, then `NODE` and
`EDGE` records. Fields are tab-separated. Backslash, tab, newline, carriage
return, and other control bytes in string fields use `\\`, `\t`, `\n`, `\r`,
and `\xHH` escapes. Numeric values use locale-independent text; doubles retain
enough digits for a round trip.

Unknown record types, unsupported versions, invalid counts, duplicate
identities, missing references, malformed escapes, and invalid tree topology
are rejected. The implementation exposes stream and filesystem load/save
helpers. `save_pure_tree` truncates the destination file; atomic replacement is
not part of this format contract.

## Scope

Pure Tree does not implement UCT/PUCT selection, policy/value adjustment,
auto-tuning, or a model registry. Those consumers can use the validated node
and edge records without changing the persisted format.
