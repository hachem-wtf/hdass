#!/usr/bin/env bash
# End-to-end example tests: transpile each program with hdass, assemble it with
# nasm, link it with ld, run it, and compare its stdout and exit status against
# the expected values below.
#
# The generated programs use Linux x86-64 syscalls, so this must run in the
# amd64 Linux environment (see the Docker setup in the README), not on macOS.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

hdass="${HDASS:-./bin/debug-linux/hdass}"
if [ ! -x "$hdass" ]; then
	echo "error: '$hdass' not found; build it first with:" >&2
	echo "  premake5 gmake && make config=debug" >&2
	exit 1
fi

for tool in nasm ld; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "error: '$tool' not found; run this inside the Docker environment" >&2
		exit 1
	fi
done

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

pass=0
fail=0

# check <name> <source> <expected_exit> <expected_stdout>
# expected_stdout is compared after trailing newlines are stripped (as $() does).
check()
{
	local name="$1" source="$2" expected_exit="$3" expected_stdout="$4"
	local asm="$work/$name.asm" obj="$work/$name.o" bin="$work/$name"

	if ! "$hdass" "$source" -o "$asm" 2>"$work/err"; then
		echo "FAIL $name (transpile)"; cat "$work/err"; fail=$((fail + 1)); return
	fi
	if ! nasm -f elf64 "$asm" -o "$obj" 2>"$work/err"; then
		echo "FAIL $name (assemble)"; cat "$work/err"; fail=$((fail + 1)); return
	fi
	if ! ld "$obj" -o "$bin" 2>"$work/err"; then
		echo "FAIL $name (link)"; cat "$work/err"; fail=$((fail + 1)); return
	fi

	local actual_stdout actual_exit
	actual_stdout="$("$bin")"
	actual_exit=$?

	if [ "$actual_exit" != "$expected_exit" ] || [ "$actual_stdout" != "$expected_stdout" ]; then
		echo "FAIL $name"
		echo "  expected: exit=$expected_exit stdout=$(printf '%q' "$expected_stdout")"
		echo "  actual:   exit=$actual_exit stdout=$(printf '%q' "$actual_stdout")"
		fail=$((fail + 1))
		return
	fi

	echo "PASS $name"
	pass=$((pass + 1))
}

check hello_world examples/hello_world.hdass 0  "Hello, World!"
check greet       examples/greet.hdass       0  "hdass works!"
check exit_code   examples/exit_code.hdass   42 ""
check arithmetic  examples/arithmetic.hdass  15 ""
check loop_sum    examples/loop_sum.hdass    15 ""
check branch      examples/branch.hdass      8  ""
check call        examples/call.hdass        21 ""
check fibonacci   examples/fibonacci.hdasm   0  "0
1
1
2
3
5
8
13
21
34"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
