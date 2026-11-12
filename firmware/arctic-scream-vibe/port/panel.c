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

enum { VP_FM, VP_IN, VP_VOICE, VP_ROUTE, VP_SAMPLE };   /* the FM-1's pages / sound in's: IN, VOICE, ROUTE, SAMPLE (F4 twice) */
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
    if (p < 4) sel = (uint32_t)p;               /* 1..4: the track */
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
        if (b >= VB_K1 && b <= VB_K6) vp_set_btn(VP_K[b - VB_K1], e->down);
        else if (b >= VB_F1 && b <= VB_F3) vp_set_btn(VP_F[b - VB_F1], e->down);
        else if (b == VB_F4) { if (e->down) vp_page = vp_page == VP_IN ? VP_SAMPLE : vp_page == VP_SAMPLE ? VP_FM : VP_IN; }
        else if (b >= VB_F5 && b <= VB_F6) { if (e->down) vp_page = vp_page == b - VB_F4 + VP_IN ? VP_FM : b - VB_F4 + VP_IN; }
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
    for (p = 0; p < 6; p++) vibe_led_button(VB_K1 + p, VP_TRK[sel & 3u], PAGES[page].btn == VP_K[p] ? 1.0f : 0.15f);
    for (p = 0; p < 3; p++) vibe_led_button(VB_F4 + p, 0xff1a8c, vp_page == VP_IN + p ? 1.0f : 0.15f);
}

static const char *const VP_HELP[] = {
    "K1..K6", "SOUND ENV LFO ARP FX SEQ (again: next page; held: its pad layer)",
    "F1 F2 F3", "SEL, GLOBAL, SAVE (held: mutes, solos, slots on the pads)",
    "F4 F5 F6", "IN (again: SAMPLE), VOICE, ROUTE: sound in on encoders 1..4",
    "SAMPLE", "REC records the input into the slot; DATA, ENTER load a WAV",
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

static void vp_knobs(int y)                     /* the page's four knobs, small, in one row (encoders 1..4) */
{
    char b[16];
    int k;
    for (k = 0; k < 4; k++) {
        volatile int32_t *q = knob_param(page, (uint32_t)k);
        int x = 6 + k * 118, mx = knob_max(page, (uint32_t)k), mn = knob_min(page, (uint32_t)k);
        const char *n = knob_name(page, (uint32_t)k);
        if (!n || !*n) continue;
        vg_text(n, x, y, VG_SMALL, 0x8A93A6, VG_LEFT);
        knob_text(page, (uint32_t)k, b);
        vg_text(b, x + 112, y, VG_SMALL, 0xE6EAF2, VG_RIGHT);
        vg_rect(x, y + 17, 112, 2, 0x1a2230);
        if (q && mx > mn) vg_rect(x, y + 17, (int)(112 * (vd_clamp((float)(*q - mn) / (float)(mx - mn), 0.0f, 1.0f))), 2, VP_TRK[sel & 3u]);
    }
}

static void vp_home(void)                       /* HOME, the whole display: the four tracks, each its wave as it sounds
                                                 * (ui.c's wv_d, the FM-1's HOME picture's), over the stars */
{
    static const uint16_t SX[24] = {31, 77, 140, 188, 233, 291, 347, 402, 455, 18, 109, 167, 214, 262, 318, 371, 429, 64,
                                    122, 197, 276, 333, 389, 446};
    char b[24];
    uint32_t t, i;
    for (i = 0; i < 24u; i++) {                 /* (the sky: a few stars, brighter as it gets louder) */
        uint32_t c = (fm1_ms / 700u + i * 7u) % 9u == 0u ? 0x8A93A6 : 0x2a3344;
        vg_rect(SX[i], 32 + (int)((i * 53u) % 236u), 2, 2, c);
    }
    for (t = 0; t < NT; t++) {
        int c = 62 + (int)t * 60, on = trk[t].level > 0 && !(trk_quiet >> t & 1u);
        uint32_t col = on ? VP_TRK[t] : 0x404850, dim = vg_mix(col, 0x05080d, 0.75f);
        char nm[24];
        nm[0] = 0;
        if (trk_sname(t, nm) == nm) strcpy(nm, ENGINES[trk[t].eng]->name);
        if (t == sel) vg_rect(0, c - 26, 3, 52, col);
        b[0] = 'T', b[1] = (char)('1' + t), b[2] = 0;
        vg_text(b, 10, c - 26, VG_BOLD, t == sel ? 0xE6EAF2 : 0x8A93A6, VG_LEFT);
        vg_text(nm, 32, c - 26, VG_SMALL, t == sel ? col : 0x5B6577, VG_LEFT);
        {                                       /* the wave: across the whole width, a glow under a line */
            float wx[SC_N], wy[SC_N];
            for (i = 0; i < SC_N; i++) {
                int32_t v = wv_d[t][i];
                wx[i] = 10.0f + (float)i * (460.0f / (float)(SC_N - 1u));
                wy[i] = (float)c + 4.0f - 24.0f * (float)v / (float)((v < 0 ? -v : v) + 500);
            }
            for (i = 1; i < SC_N; i++) vg_line(wx[i - 1], wy[i - 1], wx[i], wy[i], 6.0f, dim);
            for (i = 1; i < SC_N; i++) vg_line(wx[i - 1], wy[i - 1], wx[i], wy[i], t == sel ? 2.0f : 1.5f, col);
        }
    }
    vg_text(playing ? "PLAY" : "", 468, 8, VG_BOLD, 0x3fb950, VG_RIGHT);
    vg_itoa((int)seq_bpm, b);
    vg_text(b, playing ? 420 : 468, 8, VG_BOLD, 0x8A93A6, VG_RIGHT);
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
    vg_text("ARCTIC SCREAM", 10, 8, VG_BOLD, 0xE6EAF2, VG_LEFT);   /* the header */
    if (vp_page == VP_FM && page == PG_HOME) {  /* HOME: the tracks, full width; the knobs small under them */
        vp_home();
        vp_knobs(272);
    } else if (vp_page == VP_FM) {              /* the FM-1's other pages: its screen, as it is, in the middle */
        vg_text(PAGES[page].name, 468, 8, VG_BOLD, VP_TRK[sel & 3u], VG_RIGHT);
        for (y = 0; y < 240u; y++) {
            uint16_t *row = vfb + (36u + y) * VIBE_SCREEN_W + 120u;
            const uint16_t *src = fb + y * 240u;
            for (x = 0; x < 240u; x++) row[x] = src[x];
        }
    } else {                                    /* sound in's pages: the FM-1's screen left, the page right */
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
    {                                           /* the soft keys' captions: one thin line at the bottom */
        static const char *const F[6] = {"SEL", "GLOBAL", "SAVE", "IN", "VOICE", "ROUTE"};
        vg_rect(0, 303, 480, 1, 0x141b26);
        for (i = 0; i < 6; i++) {
            int hi = i >= 3 && (vp_page == VP_IN + i - 3 || (i == 3 && vp_page == VP_SAMPLE));
            vg_text(i == 3 && vp_page == VP_SAMPLE ? "SAMPLE" : F[i], 40 + i * 80, 305, VG_SMALL, hi ? 0xff1a8c : 0x4a5466,
                    VG_CENTER);
        }
    }
}
