#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
# Builds Arctic Scream VIBE with the VIBE FM SDK's own tools/build.sh (sdk/, a clone: DEVELOPER.md) and checks it.
#   tools/build.sh            ->  sdk/build/arctic-scream-vibe.{wasm,zip}, then vibe-check
#   ZIG   Zig 0.13.0 (default: ~/.local/zig/zig-macos-aarch64-0.13.0/zig, else zig on the PATH)
set -e
cd "$(dirname "$0")/.."
[ -n "$ZIG" ] || { ZIG="$HOME/.local/zig/zig-macos-aarch64-0.13.0/zig"; [ -x "$ZIG" ] || ZIG=zig; }
export ZIG
sh sdk/tools/build.sh ../firmware/arctic-scream-vibe   # (relative to sdk/: its build.sh splits a path with spaces)
[ "$1" = "--no-check" ] || node sdk/tools/vibe-check.mjs sdk/build/arctic-scream-vibe.wasm
