from __future__ import annotations

import sys

ROUTES = {
    "core-state": ("engine/src/position.cpp + engine/include", "tests/core_smoke_test.cpp", "doc/architecture/ARCHITECTURE.md"),
    "move-generation": ("engine/src/{movegen,legal,attack}.cpp", "tests/{movegen,legal_move}_test.cpp", "doc/specifications/{MOVE_GENERATION,LEGAL_MOVE_GENERATION}.md"),
    "runtime": ("runtime/", "tests/runtime_turn_runner_test.cpp", "doc/specifications/ENGINE_RUNTIME.md"),
    "protocol": ("protocol/", "matching protocol tests", "doc/architecture/ARCHITECTURE.md"),
    "engines": ("engines/", "fixed-position engine tests/benchmarks", "engine/runtime contract"),
    "tooling": ("tools/", "command-specific smoke", "doc/architecture/ARCHITECTURE.md"),
    "build-policy": ("CMakeLists.txt + build.bat + .github/workflows + tools/kadoka_rule_checker", "rule checker + CTest", "doc/architecture/CONTEXT_ROUTING.md"),
}


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] in {"-h", "--help"}:
        print("usage: python tools/context_route.py <route|--list>")
        return 0
    if sys.argv[1] == "--list":
        for name in ROUTES:
            print(name)
        return 0
    route = ROUTES.get(sys.argv[1])
    if route is None:
        print(f"unknown route: {sys.argv[1]}")
        return 2
    source, tests, docs = route
    print(f"route={sys.argv[1]}")
    print(f"source={source}")
    print(f"tests={tests}")
    print(f"docs={docs}")
    print("stop=when target contract and acceptance evidence are clear")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
