#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

FORMAT = "kadoka.ai_metadata.v1"
VALID_GAMES = {"othello", "shogi", "tetris"}
REQUIRED_STRING_FIELDS = (
    "model_id",
    "model_name",
    "model_version",
    "architecture",
    "game",
)


def validate(path: Path, expected_game: str | None) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return [f"{path}: META000 {exc}"]

    if not isinstance(data, dict):
        return [f"{path}: META001 root must be an object"]

    if data.get("format") != FORMAT:
        errors.append(f"{path}: META101 format must be {FORMAT}")

    for field in REQUIRED_STRING_FIELDS:
        value = data.get(field)
        if not isinstance(value, str) or not value.strip():
            errors.append(f"{path}: META102 {field} must be a non-empty string")

    game = data.get("game")
    if isinstance(game, str) and game not in VALID_GAMES:
        errors.append(f"{path}: META103 unknown game: {game}")
    if expected_game and game != expected_game:
        errors.append(f"{path}: META104 expected game {expected_game}, got {game!r}")

    license_value = data.get("license")
    if isinstance(license_value, str):
        if not license_value.strip():
            errors.append(f"{path}: META105 license must not be empty")
    elif isinstance(license_value, dict):
        technical = license_value.get("technical")
        if not isinstance(technical, str) or not technical.strip():
            errors.append(f"{path}: META106 license.technical must be a non-empty string")
    else:
        errors.append(f"{path}: META105 license must be a string or object")

    for field in ("runtime_requirements", "determinism", "source", "distribution"):
        if field in data and not isinstance(data[field], dict):
            errors.append(f"{path}: META107 {field} must be an object")

    for field in ("dataset_provenance", "benchmark_results"):
        if field in data and not isinstance(data[field], list):
            errors.append(f"{path}: META108 {field} must be an array")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate kadoka.ai_metadata.v1 files")
    parser.add_argument("paths", nargs="+", type=Path)
    parser.add_argument("--game", choices=sorted(VALID_GAMES))
    args = parser.parse_args()

    errors: list[str] = []
    for path in args.paths:
        errors.extend(validate(path, args.game))

    for error in errors:
        print(error)
    if errors:
        print(f"Family metadata check: {len(errors)} error(s)")
        return 1

    print(f"Family metadata check: OK ({len(args.paths)} file(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
