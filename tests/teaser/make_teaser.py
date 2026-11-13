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
          (6, "SOUND IN: A VOICE FEEDS THE HOWL", "mic, line or guitar, through the VOICE into Holvut"),
          (11.5, "K1 TO K6: SIX DEMOS AT A TOUCH", "A6 BIEGGA, the wind sings over a still tundra"),
          (16, "16 PADS: DICE IN EIGHT MOODS", "a new part on the track, from HUSH to MACHINE"),
          (20.5, "LONG SAMPLES: 95 SECONDS A SLOT", "record the input, or load a WAV from the card"),
          (25.5, "", "")]
# the opening: the whole VIBE (the SDK's emulator, a screenshot: assets/vibe-emulator.png, its display at DISP), the
# firmware's live picture in its display; then the camera moves in to the display alone
DEVICE = os.path.join(HERE, "assets", "vibe-emulator.png")
DISP = (152, 192, 402, 268)                     # (x, y, w, h) of the display in the screenshot
HOLD, ZOOM, XF = 2.8, 1.6, 0.4
# the outro: the engine's own boot screen (arctic-drones' host build: its logo's glitch, frame by frame), its title and
# the tracks' lines, with a video glitch over it (the colours split, slices torn sideways, lines of noise), calming down
AD = os.path.join(ROOT, "..", "arctic-drones")
BOOT_FPS, BOOT_FROM = 25, 40                  # the device alone, the move in, the fade to the display's layout

B = lambda t, i, d=1: {"t": round(S0 + t, 3), "ev": [3, i, d, 0]}   # a button (VB_ ids: sdk/include/vibe.h)
VB_F4, VB_SHIFT, VB_K1, VB_K6, VB_RETURN, VB_REC = 3, 6, 10, 15, 16, 21


def press(t, i):
    return [B(t, i, 1), B(t + 0.06, i, 0)]


def steps():
    s = []
    s += press(0.2, VB_K1)                                            # A1 MANDALORIA
    s += [B(6.1, VB_SHIFT, 1)] + press(6.15, VB_F4) + [B(6.25, VB_SHIFT, 0)]   # IN
    s += [{"t": S0 + 6.4, "ev": [4, 0, 0, 1]}]                        # IN 1 on
    s += press(11.3, VB_RETURN)
    s += press(11.6, VB_K6)                                           # A6 BIEGGA
    s += [{"t": S0 + 16.2, "ev": [1, 2, 1, 0.8]}, {"t": S0 + 16.25, "ev": [1, 2, 0, 0]}]   # T3
    s += [{"t": S0 + 16.5, "ev": [1, 13, 1, 0.8]}, {"t": S0 + 16.55, "ev": [1, 13, 0, 0]}]   # DICE PULSE
    s += [{"t": S0 + 18.5, "ev": [1, 15, 1, 0.8]}, {"t": S0 + 18.55, "ev": [1, 15, 0, 0]}]   # DICE MACHINE
    s += [B(20.6, VB_SHIFT, 1)] + press(20.65, VB_F4) + press(20.8, VB_F4) + [B(20.9, VB_SHIFT, 0)]   # SAMPLE
    s += press(21.3, VB_REC) + press(24.8, VB_REC)
    s += press(25.3, VB_RETURN)
    for f in range(int(LEN * FPS)):
        s.append({"t": round(S0 + f / FPS + 0.0005, 4), "shot": "f%04d" % f})
    return s


def voice_input(path):                        # the test voice (macOS say) at 6.6 s and 21.4 s of the teaser
    tmp = os.path.join(OUT, "v.aiff")
    subprocess.run(["say", "-v", "Daniel", "-o", tmp, "I am the voice from under the ice. Hear me scream."], check=True)
    subprocess.run(["afconvert", "-f", "WAVE", "-d", "LEI16@48000", "-c", "1", tmp, tmp + ".wav"], check=True)
    v = wave.open(tmp + ".wav").readframes(10 ** 9)
    total = bytearray(int((S0 + LEN) * SR) * 2)
    for at in (6.6, 21.4):
        o = int((S0 + at) * SR) * 2
        total[o:o + len(v)] = v[:len(total) - o]
    w = wave.open(path, "wb")
    w.setnchannels(1), w.setsampwidth(2), w.setframerate(SR), w.writeframes(bytes(total))
    w.close()
    os.rename(tmp, os.path.join(OUT, "v-used.aiff"))


_boot = []


def boot_frames():                              # the boot screen, 240 x 240, at 25 frames a second (the host build)
    if _boot:
        return _boot
    d = os.path.join(OUT, "boot")
    os.makedirs(d, exist_ok=True)
    scr = os.path.join(d, "boot.txt")
    open(scr, "w").write("t 6000\nend\n")
    subprocess.run([os.path.join(AD, "build", "host", "grimm"), os.path.join(d, "b"), scr, str(BOOT_FPS)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    names = sorted(n for n in os.listdir(d) if n.endswith(".ppm"))
    for n in names:
        im = Image.open(os.path.join(d, n)).convert("RGB")
        _boot.append(im.crop((0, 58, 240, 206)))   # (the title, p0larpatch and the four lines)
    return _boot


def outro(t, dur, bg):
    import random
    fr = boot_frames()
    logo = fr[min(len(fr) - 1, BOOT_FROM + int(t * BOOT_FPS))]
    big = logo.resize((1440, 888), Image.LANCZOS)
    im = Image.new("RGB", (1920, 1080), bg)
    im.paste(big, (240, 10))
    rnd = random.Random(int(t * 30))
    amt = max(0.0, 1.0 - t / (dur * 0.85)) * 0.85 + 0.15 * (rnd.random() < 0.12)   # (strong at first, then a twitch now and then)
    if amt <= 0.02:
        return im
    r, g, b = im.split()                        # the colours split sideways
    sh = int(18 * amt * (0.5 + rnd.random()))
    r = r.transform(r.size, Image.AFFINE, (1, 0, -sh, 0, 1, 0))
    b = b.transform(b.size, Image.AFFINE, (1, 0, sh, 0, 1, 0))
    im = Image.merge("RGB", (r, g, b))
    for _ in range(int(14 * amt)):              # slices torn sideways
        y = rnd.randrange(40, 900)
        h = rnd.randrange(4, 40)
        dx = int(rnd.uniform(-120, 120) * amt)
        sl = im.crop((0, y, 1920, y + h))
        im.paste(bg, (0, y, 1920, y + h))
        im.paste(sl, (dx, y))
    d = ImageDraw.Draw(im)
    for _ in range(int(10 * amt)):              # lines of noise, in the tracks' colours
        y = rnd.randrange(40, 900)
        c = rnd.choice([(34, 211, 238), (63, 185, 80), (167, 139, 250), (255, 26, 140), (234, 246, 250)])
        x0 = rnd.randrange(0, 1700)
        d.rectangle((x0, y, x0 + rnd.randrange(60, 600), y + rnd.randrange(1, 4)), fill=c)
    return im


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
    device = Image.open(DEVICE).convert("RGB")
    dx, dy, dw, dh = DISP
    for f in range(int(LEN * FPS)):
        t = f / FPS
        im = Image.new("RGB", (1920, 1080), bg)
        sc = max(i for i, s in enumerate(SCENES) if s[0] <= t)
        start, title, sub = SCENES[sc]
        raw = Image.open(os.path.join(frames, "f%04d.png" % f)).convert("RGB")
        if title:
            shot = raw.resize((1440, 960), Image.LANCZOS)   # (smooth, not the pixels' blocks)
            im.paste(shot, (240, 0))
            if t < HOLD + ZOOM + XF:            # the opening: the whole VIBE, then in to its display
                dev = device.copy()
                dev.paste(raw.resize((dw, dh), Image.LANCZOS), (dx, dy))
                k = 0.0 if t < HOLD else min(1.0, (t - HOLD) / ZOOM)
                k = k * k * (3 - 2 * k)         # (ease in, ease out)
                s1 = 1440.0 / dw                # the end: the display where the layout has it (240, 0, 1440 x 960)
                end = (dx - 240 / s1, dy, 1920 / s1, 1080 / s1)
                fh = device.size[1]             # the start: the whole device, its height in the frame
                st_w = fh * 16 / 9
                begin = ((device.size[0] - st_w) / 2, 0, st_w, fh)
                cx, cy, cw, ch = (begin[i] + (end[i] - begin[i]) * k for i in range(4))
                cam = Image.new("RGB", (int(round(cw)), int(round(ch))), bg)
                cam.paste(dev, (int(round(-cx)), int(round(-cy))))
                cam = cam.resize((1920, 1080), Image.LANCZOS)
                a2 = 1.0 if t < HOLD + ZOOM else 1.0 - (t - HOLD - ZOOM) / XF
                im = Image.blend(im, cam, max(0.0, min(1.0, a2)))
            d = ImageDraw.Draw(im)
            if t < HOLD + ZOOM:                 # (over the device: a dark band under the caption)
                d.rectangle((0, 950, 1920, 1080), fill=bg)
            a = min(1.0, (t - start) / 0.4)
            col = tuple(int(bg[k] + (c - bg[k]) * a) for k, c in enumerate((234, 246, 250)))
            sub_c = tuple(int(bg[k] + (c - bg[k]) * a) for k, c in enumerate((127, 168, 184)))
            d.text((960, 988), title, font=big, fill=col, anchor="mm")
            d.text((960, 1040), sub, font=small, fill=sub_c, anchor="mm")
        else:                                  # the end: the logo, glitching; who, what
            im = outro(t - start, LEN - start, bg)
            d = ImageDraw.Draw(im)
            a = min(1.0, (t - start) / 0.8)
            c = tuple(int(bg[k] + (v - bg[k]) * a) for k, v in enumerate((127, 168, 184)))
            d.text((960, 900), "ARCTIC SCREAM VIBE 2.0  ·  POLARPATCH", font=small, fill=c, anchor="mm")
            d.text((960, 945), "built with the VIBE FM SDK  ·  GPL-3.0", font=tiny, fill=c, anchor="mm")
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
