/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Sound in (what the FM-1 cannot): VIBE FM's IN 1 / IN 2, through the VOICE (a horror voice: pitch, ring, growl), into
 * the engine. Written for this firmware from textbook techniques (a two-tap delay-line pitch shifter with crossfaded
 * windows, a ring modulator, a soft clip); none of another's code.
 *
 *   main.c   live_put: each host block's input, at the host's rate, into li_ring
 *   engine   live_take (grimm.c app_audio): n frames at 44.1 kHz for its render, resampled from li_ring a little behind
 *            its newest frame (the engine renders ahead of the host's blocks), through the VOICE; live_ex: the tracks
 *            whose Holvut strings hear it (howl.c hw_excite, EXCITE the amount); live_g: the levels into the mix and
 *            the DLY, REV and GRAIN buses (grimm.c live_mix)
 *
 * The engine includes this file through the platform (fm1_vibe.c), so it is in the engine's audio path: nothing here
 * waits, allocates or calls the host. */
#define FM1_LIVE_IN 1
#include "vibe_dsp.h"

#define LI_RING 8192u                           /* host frames kept (~170 ms at 48 kHz) */
#define LI_LAG 768u                             /* how far behind the newest the engine reads (the engine runs up to
                                                 * 256 of its frames ahead of the host's block, and a block is <= 256) */
#define LV_DL 4096u                             /* the VOICE's pitch line: 4096 frames (~93 ms) at 44.1 kHz */
#define LV_WIN 2048.0f                          /* its window: ~46 ms (longer: smoother, more smeared) */

static float li_ring[LI_RING];
static uint32_t li_w, li_r, li_live;            /* written (host frames, counting on); read point's whole part; frames
                                                 * since the input last came (0: none) */
static float li_rf, li_step = 1.0f;             /* the read point's fraction; host frames an engine frame (sr / 44100) */
static int16_t li_buf[256];                     /* (VB, voices.c: the longest render) */
static uint32_t live_ex;                        /* the tracks (bit t) whose Holvut hears the input */
static int32_t live_amt;                        /* .. how much (0..127, as EXCITE: in its place) */
static int32_t live_g[4];                       /* the input's levels: the mix, DLY, REV, GRAIN (Q15) */

static struct {                                 /* the settings (main.c, from the parameters, once a block) */
    int on;                                     /* the input heard at all */
    float gain;                                 /* IN's level, linear */
    float voice;                                /* the VOICE's share, 0 (the input as it is) .. 1 (the voice alone) */
    float shift;                                /* PITCH as a ratio (2^(semitones / 12)) */
    float ring, ring_hz;                        /* RING: its share, its frequency */
    float growl;                                /* GROWL: the soft clip's drive, 0 .. 1 */
} lv = {0, 1.0f, 1.0f, 0.5f, 0.0f, 60.0f, 0.0f};

static float lv_dl[LV_DL];                      /* the pitch line */
static uint32_t lv_dw;
static float lv_ph, lv_rph, lv_hp_x, lv_hp_y, lv_env;

static void live_put(const float *in1, const float *in2, int frames)   /* main.c: the host's block (NULL: off) */
{
    int k;
    if (!in1 && !in2) { li_live = 0; return; }
    for (k = 0; k < frames; k++) {
        float x = (in1 ? in1[k] : 0.0f) + (in2 ? in2[k] : 0.0f);
        li_ring[li_w++ % LI_RING] = x;
    }
    if (!li_live) li_r = li_w - LI_LAG, li_rf = 0.0f;   /* (it came: read from LI_LAG back) */
    li_live = 1;
}

static float lv_tap(float d)                    /* the pitch line, d frames back (linear) */
{
    uint32_t i = (uint32_t)d;
    float f = d - (float)i, a = lv_dl[(lv_dw - i) & (LV_DL - 1u)], b = lv_dl[(lv_dw - i - 1u) & (LV_DL - 1u)];
    return a + (b - a) * f;
}

static float lv_voice(float x)                  /* one frame through the VOICE (44.1 kHz) */
{
    float y, g1, g2, d1, d2;
    lv_hp_y = x - lv_hp_x + lv_hp_y * 0.995f;   /* (DC and rumble out: ~35 Hz) */
    lv_hp_x = x;
    x = lv_hp_y;
    lv_dl[++lv_dw & (LV_DL - 1u)] = x;
    lv_ph += (1.0f - lv.shift) / LV_WIN;        /* PITCH: two read heads sliding through the line, half a window apart,
                                                 * each faded in and out by a sine window, so they cross unheard */
    if (lv_ph >= 1.0f) lv_ph -= 1.0f;
    if (lv_ph < 0.0f) lv_ph += 1.0f;
    d1 = 2.0f + lv_ph * LV_WIN;
    d2 = 2.0f + (lv_ph >= 0.5f ? lv_ph - 0.5f : lv_ph + 0.5f) * LV_WIN;
    g1 = vd_sin(lv_ph * 0.5f);
    g2 = vd_sin((lv_ph >= 0.5f ? lv_ph - 0.5f : lv_ph + 0.5f) * 0.5f);
    y = lv_tap(d1) * g1 + lv_tap(d2) * g2;
    if (lv.ring > 0.0f) {                       /* RING: the voice times a sine, a metal throat */
        lv_rph += lv.ring_hz * (1.0f / (float)FS);
        if (lv_rph >= 1.0f) lv_rph -= 1.0f;
        y *= 1.0f - lv.ring + lv.ring * vd_sin(lv_rph);
    }
    if (lv.growl > 0.0f) {                      /* GROWL: torn, the level kept */
        float dr = 1.0f + lv.growl * 24.0f;
        y = vd_tanh(y * dr) * (1.0f / vd_tanh(dr * 0.5f)) * 0.5f;
    }
    return x + (y - x) * lv.voice;
}

static const int16_t *live_take(uint32_t n)     /* the engine (app_audio): this render's n frames, or 0 */
{
    uint32_t i;
    if (!lv.on || !li_live) return 0;
    if (li_w - li_r > LI_RING - 4u || (int32_t)(li_w - li_r) < (int32_t)(LI_LAG / 4u))
        li_r = li_w - LI_LAG, li_rf = 0.0f;     /* (out of step, a stall or a burst: back to LI_LAG behind) */
    for (i = 0; i < n; i++) {
        float a = li_ring[li_r % LI_RING], b = li_ring[(li_r + 1u) % LI_RING], x = (a + (b - a) * li_rf) * lv.gain;
        uint32_t w;
        x = lv_voice(x);
        lv_env += ((x < 0.0f ? -x : x) - lv_env) * 0.0005f;   /* (the level, for the display) */
        x = x > 0.999f ? 0.999f : x < -0.999f ? -0.999f : x;
        li_buf[i] = (int16_t)(x * 32767.0f);
        li_rf += li_step;
        w = (uint32_t)li_rf;
        li_r += w, li_rf -= (float)w;
    }
    return li_buf;
}
