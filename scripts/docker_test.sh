#!/usr/bin/env bash
# Convenience wrapper: build hdass and run the full test suite (unit tests plus
# the end-to-end example tests) inside the amd64 Linux Docker environment.
# Run this from the host; it brings the container up if it is not already.
#
# Colours are on by default; set NO_COLOR to disable them.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

docker compose up -d >/dev/null

# stdin is redirected from /dev/null so `docker compose exec -T` does not hang
# waiting on the stream after the suite finishes.
docker compose exec -T -e "NO_COLOR=${NO_COLOR:-}" hdass bash /hdass/scripts/run_suite.sh </dev/null
