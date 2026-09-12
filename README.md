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

🚧 Core implementation phase.

The current core includes board/hand representation, SFEN parsing and serialization, and pseudo-legal move generation including promotion and basic drop restrictions. Full king-safety legality, make/unmake, hashing, repetition handling, and USI remain upcoming milestones.

## Design principles

1. **Correctness before strength.** Search and learning are useless if the game core can produce illegal states.
2. **Engines are interchangeable.** Every AI should run behind the same match/benchmark interface.
3. **Time controls are first-class.** Engines should be able to return the best result they currently have when a time limit is used.
4. **Explainability is useful.** Evaluation-based engines should be able to expose why a position received its score.
5. **Experiments stay reproducible.** Seeds, engine settings, model versions, and game records should be preservable.
6. **Obake are allowed to be silly, not corrupt the rules.** Character behavior belongs above the authoritative legal-move layer.
7. **Implementation tasks should be local.** The repository should make it easy for Codex or a human contributor to identify the target subsystem, acceptance tests, and out-of-scope behavior.

## Planned layout

```text
engine/       core shogi state and rules
engines/      individual AI implementations
protocol/     USI and other frontends
tools/        training, self-play, conversion, and analysis tools
tests/        correctness and regression tests
doc/          detailed project documentation
  architecture/    architecture and dependency decisions
  specifications/  rule, API, and file-format specifications
  diagrams/        diagrams, tables, and visual design notes
models/       model metadata/pointers; large trained data is not committed here
```

Repository-level information stays in this README. Detailed documentation should normally be added under `doc/`, using a suitable subdirectory rather than placing many unrelated files directly in `doc/`.

## Documentation

- [Architecture](doc/architecture/ARCHITECTURE.md)
- [Move generation specification](doc/specifications/MOVE_GENERATION.md)
- [Diagram area](doc/diagrams/README.md)

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

This repository uses two license layers with a deliberately narrow boundary:

- **MIT License** (`LICENSE`) — applies to the software and technical AI contents, including source code, algorithms, search/evaluation/learning methods, model implementations, trained weights, parameters, datasets, protocols, tools, tests, and technical documentation unless separately noted.
- **Kadoka Shougi AI Character License** (`CHARACTER_LICENSE.md`) — applies only to official AI character **names** and **visual character designs / character artwork**.

For example, the name and visual appearance of an AI such as **賢者メリースライム** are covered by the character license, while the AI engine itself — including its methods, model, learned weights, parameters, and implementation — is MIT-licensed.

A character name appearing inside MIT-licensed source code or model metadata does not change the license of the technical implementation itself.
