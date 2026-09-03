#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [ $# -lt 1 ]; then
	echo "usage: $(basename "$0") <file.hdass>" >&2
	exit 1
fi

file="$1"
name="$(basename "${file%.*}")"

docker compose up -d >/dev/null

docker compose exec -T -e SRC="$file" -e NAME="$name" hdass bash -c '
	set -e
	cd /hdass
	premake5 gmake >/dev/null
	make config=debug >/dev/null
	./bin/debug-linux/hdass "$SRC" -o "/tmp/$NAME.asm"
	echo
	cat "/tmp/$NAME.asm"
	echo
	nasm -f elf64 "/tmp/$NAME.asm" -o "/tmp/$NAME.o"
	ld -e main "/tmp/$NAME.o" -o "/tmp/$NAME"
	set +e
	"/tmp/$NAME"
	printf "\n[exit %s]\n" "$?"
' </dev/null
