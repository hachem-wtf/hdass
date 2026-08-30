#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

docker compose up -d >/dev/null

docker compose exec -T -e "NO_COLOR=${NO_COLOR:-}" hdass bash /hdass/scripts/run_suite.sh </dev/null
