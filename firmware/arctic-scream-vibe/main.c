/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Arctic Scream VIBE: Arctic Scream (engine/, synced from arctic-drones by tools/sync_engine.sh, never edited here) as a
 * VIBE FM firmware. Built with the VIBE FM SDK (Apache-2.0).
 *
 * The engine runs as on the FM-1, on a fake FM-1 (port/fm1_vibe.c), at its own 44.1 kHz: vibe_on_audio asks it for
 * the milliseconds the block needs (app_step, each one platform_poll) and resamples its ring to the host's rate. */
#define FM1_PLATFORM_FILE "../port/fm1_vibe.c"   /* (platform.c includes it, relative to engine/) */
#define APP_VER "2.0"                           /* (the same as Arctic Scream on the FM-1: DEVELOPER.md) */
#include "engine/grimm.c"

#include "vibe.h"
#include "vibe_gfx.h"
#include "vibe_dsp.h"

static float vb_sr = 44100.0f, vb_step = 1.0f, vb_pos;   /* the host's rate; engine frames a host frame; where in the ring */

/* ---- the parameters (the DAW's, the plugin's Editor): sound in and the VOICE. Their ids are for ever */
enum { P_IN, P_MODE, P_GAIN, P_VOICE, P_PITCH, P_RING, P_RING_HZ, P_GROWL, P_HOLVUT, P_DRY, P_DELAY, P_SPACE, P_GRAIN,
       P_HOLVUT_AMT, P_SLOT, P_REC_DRY, P_COUNT };
static const char *const C_IN[] = {"OFF", "IN 1", "IN 2", "IN 1+2"};
static const char *const C_MODE[] = {"LINE", "MIC", "HI-Z"};
static const char *const C_SLOT[] = {"1", "2", "3", "4"};
static const char *const C_REC[] = {"AS HEARD", "DRY"};
static const char *const C_HOLVUT[] = {"OFF", "T1", "T2", "T3", "T4", "ALL"};
static const vibe_param PARAMS[P_COUNT] = {
    {"in", "Input", 0, 3, 0, "", 4, C_IN},
    {"in_mode", "Input type", 0, 2, 1, "", 3, C_MODE},
    {"in_gain", "Input gain", -12, 36, 0, "dB", 0, 0},
    {"voice", "Voice", 0, 100, 100, "%", 0, 0},
    {"voice_pitch", "Pitch", -24, 12, -12, "st", 0, 0},
    {"voice_ring", "Ring", 0, 100, 0, "%", 0, 0},
    {"voice_ring_hz", "Ring freq", 20, 400, 60, "Hz", 0, 0},
    {"voice_growl", "Growl", 0, 100, 20, "%", 0, 0},
    {"in_holvut", "Into Holvut", 0, 5, 5, "", 6, C_HOLVUT},
    {"in_dry", "Input level", 0, 100, 50, "%", 0, 0},
    {"in_delay", "Input to DELAY", 0, 100, 0, "%", 0, 0},
    {"in_space", "Input to SPACE", 0, 100, 40, "%", 0, 0},
    {"in_grain", "Input to GRAIN", 0, 100, 0, "%", 0, 0},
    {"in_holvut_amt", "Holvut hears", 0, 100, 60, "%", 0, 0},
    {"sample_slot", "Sample slot", 0, 3, 0, "", 4, C_SLOT},
    {"rec_dry", "Record", 0, 1, 0, "", 2, C_REC},
};
static const char *const GROUPS[P_COUNT] = {"IN", "IN", "IN", "VOICE", "VOICE", "VOICE", "VOICE", "VOICE", "ROUTE",
                                            "ROUTE", "ROUTE", "ROUTE", "ROUTE", "ROUTE",
                                            "SAMPLE", "SAMPLE"};
static int vb_in_was = -1, vb_mode_was = -1;

static void vb_input_sync(void)                 /* (not from the audio) the host's input on, its type, as the
                                                 * parameters say */
{
    int in = (int)vibe_param_get(P_IN), mode = (int)vibe_param_get(P_MODE);
    if (in != vb_in_was && in > 0) vibe_request(VIBE_REQ_AUDIO_INPUT);
    if (mode != vb_mode_was || in != vb_in_was) vibe_input_mode(0, mode), vibe_input_mode(1, mode);
    vb_in_was = in, vb_mode_was = mode;
}

static void vb_live_params(void)                /* (once a block) the parameters into live_in.c's settings */
{
    int h = (int)vibe_param_get(P_HOLVUT);
    lv.on = (int)vibe_param_get(P_IN) > 0;
    lv.gain = vd_exp2(vibe_param_get(P_GAIN) * (1.0f / 6.0206f));
    lv.voice = vibe_param_get(P_VOICE) * 0.01f;
    lv.shift = vd_exp2(vibe_param_get(P_PITCH) * (1.0f / 12.0f));
    lv.ring = vibe_param_get(P_RING) * 0.01f;
    lv.ring_hz = vibe_param_get(P_RING_HZ);
    lv.growl = vibe_param_get(P_GROWL) * 0.01f;
    live_ex = h == 5 ? 15u : h > 0 ? 1u << (h - 1) : 0u;
    live_amt = (int32_t)(vibe_param_get(P_HOLVUT_AMT) * 1.27f + 0.5f);
    sl_rec_dry = (int)vibe_param_get(P_REC_DRY);
    live_g[0] = (int32_t)(vibe_param_get(P_DRY) * 163.84f);   /* (100 %: half, as a track at full LEVEL) */
    live_g[1] = (int32_t)(vibe_param_get(P_DELAY) * 327.67f);
    live_g[2] = (int32_t)(vibe_param_get(P_SPACE) * 327.67f);
    live_g[3] = (int32_t)(vibe_param_get(P_GRAIN) * 327.67f);
}

#include "port/slots.c"                         /* long samples: REC, LOAD, DROP into the four slots */
#include "port/panel.c"                         /* the VIBE's controls and display */
#include "port/persist.c"                       /* the saves, the settings, the samples: the host keeps them */

void vibe_on_init(float sample_rate)
{
    vb_sr = sample_rate > 1000.0f ? sample_rate : 44100.0f;
    vb_step = (float)FS / vb_sr;
    vibe_notes(VIBE_NOTES);
    vibe_params(PARAMS, P_COUNT);
    vibe_param_groups(GROUPS);
    li_step = vb_sr / (float)FS;
    vb_input_sync();
    memset(fake_flash, 0xFF, sizeof fake_flash);   /* (erased, as a new FM-1's) */
    vs_load();                                  /* (what the host kept: the FM-1 as it was left) */
    { char p[8]; int a, b2, c; (void)vibe_media_dropped(p, 0, &a, &b2, &c); }   /* (SDK 14: drops welcome) */
    app_start();
}

void vibe_on_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_PARAM) vb_input_sync();
    else if (e->type == VIBE_EV_SAVE) vs_flush(1);
    else if (e->type == VIBE_EV_MEDIA) {        /* (SDK 13: the host has more; a source came or went; a drop) */
        if (e->id == VIBE_MEDIA_EV_SOURCES && !ml_cur[0]) ml_want = 1;
        sl_work((uint32_t)vibe_param_get(P_SLOT));
    }   /* (the host keeps our state now: a DAW saving its project) */
    else if (e->type == VIBE_EV_NOTE) {         /* a note: the VIBE's keys and pads on the selected track's MIDI
                                                 * channel (MIDI CHANNEL's T1 .. T4, or any for ALL > SEL), MIDI and the
                                                 * DAW's on their own (as on the FM-1's USB); an off where its on went */
        static uint8_t ch_of[128];
        uint32_t v = (uint32_t)(e->value * 127.0f + 0.5f), n = (uint32_t)e->id & 127u, c;
        if (e->down) {
            c = e->midi[1] == VIBE_NOTE_KEYS || e->midi[1] == VIBE_NOTE_PADS ? out_ch(sel) : (uint32_t)e->midi[0] & 15u;
            ch_of[n] = (uint8_t)c;
        } else
            c = ch_of[n];
        vb_midi((e->down ? 0x90u : 0x80u) | c, n, e->down ? (v ? v : 1u) : 0u);
    } else
        vp_event(e);
}

static float vb_at(uint32_t i, uint32_t c) { return ring[(i % RING) * 2u + c]; }

void vibe_on_audio(const float *in, float *out_l, float *out_r, int frames)
{
    int k, src = (int)vibe_param_get(P_IN);   /* (1 IN 1, 2 IN 2, 3 both) */
    vb_live_params();
    live_put(src & 1 ? in : 0, src & 2 ? vibe_input(1) : 0, frames);
    for (k = 0; k < frames; k++) {
        uint32_t i = ring_r + (uint32_t)vb_pos, c;
        float t = vb_pos - (float)(uint32_t)vb_pos, o[2];
        while (ring_w - i < 3u) dm_step(), app_step();   /* (the engine's next millisecond: up to 256 frames more;
                                                 * a demo's next action first) */
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

/* ---- the display (port/panel.c); the FM-1's screen also to an FM-1 under VIBE FM Link, as it is (SDK 25) */
void vibe_on_draw(uint16_t *vfb)
{
    int o = 1 + set.oct;                        /* (the keys' octave follows OCT- / OCT+ as the FM-1 has it: C3 at 0) */
    vibe_keys_octave(o < 0 ? 0 : o > 7 ? 7 : o);
    sl_work((uint32_t)vibe_param_get(P_SLOT));
    vp_draw(vfb);
    vs_flush(0);
    vibe_link_screen(fb);
}
