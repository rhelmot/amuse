#!/usr/bin/env bash

args=("$@")

if (( NIX_DEBUG >= 7 )); then
	set -x
fi

NEXT=
for f in $NIX_CFLAGS_COMPILE; do
	if [[ -n "$NEXT" ]]; then
		args+=("$NEXT" "$f")
		NEXT=""
		continue
	fi
	case "$f" in
		-I*|-isystem=*|-D*)
			args+=("$f")
			;;
		-I|-isystem)
			NEXT="$f"
			;;
	esac
done
args+=(@defaultArgs@)

if (( NIX_DEBUG >= 1 )); then
	set -x
fi

exec @wrapped@ "${args[@]}"
