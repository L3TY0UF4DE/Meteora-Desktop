#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Nested X11 desktop. Keeps the outer desktop, config and notification bus intact.
set -Eeuo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
[[ -n ${DISPLAY:-} ]] || { echo 'Run this from an existing graphical session with X11/Xwayland.' >&2; exit 1; }
for tool in Xephyr xauth dbus-run-session python3; do
    command -v "$tool" >/dev/null || { echo "Missing $tool. See README for nested-demo dependencies." >&2; exit 1; }
done
[[ -x build/meteora-shell ]] || { echo 'Build Meteora first: bash setup-lightweight.sh' >&2; exit 1; }
# Stage locally so the installed session script and assets are the exact tested code.
cmake --install build --prefix "$PWD/build/demo-stage" >/dev/null
exec python3 tools/nested_demo.py "$PWD/build/demo-stage" "${METEORA_DEMO_SIZE:-1280x800}"
