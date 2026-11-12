/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Arctic Scream VIBE: Arctic Scream (engine/, synced from arctic-drones by tools/sync_engine.sh, never edited here) as a
 * VIBE FM firmware. Built with the VIBE FM SDK (Apache-2.0).
 *
 * The engine runs as on the FM-1, on a fake FM-1 (port/fm1_vibe.c), at its own 44.1 kHz: vibe_on_audio asks it for
 * the milliseconds the block needs (app_step, each one platform_poll) and resamples its ring to the host's rate. */
#define FM1_PLATFORM_FILE "../port/fm1_vibe.c"   /* (platform.c includes it, relative to engine/) */
#define APP_VER "0.1"
#include "engine/grimm.c"

#include "vibe.h"
#include "vibe_gfx.h"
#include "vibe_dsp.h"

static float vb_sr = 44100.0f, vb_step = 1.0f, vb_pos;   /* the host's rate; engine frames a host frame; where in the ring */

/* ---- the panel, for now: pads 1..14 the FM-1's fourteen buttons, the encoders its seven, the keys play the selected
 * track (MIDI, as a keyboard on the FM-1's USB) */
static uint32_t vb_btn;                         /* the FM-1 buttons held (BTN_ ids) */
static const int8_t VB_ENC[VE_COUNT] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4, ENC_ALGORITHM, ENC_PRESETS, ENC_SELECT};
static float vb_enc_frac[VE_COUNT];             /* (fractional detents, summed) */

void vibe_on_init(float sample_rate)
{
    vb_sr = sample_rate > 1000.0f ? sample_rate : 44100.0f;
    vb_step = (float)FS / vb_sr;
    vibe_notes(VIBE_NOTES);
    memset(fake_flash, 0xFF, sizeof fake_flash);   /* (erased, as a new FM-1's: the factory samples go in at boot) */
    app_start();
}

void vibe_on_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_NOTE) {
        uint32_t v = (uint32_t)(e->value * 127.0f + 0.5f);
        vb_midi(e->down ? 0x90u : 0x80u, (uint32_t)e->id, e->down ? (v ? v : 1u) : 0u);
    } else if (e->type == VIBE_EV_PAD && e->id < 14) {
        if (e->down) vb_btn |= 1u << e->id;
        else vb_btn &= ~(1u << e->id);
        vb_buttons(vb_btn);
    } else if (e->type == VIBE_EV_ENCODER && e->id < VE_COUNT) {
        int32_t d;
        vb_enc_frac[e->id] += e->value;
        d = (int32_t)vb_enc_frac[e->id];
        vb_enc_frac[e->id] -= (float)d;
        if (d) vb_enc((uint32_t)VB_ENC[e->id], d);
    }
}

static float vb_at(uint32_t i, uint32_t c) { return ring[(i % RING) * 2u + c]; }

void vibe_on_audio(const float *in, float *out_l, float *out_r, int frames)
{
    int k;
    (void)in;
    for (k = 0; k < frames; k++) {
        uint32_t i = ring_r + (uint32_t)vb_pos, c;
        float t = vb_pos - (float)(uint32_t)vb_pos, o[2];
        while (ring_w - i < 3u) app_step();     /* (the engine's next millisecond: up to 256 frames more) */
        for (c = 0; c < 2u; c++) {              /* cubic Hermite through i-1 .. i+2 */
            float y0 = vb_at(i - 1u, c), y1 = vb_at(i, c), y2 = vb_at(i + 1u, c), y3 = vb_at(i + 2u, c);
            float a = (y3 - y0) * 0.5f + (y1 - y2) * 1.5f, b = y0 - y1 * 2.5f + y2 * 2.0f - y3 * 0.5f, d = (y2 - y0) * 0.5f;
            o[c] = ((a * t + b) * t + d) * t + y1;
        }
        out_l[k] = o[0];
        out_r[k] = o[1];
        vb_pos += vb_step;
        if (vb_pos >= 1.0f) {                   /* (the ring's read point follows: whole frames) */
            uint32_t w = (uint32_t)vb_pos;
            ring_r += w;
            vb_pos -= (float)w;
        }
    }
}

/* ---- the display: the FM-1's 240 x 240 screen, 4/3 as big (320 x 320) in the middle, for now; and to an FM-1 under
 * VIBE FM Link as it is (SDK 25) */
void vibe_on_draw(uint16_t *vfb)
{
    uint32_t x, y;
    for (y = 0; y < VIBE_SCREEN_H; y++) {
        uint16_t *row = vfb + y * VIBE_SCREEN_W;
        const uint16_t *src = fb + (y * 3u / 4u) * 240u;
        for (x = 0; x < 80u; x++) row[x] = row[VIBE_SCREEN_W - 1u - x] = 0;
        for (x = 0; x < 320u; x++) row[80u + x] = vb_lit ? src[x * 3u / 4u] : 0;
    }
    vibe_link_screen(fb);
}
