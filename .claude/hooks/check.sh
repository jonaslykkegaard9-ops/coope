#!/bin/bash
# Syntax-check the program, and optionally lint it:  check.sh [--tidy]
#
# main.c is the whole program — it includes console.c and timer.c, which pull in
# drawables.c, drawable.c, box.c, array.c and string.c — so one invocation covers
# every source file except test.c.
#
# test.c is deliberately not checked: its harness uses __if_exists, an MSVC-only
# extension. Enabling clang's -fms-extensions to get it breaks mingw's own
# vadefs.h and mmintrin.h, so the two cannot be combined. test.c needs MSVC.
set -uo pipefail
cd "$(dirname "$0")/../.."

MINGW_INC=/usr/x86_64-w64-mingw32/include
SHIM="${COOPE_WINSHIM:-${HOME}/.cache/coope/winshim}"
FLAGS=(--target=x86_64-w64-mingw32 -isystem "${SHIM}" -isystem "${MINGW_INC}")

if [ ! -e "${SHIM}/Windows.h" ]; then
  echo "Windows headers missing — run .claude/hooks/session-start.sh first." >&2
  exit 127
fi

# Always lint through main.c. This is a unity build: box.c, array.c and friends
# are #included in a specific order and are NOT valid translation units on their
# own (box.c alone reports 20 phantom errors), so linting a file directly lies.
# -header-filter keeps findings from the included .c files while dropping the SDK.
if [ "${1:-}" = "--tidy" ]; then
  exec clang-tidy --quiet -header-filter="^$(pwd)/" main.c -- "${FLAGS[@]}"
fi

clang "${FLAGS[@]}" -fsyntax-only main.c && echo "✅ syntax OK (whole program via main.c)"
