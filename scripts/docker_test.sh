#!/usr/bin/env bash
# Convenience wrapper: build hdass and run the full test suite (unit tests plus
# the end-to-end example tests) inside the amd64 Linux Docker environment.
# Run this from the host; it brings the container up if it is not already.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

docker compose up -d

docker compose exec -T hdass bash -c '
	set -e
	premake5 gmake
	make config=debug
	./bin/debug-linux/tests
	./scripts/test_examples.sh
'
