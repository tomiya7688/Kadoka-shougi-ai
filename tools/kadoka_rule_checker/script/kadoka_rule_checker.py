from __future__ import annotations

import fnmatch
import re
import sys
from dataclasses import dataclass
from pathlib import Path

IGNORE_FILE = ".kadoka-check-ignore"
SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h"}


@dataclass(frozen=True)
class Finding:
    path: str
    line: int
    code: str
    message: str


def load_ignores(root: Path) -> list[tuple[str, str, str]]:
    path = root / IGNORE_FILE
    if not path.exists():
        return []
    rules = []
    for number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split(maxsplit=2)
        if len(parts) != 3:
            print(f"{IGNORE_FILE}:{number} KSH900 ignore requires: CODE GLOB reason")
            raise SystemExit(2)
        rules.append(tuple(parts))
    return rules


def ignored(path: str, code: str, rules: list[tuple[str, str, str]]) -> bool:
    normalized = path.replace("\\", "/")
    return any(rule_code in {code, "*"} and fnmatch.fnmatch(normalized, pattern)
               for rule_code, pattern, _reason in rules)


def scan_tree(root: Path, relative_root: str, forbidden: tuple[str, ...], code: str,
              ignores: list[tuple[str, str, str]]) -> list[Finding]:
    findings: list[Finding] = []
    include_re = re.compile(r'^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]')
    base = root / relative_root
    if not base.exists():
        return findings
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in SUFFIXES:
            continue
        rel = path.relative_to(root).as_posix()
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            match = include_re.match(line)
            if not match:
                continue
            included = match.group(1).replace("\\", "/").lower()
            bad = next((item for item in forbidden if item in included), None)
            if bad and not ignored(rel, code, ignores):
                findings.append(Finding(rel, number, code, f"forbidden dependency in {relative_root}: {included}"))
    return findings


def scan_cmake(root: Path, ignores: list[tuple[str, str, str]]) -> list[Finding]:
    text = (root / "CMakeLists.txt").read_text(encoding="utf-8")
    findings: list[Finding] = []
    match = re.search(r"target_link_libraries\s*\(\s*kadoka_shogi_core\s+(.*?)\)", text, re.DOTALL)
    if match and "kadoka_shogi_runtime" in match.group(1) and not ignored("CMakeLists.txt", "KSH102", ignores):
        line = text[:match.start()].count("\n") + 1
        findings.append(Finding("CMakeLists.txt", line, "KSH102", "core must not link runtime"))
    return findings


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    if not (root / "CMakeLists.txt").exists():
        print("KSH000 CMakeLists.txt not found")
        return 2
    ignores = load_ignores(root)
    findings = []
    findings += scan_tree(root, "engine", ("runtime/", "protocol/", "tools/", "engines/"), "KSH101", ignores)
    findings += scan_tree(root, "runtime", ("protocol/", "tools/", "training/", "analysis/"), "KSH103", ignores)
    findings += scan_cmake(root, ignores)
    findings.sort(key=lambda item: (item.path, item.line, item.code))
    for item in findings:
        print(f"{item.path}:{item.line} {item.code} {item.message}")
    if findings:
        print(f"Kadoka shogi check: {len(findings)} error(s)")
        return 1
    print("Kadoka shogi check: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
