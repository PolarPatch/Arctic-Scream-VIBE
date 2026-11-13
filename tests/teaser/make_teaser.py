#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 PolarPatch
"""A 30 s teaser of Arctic Scream VIBE, 1920 x 1080: the firmware itself (sdk/tools/wasmtest.mjs: its display at 30
frames a second and its sound), a caption a scene. Needs Pillow, ffmpeg, macOS `say` (the test voice).

    <venv>/bin/python tests/teaser/make_teaser.py    ->  tests/teaser/out/arctic-scream-vibe-teaser.mp4
"""
import json
import os
import subprocess
import wave

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(HERE, "out")
FONT = os.path.join(ROOT, "..", "arctic-drones", "assets", "fonts", "PressStart2P-Regular.ttf")
ICON = os.path.join(ROOT, "firmware", "arctic-scream-vibe", "icon.png")
WASM = os.path.join(ROOT, "sdk", "build", "arctic-scream-vibe.wasm")
S0, LEN, FPS, SR = 6.0, 30.0, 30, 48000          # (the boot screen's 5.6 s left out)

# the scenes: from, title, the line under it
SCENES = [(0, "ARCTIC SCREAM 2.0, NOW ON VIBE FM", "long dark screaming ambient from the frozen Arctic"),
          (5, "SOUND IN: A VOICE FEEDS THE HOWL", "mic, line or guitar, through the VOICE into Holvut"),
          (11, "K1 TO K6: SIX DEMOS AT A TOUCH", "A6 BIEGGA, the wind sings over a still tundra"),
          (16, "16 PADS: DICE IN EIGHT MOODS", "a new part on the track, from HUSH to MACHINE"),
          (21, "LONG SAMPLES: 95 SECONDS A SLOT", "record the input, or load a WAV from the card"),
          (26, "", "")]

B = lambda t, i, d=1: {"t": round(S0 + t, 3), "ev": [3, i, d, 0]}   # a button (VB_ ids: sdk/include/vibe.h)
VB_F4, VB_SHIFT, VB_K1, VB_K6, VB_RETURN, VB_REC = 3, 6, 10, 15, 16, 21


def press(t, i):
    return [B(t, i, 1), B(t + 0.06, i, 0)]


def steps():
    s = []
    s += press(0.2, VB_K1)                                            # A1 MANDALORIA
    s += [B(5.1, VB_SHIFT, 1)] + press(5.15, VB_F4) + [B(5.25, VB_SHIFT, 0)]   # IN
    s += [{"t": S0 + 5.4, "ev": [4, 0, 0, 1]}]                        # IN 1 on
    s += press(10.7, VB_RETURN)
    s += press(11.1, VB_K6)                                           # A6 BIEGGA
    s += [{"t": S0 + 16.2, "ev": [1, 2, 1, 0.8]}, {"t": S0 + 16.25, "ev": [1, 2, 0, 0]}]   # T3
    s += [{"t": S0 + 16.6, "ev": [1, 13, 1, 0.8]}, {"t": S0 + 16.65, "ev": [1, 13, 0, 0]}]   # DICE PULSE
    s += [{"t": S0 + 18.8, "ev": [1, 15, 1, 0.8]}, {"t": S0 + 18.85, "ev": [1, 15, 0, 0]}]   # DICE MACHINE
    s += [B(21.0, VB_SHIFT, 1)] + press(21.05, VB_F4) + press(21.2, VB_F4) + [B(21.3, VB_SHIFT, 0)]   # SAMPLE
    s += press(21.8, VB_REC) + press(25.3, VB_REC)
    s += press(25.8, VB_RETURN)
    for f in range(int(LEN * FPS)):
        s.append({"t": round(S0 + f / FPS + 0.0005, 4), "shot": "f%04d" % f})
    return s


def voice_input(path):                        # the test voice (macOS say) at 5.6 s and 21.9 s of the teaser
    tmp = os.path.join(OUT, "v.aiff")
    subprocess.run(["say", "-v", "Daniel", "-o", tmp, "I am the voice from under the ice. Hear me scream."], check=True)
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@48000", "-c", "1", tmp, tmp + ".wav"], check=True)
    v = wave.open(tmp + ".wav").readframes(10 ** 9)
    total = bytearray(int((S0 + LEN) * SR) * 2)
    for at in (5.6, 21.9):
        o = int((S0 + at) * SR) * 2
        total[o:o + len(v)] = v[:len(total) - o]
    w = wave.open(path, "wb")
    w.setnchannels(1), w.setsampwidth(2), w.setframerate(SR), w.writeframes(bytes(total))
    w.close()
    os.rename(tmp, os.path.join(OUT, "v-used.aiff"))


def main():
    os.makedirs(OUT, exist_ok=True)
    frames = os.path.join(OUT, "frames")
    os.makedirs(frames, exist_ok=True)
    voice_input(os.path.join(OUT, "input.wav"))
    script = {"seconds": S0 + LEN + 0.2, "input": "input.wav", "steps": steps()}
    json.dump(script, open(os.path.join(OUT, "script.json"), "w"))
    subprocess.run(["node", os.path.join(ROOT, "sdk", "tools", "wasmtest.mjs"), WASM, os.path.join(OUT, "script.json"),
                    frames, str(SR)], check=True, stdout=subprocess.DEVNULL)
    big, small, tiny = (ImageFont.truetype(FONT, n) for n in (30, 18, 16))
    icon = Image.open(ICON).convert("RGB")
    bg = (6, 4, 16)
    comp = os.path.join(OUT, "comp")
    os.makedirs(comp, exist_ok=True)
    for f in range(int(LEN * FPS)):
        t = f / FPS
        im = Image.new("RGB", (1920, 1080), bg)
        sc = max(i for i, s in enumerate(SCENES) if s[0] <= t)
        start, title, sub = SCENES[sc]
        if title:
            shot = Image.open(os.path.join(frames, "f%04d.png" % f)).convert("RGB").resize((1440, 960), Image.NEAREST)
            im.paste(shot, (240, 0))
            d = ImageDraw.Draw(im)
            a = min(1.0, (t - start) / 0.4)
            col = tuple(int(bg[k] + (c - bg[k]) * a) for k, c in enumerate((234, 246, 250)))
            sub_c = tuple(int(bg[k] + (c - bg[k]) * a) for k, c in enumerate((127, 168, 184)))
            d.text((960, 988), title, font=big, fill=col, anchor="mm")
            d.text((960, 1040), sub, font=small, fill=sub_c, anchor="mm")
        else:                                  # the end: the icon, who, what
            a = min(1.0, (t - start) / 0.6)
            ic = icon.resize((1152, 648), Image.NEAREST)
            im.paste(Image.blend(Image.new("RGB", ic.size, bg), ic, a), (384, 120))
            d = ImageDraw.Draw(im)
            c = tuple(int(bg[k] + (v - bg[k]) * a) for k, v in enumerate((127, 168, 184)))
            d.text((960, 860), "ARCTIC SCREAM VIBE 2.0  ·  POLARPATCH", font=small, fill=c, anchor="mm")
            d.text((960, 905), "built with the VIBE FM SDK  ·  GPL-3.0", font=tiny, fill=c, anchor="mm")
        im.save(os.path.join(comp, "c%04d.png" % f))
    mp4 = os.path.join(OUT, "arctic-scream-vibe-teaser.mp4")
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-framerate", str(FPS), "-i", os.path.join(comp, "c%04d.png"),
                    "-ss", str(S0), "-t", str(LEN), "-i", os.path.join(frames, "audio.wav"),
                    "-af", "afade=t=in:d=0.3,afade=t=out:st=%.1f:d=2.5,loudnorm=I=-16:TP=-1.5" % (LEN - 2.5),
                    "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "16", "-preset", "slow", "-c:a", "aac", "-b:a",
                    "256k", "-shortest", mp4], check=True)
    print(mp4)


if __name__ == "__main__":
    main()
