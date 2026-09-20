#!/bin/bash
# Makes this Windows/MSVC C project checkable in a Claude Code web session.
#
# There is nothing to "install" in the usual sense — no package manifest — but
# without Windows headers clang cannot parse a single file here, since stdafx.h
# includes <Windows.h>. This fetches them and papers over one filename-case
# difference, after which the whole program syntax-checks on Linux.
set -euo pipefail

# A real checkout on Windows has the SDK already; only the web sandbox needs this.
if [ "${CLAUDE_CODE_REMOTE:-}" != "true" ]; then
  exit 0
fi

MINGW_INC=/usr/x86_64-w64-mingw32/include
SHIM="${HOME}/.cache/coope/winshim"

if [ ! -e "${MINGW_INC}/windows.h" ]; then
  echo "Installing Windows headers (mingw-w64)…"
  apt-get update -qq || true          # a broken third-party PPA must not fail the hook
  DEBIAN_FRONTEND=noninteractive apt-get install -y -qq mingw-w64-x86-64-dev
fi

# stdafx.h says #include <Windows.h>; the file on disk is windows.h. Windows
# filesystems don't care, Linux does — so provide the capitalised spelling.
mkdir -p "${SHIM}"
ln -sf "${MINGW_INC}/windows.h" "${SHIM}/Windows.h"

# Publish the flags so any session can run the check without rediscovering them.
if [ -n "${CLAUDE_ENV_FILE:-}" ]; then
  {
    echo "export COOPE_WINSHIM=\"${SHIM}\""
    echo "export COOPE_CFLAGS=\"--target=x86_64-w64-mingw32 -isystem ${SHIM} -isystem ${MINGW_INC}\""
  } >> "${CLAUDE_ENV_FILE}"
fi

echo "Ready: clang can now parse the project against a Windows target."
