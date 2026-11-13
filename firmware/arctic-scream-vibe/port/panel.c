/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* Arctic Scream VIBE's panel: VIBE FM's controls onto Arctic Scream, and its display in the house layout.
 *
 *   K1..K6      the demos A1..A6 (the FM-1's web page's: port/demo.c); HOME names the one playing
 *   F1..F6      SOUND ENV LFO ARP FX SEQ: the FM-1's EDIT, ENV, LFO, ARP, FX, SEQ (pressed again: their next page;
 *               held: their layer on the pads, as on the FM-1's keys)
 *   SHIFT F1..3 SEL, GLOBAL, SAVE (held: their layers on the pads)
 *   SHIFT F4..6 IN (again: SAMPLE), VOICE, ROUTE: sound in, on the right half (encoders 1..4 its four values)
 *   RETURN      the FM-1's HOME (and the right half back to the pads)
 *   HOME        what every control does (vg_controls); the library is the host's (held, SHIFT + HOME)
 *   PLAY REC    PLAY, REC (SAMPLE: REC records the input)
 *   STOP        the sequencer, if it plays; else the track's sound let go (its latched notes, GENERATE); SHIFT: all
 *   < >  MENU   UNDO / REDO; MENU: what every control does (as HOME)
 *   OCT - / +   OCT- / OCT+ (the keys follow: vibe_keys_octave)
 *   enc 1..4    KNOB 1..4 (on IN, VOICE, ROUTE: those four values); A the track into SPACE (SHIFT: DELAY), B SELECT (the tempo, the
 *               steps), DATA PRESETS (the big wheel: the sounds)
 *   keys        notes on the selected track (MIDI, with the DAW's)
 *   pads        nothing held: 1..4 the track (the selected one again: its LATCH), 5..8 slots A..D (recalled), 9..16 DICE's eight moods on the track;
 *               a button held: the FM-1's function keys under it, 1..8 its eight white keys from F2, 9..16 the next
 *               eight (DICE, ACCIDENT) or, with FX, GENERATE's black keys */

enum { VP_FM, VP_IN, VP_VOICE, VP_ROUTE, VP_SAMPLE };   /* the FM-1's pages / sound in's: IN, VOICE, ROUTE, SAMPLE (F4 twice) */
static int vp_page = VP_FM, vp_help;
static uint32_t vp_btn;                         /* the FM-1 buttons the VIBE holds (BTN_ ids) */
static uint32_t vp_fkeys;                       /* the FM-1 keys the pads hold (bit = key) */
static int8_t vp_padkey[16];
#include "demo.c"                               /* the demos on K1..K6 */                    /* a held pad's FM-1 key (-1: a direct function, none) */
static uint32_t vp_stop_seq;                   /* (STOP pressed to stop the sequencer: its let go too) */
static uint32_t vp_shift, vp_f_t = 0x80000000u;   /* SHIFT held; fm1_ms of the last F key */

static const int8_t VP_K[6] = {BTN_EDIT, BTN_ENV, BTN_LFO, BTN_ARP, BTN_FX, BTN_SEQ};
static const int8_t VP_F[3] = {BTN_SEL, BTN_GLO, BTN_SAVE};
static const int8_t VP_ENC[VE_COUNT] = {ENC_KNOB1, ENC_KNOB2, ENC_KNOB3, ENC_KNOB4, ENC_ALGORITHM, ENC_SELECT, ENC_PRESETS};
static float vp_frac[VE_COUNT];                 /* (fractional detents, summed) */
static const uint32_t VP_TRK[4] = {0x22d3ee, 0x3fb950, 0xa78bfa, 0xff1a8c};   /* the tracks' colours (ui.c UI_ACCENTS) */
static const char *const VP_MOOD[8] = {"HUSH", "STILL", "SWELL", "DRIFT", "ECHO", "PULSE", "ARP", "MACHINE"};
static const int VP_PAGE_P[5][4] = {{-1, -1, -1, -1}, {P_IN, P_MODE, P_GAIN, P_DRY}, {P_PITCH, P_RING, P_RING_HZ, P_GROWL},
                                    {P_HOLVUT, P_HOLVUT_AMT, P_SPACE, P_DELAY}, {P_SLOT, P_REC_DRY, -1, -1}};
static const char *const VP_PAGE_N[5] = {"", "IN", "VOICE", "ROUTE", "SAMPLE"};

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
    if (p < 4 && (uint32_t)p == sel) {          /* 1..4: the track; the selected one's pad again: its LATCH on / off */
        set_latch(!trk[sel].latch);
        hd_say(trk[sel].latch ? "LATCH ON" : "LATCH OFF");
    } else if (p < 4)
        sel = (uint32_t)p;
    else if (p < 8) slot_do_recall((uint32_t)p - 4u);   /* 5..8: slots A..D */
    else dice((uint32_t)p - 8u, (uint32_t)(KEY0 + 12 * set.oct));   /* 9..16: DICE */
}

static void vp_value(int i, float d)            /* sound in's page: its value i turned d detents */
{
    int p = VP_PAGE_P[vp_page][i];
    const vibe_param *P;
    float v, st;
    if (p < 0) return;
    P = &PARAMS[p];
    v = vibe_param_get(p);
    st = P->steps ? (P->max - P->min) / (float)(P->steps - 1) : (P->max - P->min) / 100.0f;
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
        if (b >= VB_F1 && b <= VB_F6 && e->down) vp_f_t = fm1_ms;
        if (b >= VB_K1 && b <= VB_K6) { if (e->down) dm_start((uint32_t)(b - VB_K1)), vp_page = VP_FM; }   /* the demos */
        else if (b >= VB_F1 && b <= VB_F6) {    /* F1..F6: SOUND ENV LFO ARP FX SEQ; SHIFT: SEL GLOBAL SAVE IN VOICE
                                                 * ROUTE (the button an F key pressed is let go with it) */
            static int8_t held[6] = {-1, -1, -1, -1, -1, -1};
            int f = b - VB_F1;
            if (!e->down) {
                if (held[f] >= 0) vp_set_btn(held[f], 0), held[f] = -1;
            } else if (!vp_shift) {
                held[f] = VP_K[f], vp_set_btn(VP_K[f], 1);
                if (vp_page != VP_FM) vp_page = VP_FM;
            } else if (f < 3) {
                held[f] = VP_F[f], vp_set_btn(VP_F[f], 1);
            } else if (f == 3)
                vp_page = vp_page == VP_IN ? VP_SAMPLE : vp_page == VP_SAMPLE ? VP_FM : VP_IN;
            else
                vp_page = vp_page == f - 3 + VP_IN ? VP_FM : f - 3 + VP_IN;
        }
        else if (vp_page == VP_SAMPLE && (b == VB_ENTER || b == VB_DATA_PUSH)) { if (e->down) ml_open((uint32_t)vibe_param_get(P_SLOT)); }
        else if (vp_page == VP_SAMPLE && b == VB_RETURN && ml_cur[0]) { if (e->down) ml_up(); }
        else if (vp_page == VP_SAMPLE && b == VB_REC) {   /* SAMPLE: REC records the sound in into the slot (again: kept) */
            if (e->down) {
                if (sl_rec) sl_rec_stop();
                else if (wl_state == WL_IDLE) {
                    if (vibe_param_get(P_IN) < 1.0f) vibe_param_set(P_IN, 1.0f), vb_input_sync();   /* (the input off:
                                                 * IN 1 on, there is something to record) */
                    sl_rec_start((uint32_t)vibe_param_get(P_SLOT)), wl_msg[0] = 0;
                }
            }
        }
        else if (b == VB_RETURN) { vp_set_btn(BTN_HOME, e->down); if (e->down) vp_page = VP_FM; }
        else if (b == VB_PLAY) vp_set_btn(BTN_PLAY, e->down);
        else if (b == VB_STOP) {                /* STOP: the sequencer, if it plays; else the track's sound (its
                                                 * latched notes, its GENERATE) let go; SHIFT + STOP: all four */
            if (e->down && playing) vp_stop_seq = 1, vp_set_btn(BTN_PLAY, 1);
            else if (!e->down && vp_stop_seq) vp_stop_seq = 0, vp_set_btn(BTN_PLAY, 0);
            else if (e->down) {
                uint32_t t;
                dm_cur = dm_shown = -1;         /* (a demo: no longer named) */
                for (t = 0; t < NT; t++)
                    if (vp_shift || t == sel) gen_on &= ~(1u << t), nq_put(NQ_RELEASE, t, 0);
            }
        }
        else if (b == VB_LEFT) { if (e->down) hd_say(undo() ? "NOTHING TO UNDO" : "UNDO"); }
        else if (b == VB_RIGHT) { if (e->down) hd_say(redo() ? "NOTHING TO REDO" : "REDO"); }
        else if (b == VB_MENU) { if (e->down) vp_help = !vp_help; }
        else if (b == VB_REC) vp_set_btn(BTN_REC, e->down);
        else if (b == VB_OCT_DOWN) vp_set_btn(BTN_OCT_DOWN, e->down);
        else if (b == VB_OCT_UP) vp_set_btn(BTN_OCT_UP, e->down);
        else if (b == VB_SHIFT) vp_shift = e->down;
    } else if (e->type == VIBE_EV_PAD && e->id < 16) {
        vp_pad(e->id, e->down);
    } else if (e->type == VIBE_EV_ENCODER && e->id < VE_COUNT) {
        int32_t d;
        if (vp_page != VP_FM && e->id <= VE_4) { vp_value(e->id, e->value); return; }
        if (e->id == VE_A) {                    /* A: the track into SPACE (SHIFT: into DELAY), a wash at hand */
            static float fa;
            volatile int32_t *q = vp_shift ? &trk[sel].dly : &trk[sel].rev;
            int32_t d2;
            char *m;
            fa += e->value * 2.0f;
            d2 = (int32_t)fa, fa -= (float)d2;
            if (!d2) return;
            *q = clamp(*q + d2, 0, 127);
            m = put_s(put_u(put_s(hd_msg, "T"), sel + 1u), vp_shift ? " DELAY " : " SPACE ");
            put_u(m, (uint32_t)(*q * 100 / 127)), put_s(hd_msg + strlen(hd_msg), "%");
            hd_until = fm1_ms + 1500u;
            return;
        }
        if (vp_page == VP_SAMPLE && e->id == VE_DATA) {   /* SAMPLE: DATA through the media's list */
            ml_sel += e->value > 0 ? 1 : -1;
            ml_sel = ml_sel < 0 ? 0 : ml_sel >= ml_n ? (ml_n ? ml_n - 1 : 0) : ml_sel;
            return;
        }
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
    for (p = 0; p < 6; p++) vibe_led_button(VB_K1 + p, 0xa78bfa, dm_shown == p ? 1.0f : 0.15f);   /* (the demo playing) */
}

static const char *const VP_HELP[] = {
    "K1..K6", "the demos A1..A6",
    "F1..F6", "SOUND ENV LFO ARP FX SEQ (again: next page; held: pad layer)",
    "SHIFT F1..F3", "SEL, GLOBAL, SAVE (held: mutes, solos, slots on the pads)",
    "SHIFT F4..F6", "IN (again: SAMPLE), VOICE, ROUTE: sound in",
    "SAMPLE", "REC records the input into the slot; DATA, ENTER load a WAV",
    "ENC 1..4", "the four knobs on the screen",
    "ENC A", "the track into SPACE (SHIFT: DELAY)",
    "ENC B", "SELECT (tempo, steps)",
    "DATA", "PRESETS: the sounds",
    "PADS 1..4", "the track; again: its LATCH on / off",
    "PADS 5..8", "projects A..D (SAVE held: saved there)",
    "PADS 9..16", "DICE: a new part in a mood (PLAY runs it)",
    "STOP", "the sequencer; or the track's sound off (SHIFT: all)",
    "< >", "UNDO / REDO",
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

#include "home.c"                               /* HOME at the display's own size */

static void vp_fm1_full(uint16_t *vfb)          /* the FM-1's screen as it is, as tall as the display (240 -> 320, each
                                                 * pixel 1 or 2 wide and high: the pixel look kept) in the middle; on
                                                 * HOME its night sky goes on into the sides */
{
    static const uint16_t SX[28] = {7, 23, 41, 58, 70, 12, 33, 49, 66, 4, 27, 54, 75, 18, 44, 62, 9, 36, 71, 29, 52, 15,
                                    67, 39, 3, 57, 24, 46};
    uint32_t x, y, i;
    uint16_t k = UI_K;
    for (i = 0; i < VIBE_SCREEN_W * VIBE_SCREEN_H; i++) vfb[i] = k;
    if (page == PG_HOME)                        /* (the sides' stars: the FM-1's greys, now and then one brightens) */
        for (i = 0; i < 56u; i++) {
            uint32_t sx = (i < 28u ? 0u : 400u) + SX[i % 28u], sy = 8u + (i * 97u) % 300u, j;
            uint16_t c = (fm1_ms / 400u + i * 13u) % 23u == 0u ? UI_L : i % 3u ? UI_D : UI_M;
            for (j = 0; j < 9u; j++) vfb[(sy + j / 3u) * VIBE_SCREEN_W + sx + j % 3u] = c;
        }
    for (y = 0; y < VIBE_SCREEN_H; y++) {
        uint16_t *row = vfb + y * VIBE_SCREEN_W + 80u;
        const uint16_t *src = fb + (y * 3u / 4u) * 240u;
        for (x = 0; x < 320u; x++) row[x] = src[x * 3u / 4u];
    }
}

static void vp_draw(uint16_t *vfb)
{
    uint32_t x, y;
    char b[24];
    int i;
    vp_leds();
    if (vp_help) { vg_controls("ARCTIC SCREAM", VP_HELP, sizeof VP_HELP / sizeof VP_HELP[0] / 2); return; }
    vg_clear(0x05080d);
    if (!vb_lit) return;                        /* (SCREEN OFF: dark) */
    if (vp_page == VP_FM && page == PG_HOME) vp_home_full(vfb);   /* HOME: drawn again, the whole display */
    else if (vp_page == VP_FM) vp_fm1_full(vfb);   /* the FM-1's other pages: its screen, full height */
    else {                                      /* sound in's pages: the FM-1's screen left, the page right */
        vg_text("ARCTIC SCREAM", 10, 8, VG_BOLD, 0xE6EAF2, VG_LEFT);
        vg_text(VP_PAGE_N[vp_page], 468, 8, VG_BOLD, 0xff1a8c, VG_RIGHT);
        for (y = 0; y < 240u; y++) {
            uint16_t *row = vfb + (36u + y) * VIBE_SCREEN_W;
            const uint16_t *src = fb + y * 240u;
            for (x = 0; x < 240u; x++) row[x] = src[x];
        }
        for (i = 0; i < 4; i++) {
            int p = VP_PAGE_P[vp_page][i], cy = 40 + i * (vp_page == VP_SAMPLE ? 30 : 52);
            if (p < 0) continue;
            vg_text(PARAMS[p].name, 252, cy, VG_SMALL, 0x8A93A6, VG_LEFT);
            vp_text_value(p, b);
            vg_text(b, 470, cy, VG_MED, 0xE6EAF2, VG_RIGHT);
            if (vp_page != VP_SAMPLE)
                vg_rect(252, cy + 34, (int)(218.0f * (vibe_param_get(p) - PARAMS[p].min) / (PARAMS[p].max - PARAMS[p].min)), 3, 0xff1a8c);
        }
        if (vp_page == VP_SAMPLE) {             /* the media's list (DATA, ENTER), what happens */
            int n = ml_n;
            if (ml_sel < ml_top) ml_top = ml_sel;
            if (ml_sel >= ml_top + 5) ml_top = ml_sel - 4;
            vg_text(ml_none ? "NO MEDIA HERE" : ml_cur[0] ? ml_cur : "SOURCES", 252, 104, VG_SMALL, 0x5B6577, VG_LEFT);
            for (i = ml_top; i < n && i < ml_top + 5; i++) {
                int yy = 124 + (i - ml_top) * 24;
                const char *l = ml_lines[i];
                if (i == ml_sel) vg_round(248, yy - 2, 226, 22, 4, 0x24314A);
                vg_text(ml_cur[0] ? ml_field(l, 2) : ml_field(l, 2)[0] ? ml_field(l, 2) : l, 254, yy, VG_SMALL,
                        ml_cur[0] && l[0] == 'd' ? 0x22d3ee : 0xE6EAF2, VG_LEFT);
            }
            if (sl_rec) {
                vg_round(248, 250, 226, 24, 4, 0xff1a8c);
                vg_text("REC", 256, 254, VG_BOLD, 0x05080d, VG_LEFT);
                vg_ftoa((float)sl_n / 22050.0f, 1, b), vp_cat(b, sl_n >= SL_MAX ? " S  FULL" : " S");
                vg_text(b, 466, 254, VG_BOLD, 0x05080d, VG_RIGHT);
            } else
                vg_text(wl_state != WL_IDLE ? "LOADING" : wl_msg[0] ? wl_msg : "REC: RECORD  ENTER: LOAD", 252, 254,
                        VG_SMALL, wl_err ? 0xff1a8c : 0x8A93A6, VG_LEFT);
        } else {
            vg_rect(252, 262, 218, 6, 0x1a2230);
            vg_rect(252, 262, (int)(218.0f * vd_clamp(lv_env * 4.0f, 0.0f, 1.0f)), 6, li_live && lv.on ? 0x3fb950 : 0x404850);
        }
    }
    if (vp_shift || fm1_ms - vp_f_t < 2000u) {  /* the soft keys' captions: only while SHIFT is held, or a moment after
                                                 * an F key (over the bottom line) */
        static const char *const CAP_S[6] = {"SEL", "GLOBAL", "SAVE", "IN", "VOICE", "ROUTE"};
        static const char *const CAP_P[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
        const char *const *FC = vp_shift ? CAP_S : CAP_P;
        vg_begin(vfb);
        vg_rect(0, 0, 480, 24, 0x161f2a);       /* (over the header, a moment: it covers nothing that plays) */
        for (i = 0; i < 6; i++) {
            int hi = vp_shift ? i >= 3 && (vp_page == VP_IN + i - 3 || (i == 3 && vp_page == VP_SAMPLE))
                              : vp_page == VP_FM && PAGES[page].btn == VP_K[i];
            vg_text(vp_shift && i == 3 && vp_page == VP_SAMPLE ? "SAMPLE" : FC[i], 40 + i * 80, 4, VG_SMALL, hi ? 0xff1a8c : 0xC0C6D0,
                    VG_CENTER);
        }
    }
}
