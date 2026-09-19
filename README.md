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

The current core includes board/hand representation, SFEN parsing and serialization, pseudo-legal/legal move work, and a small validated runtime turn runner. Search, learning engines, repetition/perpetual-check completion and USI remain active milestones.

## Design principles

1. **Standalone shogi core first.** The core must function as a normal shogi application with no AI installed: rules, game progression, adjudication, records, and human-facing play do not depend on AI packages.
2. **Correctness before strength.** Search and learning are useless if the game core can produce illegal states.
3. **Players are interchangeable.** Human/native/external/script players use the same game-facing boundary. The game sends ordinary observable state and accepts actions; legal-move lists are not a required AI input.
4. **Time controls are first-class.** Engines should be able to return the best result they currently have when a time limit is used.
5. **Explainability is useful.** Evaluation-based engines should be able to expose why a position received its score.
6. **Experiments stay reproducible.** Seeds, engine settings, model versions, and game records should be preservable.
7. **Obake are allowed to be silly, not corrupt the rules.** Character behavior belongs above the authoritative legal-move layer.
8. **Implementation tasks should be local.** The repository should make it easy for Codex or a human contributor to identify the target subsystem, acceptance tests, and out-of-scope behavior.

## Build

On Windows:

```bat
build.bat
```

This runs the architecture checker, configures CMake, builds Release and runs CTest.

CI mirrors the sibling-project pattern:

- Linux: CMake build + CTest + CLI smoke
- Windows: `build.bat` + CLI smoke + developer executable artifact

The uploaded executable is a developer build artifact, not yet a formally defined portable distribution package.

## AI-assisted development

Start from `AI_CONTEXT.md` and route the task before opening broad documentation.

```text
python tools/context_route.py --list
python tools/context_route.py move-generation
python tools/kadoka_rule_checker/script/kadoka_rule_checker.py .
```

The repository adopts context routing and compact dependency checks from Kadoka Othello AI while retaining shogi-specific correctness boundaries.

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
- [Context routing](doc/architecture/CONTEXT_ROUTING.md)
- [Sibling project alignment](doc/architecture/SIBLING_PROJECT_ALIGNMENT.md)
- [Engine runtime](doc/specifications/ENGINE_RUNTIME.md)
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
