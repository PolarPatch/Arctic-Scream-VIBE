# Arctic Scream VIBE: developer notes

Arctic Scream as a native firmware for VIBE FM: one `.wasm` that runs as a VST3 / AU / CLAP plugin, in the browser
(the Playground, the SDK's emulator) and on the VIBE FM hardware. Built with the VIBE FM SDK. Our entry for
**The VIBE Box Challenge** on The Firmware Hub (<https://thefirmwarehub.com/lab/#contest>).

Working name: **Arctic Scream VIBE**. Firmware id (the folder's name, permanent): `arctic-scream-vibe`.

## The contest, in short

| | |
|---|---|
| Submissions close | **15 December 2026, 23:59 CET** |
| Vote | 16 to 26 December 2026; winners 27 December |
| Prizes | one VIBE Box for the community vote, one for the hub's pick |
| Must | be built with the VIBE FM SDK; play in the Playground and the plugin without crashing; use at least one or two things the FM-1 cannot; source public under an OSI licence by the deadline (ours: GPL-3.0) |
| Ports | count, if the original's licence is kept, its authors credited, and something VIBE-specific is added. The hub's one-click "Convert it" does **not** count |
| Hand in | a public GitHub repo, a release with the `.wasm`, a name, one line, the VIBE features used, a screenshot. The user submits it on the hub (their account), VIBE FM, the challenge box ticked. Updates allowed until the deadline |
| Rules | <https://thefirmwarehub.com/lab/rules/> (18+, up to three entries, you must have the rights to everything in it) |

## What the FM-1 cannot, and what we do with it

1. **Sound in** (IN 1 / IN 2, LINE, MIC or HI-Z: a microphone, a guitar, another synth). The heart of the entry:
   - **The live input into Holvut's loop.** Holvut's EXCITE already plays a track's sample into the string; here it can
     be the input itself, so a voice or a guitar is what the howl grows out of and keeps feeding.
   - **A horror voice** for the input: our own processor, built from known techniques (pitch down, ring modulation,
     formants, a granular freeze, reverse, the shimmer and Holvut after it). Written from scratch. Nothing is taken from
     any closed firmware: no decompiling, no copied tables. No product's name in the texts.
   - **Record the input into a sample slot** (SHIFT + REC or similar), so RUOHČAT, EXCITE and the SAMPLE source can use
     what was just sung or played.
   - The input through the effects alone (SPACE, DELAY, SHIMMER, TAPE, DRIVE) as an effect plugin in a DAW.
2. **Long samples.** The FM-1 has 6.6 s; the web player 95 s a slot. The VIBE has up to 64 MB of memory and the host's
   media (its card, USB sticks, the plugin's media folder: `vibe_media_*`, SDK 13). Samples load from there, never
   from the audio callback.
3. **The big screen (480 × 320)** for the pages in the house layout, and **the stage** (up to 1280 × 720, SDK 23):
   GUOVSSAHAS, the northern lights, and JIEHKKI's piece as a picture, for a room or a stream. Never the only way to
   anything.
4. **16 RGB pads:** DICE's eight moods and RUOHČAT's ice sounds, or the tracks' mutes and the sounds, lit in the
   tracks' colours.
5. **The DAW:** automatable parameters, the song position for the sequencer, GENERATE and JIEHKKI, the mixer (SDK 11).
6. Later, if there is time: CV out of MOD OUT's levels and LFOs, MIDI out.

What the contest form will list as VIBE features: audio in, the 480 × 320 screen, the pads, the stage, the DAW's song
position, the extra memory.

## Versions: one Arctic Scream, two devices

- **Patch releases, as we go:** what changes on the VIBE alone (the panel, the display, tweaks) is pushed to `main`
  as it is made and released as 2.0.1, 2.0.2 and so on: `firmware.json` `version` and `main.c` `APP_VER` set to it,
  then the tag `v2.0.N` (the workflow checks that they match, builds and releases). A release's files are never
  replaced: a fix is the next number. The hub takes the newest release.
- **The VIBE version is the FM-1 version.** Arctic Scream VIBE starts at **2.0**, as Arctic Scream 2.0 on the FM-1
  (`firmware.json` `version`, `main.c` `APP_VER`). A release is the same number on both: 2.1 on the FM-1 and 2.1 on
  the VIBE carry the same engine (OberVave does the same: 3.x on the VIBE after 2.x on the FM-1).
- **Two threads at once.** A new feature is built where it belongs and reaches both where it can:
  - the sound, the pages, the presets, DICE, JIEHKKI and the rest: in the engine, in `arctic-drones` first, tested on
    the FM-1 and synced here (`tools/sync_engine.sh`);
  - what only the VIBE can (sound in, long samples, the big screen, the pads, the stage, the DAW): here, in `port/`,
    with a hook in the engine behind a switch the FM-1 never sets (`FM1_LIVE_IN`, `FM1_PLATFORM_FILE`).
- The FM-1's tests stay green after every engine change (`tests/run_host.sh`, the ARM build), and `vibe-check` passes
  here.

## Rules for this project (as for Arctic Scream)

- Reply to the user in Norwegian. Code, comments, commits and docs in English, in the same style as `arctic-drones`.
- GPL-3.0-only. Copyright headers and LICENSING stay. Every idea from another project is written anew and credited.
- **No WCS material in this repo or in any build** (it stays in `~/Documents/WCS`). The user may load their own WAVs on
  their own VIBE or plugin at run time; nothing of theirs ships.
- No GR!MM samples or names in builds (credits only). No Synthux or Audrey in product texts (credits are fine).
  Never copy or decompile closed code (V16, the Audrey III wasm, the VIBE FM VST, the emulator).
- **The SDK's host is not ours:** never change, patch or redistribute VIBE FM VST or `sdk/emulator/`. If the host lacks
  something, the user opens an issue in `Vibe-FM-SDK/vibe-fm-sdk`.
- "Built with the VIBE FM SDK" is fine; "VIBE FM compatible" or the VIBE FM logo need permission.
- Ask before every push. Never force push. Releases come from a tag (later: a workflow), never uploaded by hand.
- `rm -rf` is not used: move to `~/.Trash` instead. (The SDK's own `tools/build.sh` cleans its `build/` itself.)
- Build and check locally first: `vibe-check` must end with PASSED before anything is called done.

## Layout

```
arctic-scream-vibe/
  DEVELOPER.md, README.md, LICENSE (GPL-3.0), LICENSING.md (to come: every source and its licence)
  firmware/arctic-scream-vibe/   the firmware folder (its name is the id): firmware.json, the .c files, icon.png,
                                 editor/ (an own Editor page, optional, SDK 16)
  tools/                         our scripts: build, sync of the engine from arctic-drones, checks
  tests/                         wasmtest scripts, the comparison with the FM-1 build
  docs/                          screens, the manual for the VIBE version, the contest text
  assets/                        the icon's source, fonts if any
  sdk/                           the VIBE FM SDK, cloned (Apache-2.0), not in our git: see below
```

## The SDK

- `sdk/` is a clone of <https://github.com/Vibe-FM-SDK/vibe-fm-sdk> (Apache-2.0), at **86d02ca** (11 November 2026).
  It is in `.gitignore`; `tools/` will fetch it at a pinned commit. The SDK is in early access: read its README's news
  before updating, and note the new commit here.
- Read first: `sdk/AGENTS.md`, `sdk/FROM-FM1.md`, the headers in `sdk/sdk/include/` (they win over the docs). A full
  ported firmware to read: `sdk/firmwares/obervave-src/` (GPL-3.0).
- Needs **Zig 0.13.0** (its clang compiles C to WebAssembly). Not installed yet.
- Build: `sh sdk/tools/build.sh <abs path>/firmware/arctic-scream-vibe` → `sdk/build/arctic-scream-vibe.{wasm,zip}`.
  Check: `node sdk/tools/vibe-check.mjs ...wasm` (PASSED). Play: `python3 sdk/tools/emulator.py`
  (<http://127.0.0.1:8766>), and the `.zip` dropped on VIBE FM VST in a DAW.
- Hard rules: no heap, no libc beyond `mem*`/`str*`, no `<math.h>` (`vd_*` instead), no store or media calls from
  `vibe_on_audio`, every frame of both outputs written, levels within ±1, no NaN; the sample rate and the block size
  (1..256) are the host's; `vibe_on_draw` paints the whole picture and may never run.

## The port: how the engine gets in

The engine is `arctic-drones/firmware/src/` (a unity build: `grimm.c` includes the rest). What we know:

- `app_audio(int32_t *out, uint32_t n)` renders the sound: it can be called from `vibe_on_audio`.
- `app_main()` is a loop that never returns (the panel, the screen, saving, MIDI): its body has to become a step
  function, called from `vibe_on_audio` for the timing (counted in frames) and `vibe_on_draw` for the screen. The
  browser build gets around this with Asyncify; the VIBE has no such thing, so the loop is split for real.
- The engine runs at a fixed **44.1 kHz** (`FS`, tables from `tools/gen_*.py`). Step 1: run it at 44.1 kHz and resample
  to the host's rate (as the web player's worklet does at 48 kHz). Later, where it matters, rate tables in
  `vibe_on_init`.
- Flash saves become `vibe_store_*` files; the samples come from the host's media.
- The 240 × 240 drawing is replaced by pages drawn anew at 480 × 320 (`vg_*`), in the house layout.

The engine files are taken from `arctic-drones` by a sync script that records the commit they came from, so the two
stay one engine. Fixes to the engine are made in `arctic-drones` first.

## Plan (to 15 December)

1. **Zig and the SDK** running: `hello` built, checked and played in the emulator.
2. **A skeleton that sounds:** the engine in `vibe_on_audio`, notes in, one simple page. Compared with the FM-1 build.
3. **The controls and the pages** in the house layout (SOUND · ENV · LFO · ARP · FX · SEQ), HOME's overview, FOCUS.
4. **Sound in:** the input into Holvut, the horror voice, recording into a slot.
5. **Long samples** from the media; the pads; the stage with GUOVSSAHAS.
6. **The DAW:** parameters, the song position, the mixer; saving with `vibe_store_*`.
7. **Release:** icon, LICENSING, README, screenshots, a short video; the repo public, a release with the `.wasm`;
   the user submits it on the hub well before 15 December.

## Ideas waiting

- **From ZEN-1 (2026-11-13):** the Arctic that plays (the aurora, the ice and the snow on the display make the notes;
  the keys reach into the picture) and the ice that answers your notes. Engine ideas, written up in arctic-drones'
  DEVELOPER.md ("What ZEN-1 showed"); the VIBE's display and stage are where they would shine. Waiting.
- **The split screen:** back to full screen by itself after ~8 s untouched; the right half in the FM-1's pixel look; the
  left half the tracks alone, full height.

## Log

- 2026-11-13: started. The folder, these notes, the SDK cloned at 86d02ca. The GitHub repo
  `PolarPatch/Arctic-Scream-VIBE` to be created private, made public for the release.
