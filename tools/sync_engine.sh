#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
# Arctic Scream's engine into the VIBE firmware: arctic-drones/firmware/src/*.c, *.h and firmware/gen/*.h, as they are,
# into firmware/arctic-scream-vibe/engine/, and the commit they came from into engine/SOURCE. The engine is one: a fix
# to it is made in arctic-drones, then synced here. Never edit engine/ by hand.
#
#   tools/sync_engine.sh [path to arctic-drones]     (default: ../arctic-drones, beside this repository)
set -e
cd "$(dirname "$0")/.."
AD="${1:-../arctic-drones}"
[ -f "$AD/firmware/src/grimm.c" ] || { echo "sync_engine: no Arctic Scream at $AD"; exit 1; }
E=firmware/arctic-scream-vibe/engine
if [ -d "$E" ]; then mkdir -p ~/.Trash && mv "$E" ~/.Trash/engine-$(date +%Y%m%d-%H%M%S)-$$; fi   # (the old copy, never rm -rf)
mkdir -p "$E"
cp "$AD"/firmware/src/*.c "$AD"/firmware/src/*.h "$E"/ 2>/dev/null || cp "$AD"/firmware/src/*.c "$E"/
cp "$AD"/firmware/gen/*.h "$E"/
cp "$AD"/LICENSING.md "$E"/LICENSING.md         # (who made what in the engine: its credits and licences travel with it)
mkdir -p "$E"/LICENSES && cp "$AD"/LICENSES/*.txt "$E"/LICENSES/
REV=$(git -C "$AD" rev-parse --short HEAD)
DIRTY=$(git -C "$AD" status --porcelain -- firmware/src firmware/gen | head -1)
{
  echo "arctic-drones $REV$( [ -n "$DIRTY" ] && echo ' (with changes not yet committed)')"
  echo "branch $(git -C "$AD" branch --show-current)"
  echo "synced $(date -u +%Y-%m-%dT%H:%MZ)"
} > "$E/SOURCE"
echo "sync_engine: $(ls "$E" | wc -l | tr -d ' ') files from arctic-drones $REV$( [ -n "$DIRTY" ] && echo ' (uncommitted changes)')"
