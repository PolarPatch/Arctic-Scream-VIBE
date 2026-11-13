#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
# The VIBE FM SDK (Apache-2.0, https://github.com/Vibe-FM-SDK/vibe-fm-sdk) into sdk/, at the commit this firmware is
# built and checked with (SDK_REV: change it only after reading the SDK's news, and note it in DEVELOPER.md).
#   tools/get_sdk.sh
set -e
cd "$(dirname "$0")/.."
SDK_REV=86d02ca
if [ ! -d sdk/.git ]; then
  git clone -q https://github.com/Vibe-FM-SDK/vibe-fm-sdk sdk
fi
git -C sdk fetch -q origin
git -C sdk checkout -q "$SDK_REV"
echo "get_sdk: sdk/ at $(git -C sdk rev-parse --short HEAD)"
