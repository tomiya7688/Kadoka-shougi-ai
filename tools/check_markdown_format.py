from __future__ import annotations

import os
import subprocess
import sys


def changed_markdown_files(base_sha: str, head_sha: str) -> list[str]:
    result = subprocess.run(
        [
            "git",
            "diff",
            "--raw",
            "--no-abbrev",
            "--diff-filter=ACMRT",
            "-z",
            base_sha,
            head_sha,
            "--",
            "*.md",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    records = (record for record in result.stdout.split(b"\0") if record)
    files = []
    while metadata := next(records, None):
        fields = metadata[1:].split()
        _, new_mode, _, _, status = fields
        old_path = os.fsdecode(next(records))
        path = os.fsdecode(next(records)) if status.startswith((b"R", b"C")) else old_path
        if new_mode in (b"100644", b"100755") and path.endswith(".md"):
            files.append(path)
    return files


def main() -> int:
    base_sha = os.environ["BASE_SHA"]
    head_sha = os.environ["HEAD_SHA"]
    files = changed_markdown_files(base_sha, head_sha)
    if not files:
        print("Markdown format check: no changed regular Markdown files")
        return 0

    subprocess.run(
        [
            sys.executable,
            "-m",
            "mdformat",
            "--number",
            "--end-of-line",
            "keep",
            "--extensions",
            "gfm",
            "--check",
            "--",
            *files,
        ],
        check=True,
    )
    print(f"Markdown format check: OK ({len(files)} file(s))")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
