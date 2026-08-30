#!/usr/bin/env bash
# Build hdass and run the full test suite (unit tests plus the end-to-end
# example tests). Expects the amd64 Linux environment; it is invoked inside the
# container by scripts/docker_test.sh, but can also be run directly there.
#
# Colours are on by default; set NO_COLOR to disable them.
set -euo pipefail

cd "$(cd "$(dirname "$0")/.." && pwd)"

if [ -z "${NO_COLOR:-}" ]; then
	bold=$'\033[1m'; red=$'\033[31m'; green=$'\033[32m'; reset=$'\033[0m'
else
	bold=; red=; green=; reset=
fi

printf '%s━━ building compiler ━━%s\n' "$bold" "$reset"
if ! build=$(premake5 gmake 2>&1 && make config=debug 2>&1); then
	printf '%s\n' "$build"
	printf '  %s✘ build failed%s\n' "$red" "$reset"
	exit 1
fi
printf '  %s✔ compiler built%s\n\n' "$green" "$reset"

printf '%s━━ unit tests ━━%s\n' "$bold" "$reset"
if units=$(./bin/debug-linux/tests 2>/dev/null); then
	printf '  %s✔%s %s\n\n' "$green" "$reset" "$(printf '%s' "$units" | tail -1)"
else
	printf '%s\n' "$units"
	./bin/debug-linux/tests || true
	printf '  %s✘ unit tests failed%s\n' "$red" "$reset"
	exit 1
fi

exec bash ./scripts/test_examples.sh
