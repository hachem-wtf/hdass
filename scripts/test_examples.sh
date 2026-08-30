#!/usr/bin/env bash
# End-to-end example tests: transpile each program with hdass, assemble it with
# nasm, link it with ld, run it, and compare its stdout and exit status against
# the expected values below.
#
# The generated programs use Linux x86-64 syscalls, so this must run in the
# amd64 Linux environment (see the Docker setup in the README), not on macOS.
#
# Colours are on by default; set NO_COLOR to disable them.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [ -z "${NO_COLOR:-}" ]; then
	bold=$'\033[1m'; dim=$'\033[2m'; red=$'\033[31m'; green=$'\033[32m'; reset=$'\033[0m'
else
	bold=; dim=; red=; green=; reset=
fi

hdass="${HDASS:-./bin/debug-linux/hdass}"
if [ ! -x "$hdass" ]; then
	echo "${red}error:${reset} '$hdass' not found; build it first with:" >&2
	echo "  premake5 gmake && make config=debug" >&2
	exit 1
fi

for tool in nasm ld; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "${red}error:${reset} '$tool' not found; run this inside the Docker environment" >&2
		exit 1
	fi
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0
fail=0

# check <name> <description> <source> <expected_exit> <expected_stdout>
# expected_stdout is compared after trailing newlines are stripped (as $() does).
check()
{
	local name="$1" desc="$2" source="$3" expected_exit="$4" expected_stdout="$5"
	local asm="$work/$name.asm" obj="$work/$name.o" bin="$work/$name" stage=""

	if ! "$hdass" "$source" -o "$asm" 2>"$work/err"; then
		stage="transpile"
	elif ! nasm -f elf64 "$asm" -o "$obj" 2>"$work/err"; then
		stage="assemble"
	elif ! ld -e main "$obj" -o "$bin" 2>"$work/err"; then
		stage="link"
	fi

	if [ -n "$stage" ]; then
		printf '  %s✘%s %-12s %s%s%s\n' "$red" "$reset" "$name" "$dim" "$desc" "$reset"
		printf '      %sfailed to %s%s\n' "$red" "$stage" "$reset"
		sed 's/^/      /' "$work/err"
		fail=$((fail + 1))
		return
	fi

	local actual_stdout actual_exit
	actual_stdout="$(timeout 10 "$bin")"
	actual_exit=$?

	if [ "$actual_exit" != "$expected_exit" ] || [ "$actual_stdout" != "$expected_stdout" ]; then
		printf '  %s✘%s %-12s %s%s%s\n' "$red" "$reset" "$name" "$dim" "$desc" "$reset"
		if [ "$actual_exit" = 124 ]; then
			printf '      %stimed out (likely an infinite loop)%s\n' "$red" "$reset"
		fi
		printf '      expected: exit %s stdout=%s\n' "$expected_exit" "$(printf '%q' "$expected_stdout")"
		printf '      actual:   exit %s stdout=%s\n' "$actual_exit" "$(printf '%q' "$actual_stdout")"
		fail=$((fail + 1))
		return
	fi

	printf '  %s✔%s %-12s %s%-42s%s %sexit %s%s\n' \
		"$green" "$reset" "$name" "$dim" "$desc" "$reset" "$dim" "$expected_exit" "$reset"
	pass=$((pass + 1))
}

printf '%s━━ example programs ━━%s\n' "$bold" "$reset"

check hello_world "writes a greeting to stdout"           examples/hello_world.hdass 0  "Hello, World!"
check greet       "writes a fixed string"                 examples/greet.hdass       0  "hdass works!"
check exit_code   "exits with a status code"              examples/exit_code.hdass   42 ""
check arithmetic  "integer compound-assignment math"      examples/arithmetic.hdass  15 ""
check loop_sum    "sums 1..5 with a countdown loop"       examples/loop_sum.hdass    15 ""
check branch      "selects the larger of two values"      examples/branch.hdass      8  ""
check call        "passes an argument through a proc"     examples/call.hdass        21 ""
check fibonacci   "prints the first ten Fibonacci numbers" examples/fibonacci.hdasm  0  "0
1
1
2
3
5
8
13
21
34"

printf '\n'
if [ "$fail" -eq 0 ]; then
	printf '  %s✔ all %d examples passed%s\n' "$green" "$pass" "$reset"
else
	printf '  %s✘ %d of %d examples failed%s\n' "$red" "$fail" "$((pass + fail))" "$reset"
fi

[ "$fail" -eq 0 ]
