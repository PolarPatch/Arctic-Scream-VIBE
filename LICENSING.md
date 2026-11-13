# Licensing

Arctic Scream VIBE is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full text in
`LICENSE`). You may use, study, change and share it. If you give someone a firmware built from it, give them its
complete source under the same licence too.

It is three things in one `.wasm`:

| | | Licence |
| --- | --- | --- |
| **Arctic Scream's engine**, `firmware/arctic-scream-vibe/engine/` | Arctic Scream for the M-VAVE FM-1 (<https://github.com/PolarPatch/Arctic-scream-FM-1>), copied unchanged by `tools/sync_engine.sh` (its commit in `engine/SOURCE`). Who made what in it, and the licences of its parts (msfa Apache-2.0, DaisySP and Rings MIT, Press Start 2P OFL 1.1, and the rest), is in `engine/LICENSING.md`, the texts in `engine/LICENSES/` | GPL-3.0-only as a whole |
| **The VIBE layer**, `firmware/arctic-scream-vibe/main.c`, `port/`, `tools/`, `tests/` | Copyright (C) 2026 PolarPatch, written for this firmware: the fake FM-1 under the engine (`port/fm1_vibe.c`, after Arctic Scream's own browser build `web/fm1_web.c`, which follows Felucca's and X0X's browser builds, GPL-3.0), the panel and the display, sound in and the VOICE (textbook techniques, none of another's code), long samples, the demos, saving | GPL-3.0-only |
| **The VIBE FM SDK**, compiled in (`sdk/src/vibe_sdk.c`, `vibe_font.c`; not in this repository: `tools/get_sdk.sh` fetches it) | Copyright 2026 Sven Trogus (zvenson), <https://github.com/Vibe-FM-SDK/vibe-fm-sdk>. Its notice: `LICENSES/NOTICE-VIBE-FM-SDK.txt`. Its display fonts (glyphs of Roboto Condensed, Copyright 2011 Google Inc., and Archivo, Copyright 2020 The Archivo Project Authors) are SIL OFL 1.1: `LICENSES/OFL-VIBE-FM-SDK-fonts.txt` | Apache-2.0 (`LICENSES/Apache-2.0-VIBE-FM-SDK.txt`); may be combined into a GPL-3.0 work |

"VIBE FM" is a trademark of Sven Trogus. This firmware is *built with the VIBE FM SDK*; it does not claim to be "VIBE FM
compatible" and uses no VIBE FM logo.

`icon.png` is made from Arctic Scream's own boot screen (its title in the pixfont, the four tracks' colours): GPL-3.0-only
as the rest. The demos A1 .. A6 (`port/demos.h`) are Arctic Scream's own (its `web/gen_demos.mjs`, from its factory
sounds). No samples ship with the firmware: the slots are empty until you record or load your own.
