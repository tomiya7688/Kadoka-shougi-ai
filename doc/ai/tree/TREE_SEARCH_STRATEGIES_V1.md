# Tree Search Strategies v1

`TreeSearchStrategy` scores the outgoing edge candidates for one parent node
and selects the highest scoring edge. Equal scores are resolved by the
lexicographically smaller edge ID. An empty candidate list returns no edge.

## Value convention

Strategies read each edge's `mean_value` from the perspective of the player
choosing at the parent node. This convention is required for the UCT/PUCT score
to compare candidates. Pure Tree remains generic; callers are responsible for
converting values to this perspective before recording or scoring them.

`PureTree::record_node_visit` and `record_edge_visit` increment visits and the
selected win/draw/loss counter, then update mean value using the new sample.
Each call receives a value and outcome from that record's own perspective.
Path backpropagation and alternating perspective conversion remain caller work.

## UCT reference

For a visited edge, the score is:

`Q + C * sqrt(log(parent_visits + 1) / edge_visits)`

where `Q` is the edge mean value from the parent player's perspective and `C`
is a finite non-negative exploration coefficient. Unvisited edges receive
positive infinity so they are explored before visited candidates.

## PUCT prior hook

`PuctStrategy` accepts a callback that returns a non-negative finite prior for
each candidate edge. The callback receives the parent node and edge. Priors are
normalized across the supplied legal candidates; the set must have a positive
finite sum.

For candidate prior `P`, the score is:

`Q + C * P * sqrt(parent_visits) / (1 + edge_visits)`

The strategy does not train or persist priors. Policy/value learning and
automatic tuning are outside this v1 boundary.

## Statistics updates

Visit updates reject non-finite or out-of-range values, invalid outcomes, and
counter overflow. Mean values remain in `[-1, 1]`; callers choose a consistent
perspective for every update. A path-level backup helper is not included.
