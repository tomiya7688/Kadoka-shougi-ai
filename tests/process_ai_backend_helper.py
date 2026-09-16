#!/usr/bin/env python3

import sys


def main() -> int:
    request_count = 0
    iterator = iter(sys.stdin)
    for raw in iterator:
        line = raw.rstrip("\r\n")
        if not line.startswith("request "):
            continue

        request_id = int(line.split()[1])
        request_count += 1

        for raw_request in iterator:
            if raw_request.rstrip("\r\n") == "end":
                break

        print(f"result {request_id}")
        print("move normal 7 7 7 6 0")
        print("score_cp 21")
        print("nodes 55")
        print("depth 3")
        print(f"info script_request_count={request_count}")
        print("end", flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
