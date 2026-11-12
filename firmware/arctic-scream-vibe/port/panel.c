/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Arctic Scream VIBE's panel: VIBE FM's controls onto Arctic Scream, and its display in the house layout.
 *
 *   K1..K6      SOUND ENV LFO ARP FX SEQ: the FM-1's EDIT, ENV, LFO, ARP, FX, SEQ (pressed again: their next page;
 *               held: their layer on the pads, as on the FM-1's keys)
 *   F1 F2 F3    SEL, GLOBAL, SAVE (held: their layers on the pads)
 *   F4 F5 F6    IN, VOICE, ROUTE: sound in, on the right half (encoders 1..4 its four values); again: back
 *   RETURN      the FM-1's HOME (and the right half back to the pads)
 *   HOME        what every control does (vg_controls); the library is the host's (held, SHIFT + HOME)
 *   PLAY REC    PLAY, REC; STOP: PLAY while it plays
 *   OCT - / +   OCT- / OCT+ (the keys follow: vibe_keys_octave)
 *   enc 1..4    KNOB 1..4 (on IN, VOICE, ROUTE: those four values); A the track (ALGORITHM), B PRESETS, DATA SELECT
 *   keys        notes on the selected track (MIDI, with the DAW's)
 *   pads        nothing held: 1..4 the track, 5..8 slots A..D (recalled), 9..16 DICE's eight moods on the track;
 *               a button held: the FM-1's function keys under it, 1..8 its eight white keys from F2, 9..16 the next
 *               eight (DICE, ACCIDENT) or, with FX, GENERATE's black keys */

enum { VP_FM, VP_IN, VP_VOICE, VP_ROUTE };      /* the right half: the pads / sound in's three pages */
static int vp_page = VP_FM, vp_help;
static uint32_t vp_btn;                         /* the FM-1 buttons the VIBE holds (BTN_ ids) */
static uint32_t vp_fkeys;                       /* the FM-1 keys the pads hold (bit = key) */
static int8_t vp_padkey[16];                    /* a held pad's FM-1 key (-1: a direct function, none) */
static uint32_t vp_shift;

static const int8_t VP_K[6] = {BTN_EDIT, BTN_ENV, BTN_LFO, BTN_ARP, BTN_FX, BTN_SEQ};
static const int8_t VP_F[3] = {BTN_SEL, BTN_GLO, BTN_SAVE};
static const int8_t VP_ENC[VE_COUNT] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4, ENC_ALGORITHM, ENC_PRESETS, ENC_SELECT};
static float vp_frac[VE_COUNT];                 /* (fractional detents, summed) */
static const uint32_t VP_TRK[4] = {0x22d3ee, 0x3fb950, 0xa78bfa, 0xff1a8c};   /* the tracks' colours (ui.c UI_ACCENTS) */
static const char *const VP_MOOD[8] = {"HUSH", "STILL", "SWELL", "DRIFT", "ECHO", "PULSE", "ARP", "MACHINE"};
static const int VP_PAGE_P[4][4] = {{0}, {P_IN, P_MODE, P_GAIN, P_DRY}, {P_PITCH, P_RING, P_RING_HZ, P_GROWL},
                                    {P_HOLVUT, P_HOLVUT_AMT, P_SPACE, P_DELAY}};
static const char *const VP_PAGE_N[4] = {"", "IN", "VOICE", "ROUTE"};

static void vp_set_btn(int b, int down)
{
    if (down) vp_btn |= 1u << b;
    else vp_btn &= ~(1u << b);
    vb_buttons(vp_btn);
}

static void vp_pad(int p, int down)             /* pad p (0..15) pressed / let go */
{
    if (!down) {
        if (vp_padkey[p] >= 0) vp_fkeys &= ~(1u << vp_padkey[p]), vb_keys(vp_fkeys);
        vp_padkey[p] = -1;
        return;
    }
    if (vp_btn & ~(1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {   /* a button held: the FM-1's keys under it */
        int k = p < 8 ? WK[p] : (vp_btn >> BTN_FX & 1u) ? BK[p - 8] : WK2[p - 8];
        vp_padkey[p] = (int8_t)k;
        vp_fkeys |= 1u << k;
        vb_keys(vp_fkeys);
        return;
    }
    vp_padkey[p] = -1;
    if (p < 4) sel = (uint32_t)p;               /* 1..4: the track */
    else if (p < 8) slot_do_recall((uint32_t)p - 4u);   /* 5..8: slots A..D */
    else dice((uint32_t)p - 8u, (uint32_t)(KEY0 + 12 * set.oct));   /* 9..16: DICE */
}

static void vp_value(int i, float d)            /* sound in's page: its value i turned d detents */
{
    int p = VP_PAGE_P[vp_page][i];
    const vibe_param *P = &PARAMS[p];
    float v = vibe_param_get(p), st = P->steps ? (P->max - P->min) / (float)(P->steps - 1) : (P->max - P->min) / 100.0f;
    if (P->steps) {                             /* (a choice: whole detents) */
        static float acc[4];
        acc[i] += d;
        if (acc[i] >= 1.0f || acc[i] <= -1.0f) { v += (float)(int)acc[i] * st; acc[i] -= (float)(int)acc[i]; }
    } else
        v += d * st;
    v = vd_clamp(v, P->min, P->max);
    vibe_param_set(p, v);
    vb_input_sync();
}

static void vp_event(const vibe_event *e)
{
    if (e->type == VIBE_EV_BUTTON) {
        int b = e->id;
        if (b == VB_HOME) { if (e->down) vp_help = !vp_help; return; }
        if (vp_help && e->down && (b == VB_RETURN)) { vp_help = 0; return; }
        if (b >= VB_K1 && b <= VB_K6) vp_set_btn(VP_K[b - VB_K1], e->down);
        else if (b >= VB_F1 && b <= VB_F3) vp_set_btn(VP_F[b - VB_F1], e->down);
        else if (b >= VB_F4 && b <= VB_F6) { if (e->down) vp_page = vp_page == b - VB_F4 + VP_IN ? VP_FM : b - VB_F4 + VP_IN; }
        else if (b == VB_RETURN) { vp_set_btn(BTN_HOME, e->down); if (e->down) vp_page = VP_FM; }
        else if (b == VB_PLAY) vp_set_btn(BTN_PLAY, e->down);
        else if (b == VB_STOP) { if (playing || !e->down) vp_set_btn(BTN_PLAY, e->down); }
        else if (b == VB_REC) vp_set_btn(BTN_REC, e->down);
        else if (b == VB_OCT_DOWN) vp_set_btn(BTN_OCT_DOWN, e->down);
        else if (b == VB_OCT_UP) vp_set_btn(BTN_OCT_UP, e->down);
        else if (b == VB_SHIFT) vp_shift = e->down;
    } else if (e->type == VIBE_EV_PAD && e->id < 16) {
        vp_pad(e->id, e->down);
    } else if (e->type == VIBE_EV_ENCODER && e->id < VE_COUNT) {
        int32_t d;
        if (vp_page != VP_FM && e->id <= VE_4) { vp_value(e->id, e->value); return; }
        vp_frac[e->id] += e->value;
        d = (int32_t)vp_frac[e->id];
        vp_frac[e->id] -= (float)d;
        if (d) vb_enc((uint32_t)VP_ENC[e->id], d);
    }
}

static void vp_leds(void)                       /* the pads' lights: the tracks, the slots, DICE (or the FM-1's keys' under
                                                 * a held button); the K and F keys */
{
    int p;
    for (p = 0; p < 16; p++) {
        uint32_t c;
        float l;
        if (vp_btn & ~(1u << BTN_OCT_DOWN | 1u << BTN_OCT_UP)) {
            int k = p < 8 ? WK[p] : (vp_btn >> BTN_FX & 1u) ? BK[p - 8] : WK2[p - 8];
            c = 0xFFFFFF, l = (vb_led_key >> k & 1u) ? 0.9f : 0.12f;
        } else if (p < 4)
            c = VP_TRK[p], l = (uint32_t)p == sel ? 1.0f : 0.25f;
        else if (p < 8)
            c = 0x22d3ee, l = slot_used((uint32_t)p - 4u) ? (pj_slot == p - 4 ? 0.9f : 0.4f) : 0.05f;
        else
            c = VP_TRK[sel & 3u], l = 0.15f + 0.05f * (float)(p - 8);
        vibe_led_pad(p, c, l);
    }
    for (p = 0; p < 6; p++) vibe_led_button(VB_K1 + p, VP_TRK[sel & 3u], PAGES[page].btn == VP_K[p] ? 1.0f : 0.15f);
    for (p = 0; p < 3; p++) vibe_led_button(VB_F4 + p, 0xff1a8c, vp_page == VP_IN + p ? 1.0f : 0.15f);
}

static const char *const VP_HELP[] = {
    "K1..K6", "SOUND ENV LFO ARP FX SEQ (again: next page; held: its pad layer)",
    "F1 F2 F3", "SEL, GLOBAL, SAVE (held: mutes, solos, slots on the pads)",
    "F4 F5 F6", "IN, VOICE, ROUTE: sound in on encoders 1..4",
    "ENC 1..4", "the four knobs on the screen",
    "ENC A / B", "the track / PRESETS",
    "DATA", "SELECT (steps, tempo)",
    "PADS 1..4", "the track",
    "PADS 5..8", "slots A..D",
    "PADS 9..16", "DICE: HUSH .. MACHINE",
    "RETURN", "the start page",
};

static void vp_cat(char *d, const char *s) { while (*d) d++; while ((*d++ = *s++) != 0) ; }

static void vp_text_value(int p, char *b)       /* a parameter's value as text */
{
    const vibe_param *P = &PARAMS[p];
    float v = vibe_param_get(p);
    if (P->choices) { strcpy(b, P->choices[(int)(v - P->min + 0.5f)]); return; }
    vg_itoa((int)(v < 0 ? v - 0.5f : v + 0.5f), b);
    if (P->unit && *P->unit) { vp_cat(b, " "); vp_cat(b, P->unit); }
}

static void vp_draw(uint16_t *vfb)
{
    uint32_t x, y;
    char b[24];
    int i;
    vp_leds();
    if (vp_help) { vg_controls("ARCTIC SCREAM", VP_HELP, sizeof VP_HELP / sizeof VP_HELP[0] / 2); return; }
    vg_clear(0x05080d);
    vg_text("ARCTIC SCREAM", 12, 8, VG_BOLD, 0xE6EAF2, VG_LEFT);   /* the header */
    vg_text(vp_page == VP_FM ? "PADS" : VP_PAGE_N[vp_page], 468, 8, VG_BOLD, vp_page == VP_FM ? 0x8A93A6 : 0xff1a8c, VG_RIGHT);
    for (y = 0; y < 240u; y++) {                /* the FM-1's screen, as it is, on the left */
        uint16_t *row = vfb + (38u + y) * VIBE_SCREEN_W;
        const uint16_t *src = fb + y * 240u;
        if (vb_lit) for (x = 0; x < 240u; x++) row[x] = src[x];
    }
    if (vp_page == VP_FM) {                     /* the right half: the pads, as they are now */
        for (i = 0; i < 16; i++) {
            int cx = 252 + (i % 4) * 56, cy = 44 + (i / 4) * 58;   /* (pad 0 top left, row by row) */
            uint32_t c = i < 4 ? VP_TRK[i] : i < 8 ? 0x22d3ee : VP_TRK[sel & 3u];
            int on = i < 4 ? (uint32_t)i == sel : i < 8 ? slot_used((uint32_t)i - 4u) : 1;
            const char *t = i < 4 ? (const char *[]){"T1", "T2", "T3", "T4"}[i] : i < 8 ? (const char *[]){"A", "B", "C", "D"}[i - 4] : VP_MOOD[i - 8];
            vg_round(cx, cy, 52, 52, 6, on ? c : 0x1a2230);
            vg_text(t, cx + 26, cy + 19, VG_SMALL, on ? 0x05080d : 0x8A93A6, VG_CENTER);
        }
    } else {                                    /* sound in: four values, encoders 1..4; the input's level */
        for (i = 0; i < 4; i++) {
            int p = VP_PAGE_P[vp_page][i], cy = 46 + i * 52;
            vg_text(PARAMS[p].name, 256, cy, VG_SMALL, 0x8A93A6, VG_LEFT);
            vp_text_value(p, b);
            vg_text(b, 468, cy + 16, VG_MED, 0xE6EAF2, VG_RIGHT);
            vg_rect(256, cy + 38, (int)(212.0f * (vibe_param_get(p) - PARAMS[p].min) / (PARAMS[p].max - PARAMS[p].min)), 3, 0xff1a8c);
        }
        vg_rect(256, 262, 212, 8, 0x1a2230);
        vg_rect(256, 262, (int)(212.0f * vd_clamp(lv_env * 4.0f, 0.0f, 1.0f)), 8, li_live && lv.on ? 0x3fb950 : 0x404850);
    }
    {                                           /* the soft keys' captions */
        static const char *const F[6] = {"SEL", "GLOBAL", "SAVE", "IN", "VOICE", "ROUTE"};
        for (i = 0; i < 6; i++) {
            int hi = i >= 3 && vp_page == VP_IN + i - 3;
            vg_round(4 + i * 79, 286, 75, 30, 4, hi ? 0xff1a8c : 0x1a2230);
            vg_text(F[i], 41 + i * 79, 293, VG_BOLD, hi ? 0x05080d : 0xE6EAF2, VG_CENTER);
        }
    }
}
