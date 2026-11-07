"""Pull request の担当、自己レビュー、base 鮮度を検査する。"""

from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path
from urllib.parse import quote
from urllib.request import Request, urlopen


# {
#   責務: [section: 指定見出しに属する本文だけを取り出す]
#   引数: [body: PR本文] [heading: 見出し]
#   戻り値: [lines: 次の見出しまでの行]
# }
def section(body: str, heading: str) -> list[str]:
    lines = body.splitlines()
    start = next((i + 1 for i, line in enumerate(lines) if line.strip() == heading), None)
    if start is None:
        return []
    end = next((i for i in range(start, len(lines)) if lines[i].startswith("## ")), len(lines))
    return lines[start:end]


# {
#   責務: [check_body: 必須の担当情報と自己レビュー結果を検査する]
#   引数: [body: PR本文]
#   戻り値: [errors: 違反理由]
# }
def check_body(body: str) -> list[str]:
    errors: list[str] = []
    target = section(body, "## 対象と依存")
    for label in ("Issue:", "base branch / 開始SHA:", "担当 / 編集範囲:"):
        if not any(line.strip().startswith("- " + label) and line.split(label, 1)[1].strip() for line in target):
            errors.append(f"対象と依存に {label} が必要です")
    review = section(body, "## 自己レビュー")
    boxes = [line for line in review if re.match(r"^\s*-\s*\[[ xX]\]", line)]
    if len(boxes) < 4 or any(not re.match(r"^\s*-\s*\[[xX]\]", line) for line in boxes):
        errors.append("自己レビューの4項目を確認して [x] にしてください")
    return errors


# {
#   責務: [latest_base_sha: GitHubからbase branchの現行SHAを取得する]
#   引数: [repository: owner/name] [branch: base名] [token: Actions token]
#   戻り値: [sha: 現行baseのコミット]
# }
def latest_base_sha(repository: str, branch: str, token: str) -> str:
    url = f"https://api.github.com/repos/{repository}/branches/{quote(branch, safe='')}"
    request = Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "User-Agent": "kadoka-pr-metadata-check",
        },
    )
    with urlopen(request, timeout=15) as response:
        return json.load(response)["commit"]["sha"]


# {
#   責務: [main: PRイベントと共有先のbaseを照合して検査結果を返す]
#   戻り値: [exit_code: 全条件成功時0]
# }
def main() -> int:
    try:
        event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text(encoding="utf-8"))
        repository = os.environ["GITHUB_REPOSITORY"]
        token = os.environ["GITHUB_TOKEN"]
        pull_request = event["pull_request"]
        base = pull_request["base"]
        errors = check_body(pull_request.get("body") or "")
        current_sha = latest_base_sha(repository, base["ref"], token)
        if current_sha != base["sha"]:
            errors.append("PRイベントのbase SHAが共有先の最新SHAと異なります")
        if errors:
            for error in errors:
                print(f"PR metadata: {error}", file=sys.stderr)
            return 1
        print(f"PR metadata: OK (base={base['ref']}@{current_sha})")
        return 0
    except (KeyError, ValueError, OSError) as error:
        print(f"PR metadata: 検査できません: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
