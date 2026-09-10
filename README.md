# Kadoka Shougi AI

Kadoka Shougi AI is an experimental shogi AI project focused on building, comparing, and learning from multiple engine styles rather than chasing hardware-at-any-cost strength.

The project will include:

- evaluation-function engines
- evaluation + machine-learning hybrid engines
- very fast candidate-update engines
- Mine Learning variants
- tree-structured learning experiments
- deliberately characterful `obake` engines such as Kadoka and Maru
- reproducible self-play, benchmarks, and analysis tools

## Project status

🚧 Bootstrap phase.

The first milestone is a correct and testable shogi core: board state, legal moves, move application/undo, SFEN/USI notation, repetition handling, and a stable engine interface.

## Design principles

1. **Correctness before strength.** Search and learning are useless if the game core can produce illegal states.
2. **Engines are interchangeable.** Every AI should run behind the same match/benchmark interface.
3. **Time controls are first-class.** Engines should be able to return the best result they currently have when a time limit is used.
4. **Explainability is useful.** Evaluation-based engines should be able to expose why a position received its score.
5. **Experiments stay reproducible.** Seeds, engine settings, model versions, and game records should be preservable.
6. **Obake are allowed to be silly, not corrupt the rules.** Character behavior belongs above the authoritative legal-move layer.

## Planned layout

```text
engine/       core shogi engine and common AI interfaces
engines/      individual AI implementations
protocol/     USI and other frontends
tools/        training, self-play, conversion, and analysis tools
tests/        correctness and regression tests
docs/         architecture and engine notes
models/       model metadata/pointers; large trained data is not committed here
```

## Initial engine families

- **Evaluate** — handcrafted evaluation + search
- **Evaluate Learning** — evaluation + learned components
- **Hyper Fast** — latency-first candidate-update engine
- **Mine Learning** — learning experiments with multiple state/history variants
- **Tree AI** — tree-structured learned data that can grow with training
- **Kadoka Best AI** — practical strongest mixed-method engine under a time budget
- **Super Star** — heavyweight ensemble of proven methods
- **Obake Kadoka / Maru** — intentionally weak, characterful engines for UI and testing

## License

A license has not been selected yet.
